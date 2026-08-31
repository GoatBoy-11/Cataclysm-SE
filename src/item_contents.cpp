#include "item_contents.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include <set>
#include <string>
#include <vector>

#include "character.h"
#include "enums.h"
#include "handle_liquid.h"
#include "item.h"
#include "item_category.h"
#include "itype.h"
#include "locations.h"
#include "map.h"
#include "output.h"
#include "string_input_popup.h"
#include "translations.h"
#include "type_id.h"
#include "ui.h"
#include "units_utility.h"

struct tripoint;

item_contents::item_contents( item *container ) : owner( container )
{
    // The pocket_data pointers stay valid because itype::pockets is filled
    // during finalization and never mutated afterwards.
    if( container != nullptr && container->type != nullptr &&
        !container->type->pockets.empty() ) {
        for( const pocket_data &data : container->type->pockets ) {
            pockets.emplace_back( container, &data );
        }
        return;
    }
    // Items with no storage at all still get one pocket, so every fan-out
    // method has something to iterate.
    default_pocket_data.type = pocket_type::CONTAINER;
    default_pocket_data.max_contains_volume = 0_ml;
    pockets.emplace_back( container, &default_pocket_data );
}

/** used to aid migration */
item_contents::item_contents( item *container,
                              std::vector<detached_ptr<item>> &items ) : item_contents( container )
{
    for( detached_ptr<item> &it : items ) {
        pockets.front().insert( std::move( it ) );
    }
    items.clear();
}

item_contents::~item_contents() = default;

bool item_contents::empty() const
{
    for( const item_pocket &pocket : pockets ) {
        if( !pocket.empty() ) {
            return false;
        }
    }
    return true;
}

const item_pocket *item_contents::pocket_containing( const item &it ) const
{
    for( const item_pocket &pocket : pockets ) {
        for( const item * const candidate : pocket.all_items_top() ) {
            if( candidate == &it ) {
                return &pocket;
            }
        }
    }
    return nullptr;
}

bool item_contents::settings_edited() const
{
    for( const item_pocket &pocket : pockets ) {
        if( !pocket.get_settings().is_null() ) {
            return true;
        }
    }
    return false;
}

auto item_contents::has_processing_items() const -> bool
{
    update_processing_cache();
    return !cached_processing_items.empty();
}

auto item_contents::processing_items() const -> const std::vector<item *> & // *NOPAD*
{
    update_processing_cache();
    return cached_processing_items;
}

auto item_contents::invalidate_processing_cache() const -> void
{
    processing_cache_dirty = true;
    // Every content mutation reaches here, directly or through
    // item::invalidate_processing_cache_upwards(), so this is also the right
    // place to retire the concatenated all_items_top() cache.
    all_items_cache_dirty = true;
}

auto item_contents::update_processing_cache() const -> void
{
    if( !processing_cache_dirty ) {
        return;
    }

    cached_processing_items.clear();
    for( const item_pocket &pocket : pockets ) {
        for( item * const &contained_item : pocket.all_items_top() ) {
            if( contained_item->needs_processing() ) {
                cached_processing_items.push_back( contained_item );
            }
        }
    }
    processing_cache_dirty = false;
}

item_pocket *item_contents::best_pocket( const item &it, const bool ignore_settings )
{
    // Classic mode: first pocket that will take it, no ranking, and player
    // organisation is ignored along with the rest of the pocket rules. Combined
    // with can_contain()'s relaxed checks this reproduces pre-pocket inventory
    // behaviour without changing what is stored on disk.
    if( pockets_are_classic() ) {
        for( item_pocket &pocket : pockets ) {
            if( pocket.can_contain( it ).success() ) {
                return &pocket;
            }
        }
        return nullptr;
    }

    const bool consider_settings = !ignore_settings;

    // Ranks a pocket for this item, higher being better. Follows the order of
    // CDDA's better_pocket(): player priority beats everything, then a pocket
    // the player named this item for, then a pocket the *data* named it for.
    const auto rank_of = [&it, consider_settings]( const item_pocket & pocket ) {
        const pocket_favorite_settings &settings = pocket.get_settings();
        const bool whitelisted = consider_settings &&
                                 ( !settings.get_item_whitelist().empty() ||
                                   !settings.get_category_whitelist().empty() );
        // A pocket that names what may go in it is the item's proper home; a
        // general-purpose pocket only happens to have room. Rank the former
        // higher so a magazine reaches the magazine well rather than the
        // roomy cargo pocket on the same item.
        const pocket_data &def = pocket.definition();
        const bool restricted = !def.ammo_restriction.empty() ||
                                !def.item_restriction.empty() ||
                                !def.mod_restriction.empty();
        return std::make_pair( whitelisted, restricted );
    };

    item_pocket *best = nullptr;
    int best_priority = 0;
    std::pair<bool, bool> best_rank{ false, false };

    for( item_pocket &pocket : pockets ) {
        // A disabled pocket rejects everything, so accepts_item() covers it.
        if( consider_settings && !pocket.get_settings().accepts_item( it ) ) {
            continue;
        }
        if( !pocket.can_contain( it ).success() ) {
            continue;
        }

        const int priority = consider_settings ? pocket.get_settings().priority() : 0;
        const std::pair<bool, bool> rank = rank_of( pocket );

        if( best == nullptr || priority > best_priority ||
            ( priority == best_priority &&
              ( rank > best_rank ||
                ( rank == best_rank &&
                  pocket.remaining_volume() < best->remaining_volume() ) ) ) ) {
            best = &pocket;
            best_priority = priority;
            best_rank = rank;
        }
    }
    return best;
}

ret_val<bool> item_contents::insert_item_impl( detached_ptr<item> &&it, const bool force )
{
    bool stacked = false;
    if( it->count_by_charges() ) {
        for( item_pocket &pocket : pockets ) {
            for( item *check : pocket.all_items_top() ) {
                // merge_charges leaves the pointer intact on failure.
                // NOLINTNEXTLINE(bugprone-use-after-move)
                if( check->merge_charges( std::move( it ) ) ) {
                    stacked = true;
                    break;
                }
            }
            if( stacked ) {
                break;
            }
        }
    }

    if( !stacked ) {
        // NOLINTNEXTLINE(bugprone-use-after-move)
        item_pocket *chosen = best_pocket( *it );
        if( chosen == nullptr ) {
            // Every insertion that no pocket accepts is recorded, whether it is
            // then refused or forced through; the audit report stays the ledger
            // of what enforcement rejects.
            // NOLINTNEXTLINE(bugprone-use-after-move)
            record_pocket_audit_miss( owner, *it );
            if( !force ) {
                // The item is deliberately NOT consumed: the caller keeps it and
                // must decide where it goes instead.
                return ret_val<bool>::make_failure( false, _( "does not fit in any pocket" ) );
            }
        }
        item_pocket *target = chosen != nullptr ? chosen : &pockets.front();
        // NOLINTNEXTLINE(bugprone-use-after-move)
        target->insert( std::move( it ) );
    }

    if( owner != nullptr ) {
        owner->invalidate_processing_cache_upwards();
    } else {
        invalidate_processing_cache();
    }
    return ret_val<bool>::make_success();
}

ret_val<bool> item_contents::insert_item( detached_ptr<item> &&it )
{
    return insert_item_impl( std::move( it ), false );
}

void item_contents::insert_item_forced( detached_ptr<item> &&it )
{
    // Copy construction, save migration and location reattachment must never
    // lose an item; they land it in pocket 0 when nothing better accepts it.
    insert_item_impl( std::move( it ), true );
}

size_t item_contents::num_item_stacks() const
{
    size_t total = 0;
    for( const item_pocket &pocket : pockets ) {
        total += pocket.all_items_top().size();
    }
    return total;
}

bool item_contents::spill_contents( const tripoint_bub_ms &pos )
{
    for( detached_ptr<item> &it : clear_items() ) {
        get_map().add_item_or_charges( pos, std::move( it ) );
    }
    return true;
}

void item_contents::handle_liquid_or_spill( Character &guy )
{
    const bool had_items = !empty();
    for( item_pocket &pocket : pockets ) {
        location_vector<item> &items = pocket.get_contents();
        for( auto iter = items.begin(); iter != items.end(); ) {
            if( ( *iter )->made_of( LIQUID ) ) {
                detached_ptr<item> det;
                iter = items.erase( iter, &det );
                liquid_handler::handle_all_liquid( std::move( det ), 1 );
            } else {
                detached_ptr<item> det;
                iter = items.erase( iter, &det );
                guy.i_add_or_drop( std::move( det ) );
            }
        }
    }
    if( had_items ) {
        if( owner != nullptr ) {
            owner->invalidate_processing_cache_upwards();
        } else {
            invalidate_processing_cache();
        }
    }
}

void item_contents::casings_handle( const std::function < detached_ptr<item>
                                    ( detached_ptr<item> && ) > &func )
{
    static const flag_id json_flag_CASING( "CASING" );
    auto changed = false;
    for( item_pocket &pocket : pockets ) {
        pocket.get_contents().remove_with( [&func, &changed]( detached_ptr<item> &&it ) {
            if( it->has_flag( json_flag_CASING ) ) {
                changed = true;
                it->unset_flag( json_flag_CASING );
                it = func( std::move( it ) );
                if( it ) {
                    it->set_flag( json_flag_CASING );
                }
            }
            return std::move( it );
        } );
    }
    if( changed ) {
        if( owner != nullptr ) {
            owner->invalidate_processing_cache_upwards();
        } else {
            invalidate_processing_cache();
        }
    }
}

std::vector<detached_ptr<item>> item_contents::clear_items()
{
    std::vector<detached_ptr<item>> ret;
    for( item_pocket &pocket : pockets ) {
        for( detached_ptr<item> &it : pocket.clear() ) {
            ret.push_back( std::move( it ) );
        }
    }
    if( owner != nullptr ) {
        owner->invalidate_processing_cache_upwards();
    } else {
        invalidate_processing_cache();
    }
    return ret;
}

void item_contents::on_destroy()
{
    for( item_pocket &pocket : pockets ) {
        pocket.on_destroy();
    }
}

void item_contents::set_item_defaults()
{
    /* For Items with a magazine or battery in its contents */
    for( item_pocket &pocket : pockets ) {
        for( item * const &contained_item : pocket.all_items_top() ) {
            /* for guns and other items defined to have a magazine but don't use "ammo" */
            if( contained_item->is_magazine() ) {
                contained_item->ammo_set(
                    contained_item->ammo_default(), contained_item->ammo_capacity() / 2
                );
            } else { //Contents are batteries or food
                contained_item->charges = contained_item->typeId()->charges_default();
            }
        }
    }
}

void item_contents::migrate_item( item &obj, const std::set<itype_id> &migrations )
{
    for( const itype_id &c : migrations ) {
        bool found = false;
        for( const item_pocket &pocket : pockets ) {
            if( std::ranges::any_of( pocket.all_items_top(), [&]( const item * const & e ) {
            return e->typeId() == c;
            } ) ) {
                found = true;
                break;
            }
        }
        if( !found ) {
            obj.put_in_expected( item::spawn( c, obj.birthday() ) );
        }
    }
}

bool item_contents::has_any_with( const std::function<bool( const item &it )> &filter ) const
{
    for( const item_pocket &pocket : pockets ) {
        if( std::ranges::any_of( pocket.all_items_top(),
        [&filter]( const item * const & it ) -> bool{ return filter( *it );} ) ) {
            return true;
        }
    }
    return false;
}

bool item_contents::stacks_with( const item_contents &rhs ) const
{
    // lhs and rhs are distinct objects, so their all_items_top() caches cannot
    // alias each other.
    const std::vector<item *> &lhs_items = all_items_top();
    const std::vector<item *> &rhs_items = rhs.all_items_top();
    return std::equal( lhs_items.begin(), lhs_items.end(), rhs_items.begin(),
    []( const item * const & a, const item * const & b ) {
        return a->charges == b->charges && a->stacks_with( *b );
    } );
}

item *item_contents::get_item_with( const std::function<bool( const item &it )> &filter )
{
    for( item_pocket &pocket : pockets ) {
        for( item * const &it : pocket.all_items_top() ) {
            if( filter( *it ) ) {
                return it;
            }
        }
    }
    return nullptr;
}

const std::vector<item *> &item_contents::all_items_top() const
{
    // The single-pocket case returns the pocket's own vector, so the reference
    // stays valid exactly as long as it did before pockets existed. Only the
    // multi-pocket path needs the concatenating cache.
    if( pockets.size() == 1 ) {
        return pockets.front().all_items_top();
    }
    // Only rebuild when the contents actually changed. Rebuilding on every call
    // would invalidate a reference a caller is still iterating, which is easy to
    // hit once ordinary items such as guns have more than one pocket.
    if( all_items_cache_dirty ) {
        cached_all_items_top.clear();
        for( const item_pocket &pocket : pockets ) {
            const std::vector<item *> &top = pocket.all_items_top();
            cached_all_items_top.insert( cached_all_items_top.end(), top.begin(), top.end() );
        }
        all_items_cache_dirty = false;
    }
    return cached_all_items_top;
}

detached_ptr<item> item_contents::remove_top( item *it )
{
    for( item_pocket &pocket : pockets ) {
        detached_ptr<item> removed = pocket.remove( it );
        if( removed ) {
            if( owner != nullptr ) {
                owner->invalidate_processing_cache_upwards();
            } else {
                invalidate_processing_cache();
            }
            return removed;
        }
    }
    return detached_ptr<item>();
}

location_vector<item>::iterator item_contents::remove_top( location_vector<item>::iterator &it,
        detached_ptr<item> *removed )
{
    // The iterator carries no publicly readable pocket identity, so this only
    // supports the single pocket phase 1 guarantees. It has no callers today.
    const auto ret = pockets.front().get_contents().erase( it, removed );
    if( owner != nullptr ) {
        owner->invalidate_processing_cache_upwards();
    } else {
        invalidate_processing_cache();
    }
    return ret;
}

std::vector<item *> item_contents::all_items_ptr()
{
    std::vector<item *> ret;
    for( item_pocket &pocket : pockets ) {
        for( item * const &it : pocket.all_items_top() ) {
            ret.push_back( it );
            std::vector<item *> inside = it->contents.all_items_ptr();
            //TODO!:check
            ret.insert( ret.end(), inside.begin(), inside.end() );
        }
    }
    return ret;
}

std::vector<const item *> item_contents::all_items_ptr() const
{
    std::vector<const item *> ret;
    for( const item_pocket &pocket : pockets ) {
        for( const item * const &it : pocket.all_items_top() ) {
            ret.push_back( it );
            std::vector<const item *> inside = it->contents.all_items_ptr();
            ret.insert( ret.end(), inside.begin(), inside.end() );
        }
    }
    return ret;
}

std::vector<item *> item_contents::gunmods()
{
    std::vector<item *> res;
    for( item_pocket &pocket : pockets ) {
        for( item * const &e : pocket.all_items_top() ) {
            if( e->is_gunmod() ) {
                res.push_back( e );
            }
        }
    }
    return res;
}

std::vector<const item *> item_contents::gunmods() const
{
    std::vector<const item *> res;
    for( const item_pocket &pocket : pockets ) {
        for( const item * const &e : pocket.all_items_top() ) {
            if( e->is_gunmod() ) {
                res.push_back( e );
            }
        }
    }
    return res;
}

item &item_contents::front()
{
    for( item_pocket &pocket : pockets ) {
        if( !pocket.empty() ) {
            return *pocket.all_items_top().front();
        }
    }
    // Being empty is a caller error, exactly as it was when this dereferenced
    // an empty location_vector.
    return *pockets.front().all_items_top().front();
}

const item &item_contents::front() const
{
    for( const item_pocket &pocket : pockets ) {
        if( !pocket.empty() ) {
            return *pocket.all_items_top().front();
        }
    }
    return *pockets.front().all_items_top().front();
}

item &item_contents::back()
{
    for( auto iter = pockets.rbegin(); iter != pockets.rend(); ++iter ) {
        if( !iter->empty() ) {
            return *iter->all_items_top().back();
        }
    }
    return *pockets.front().all_items_top().back();
}

const item &item_contents::back() const
{
    for( auto iter = pockets.rbegin(); iter != pockets.rend(); ++iter ) {
        if( !iter->empty() ) {
            return *iter->all_items_top().back();
        }
    }
    return *pockets.front().all_items_top().back();
}

units::volume item_contents::item_size_modifier() const
{
    // Rigidity is still decided per item by item::volume(); gating this on
    // pocket_data::rigid belongs with the phase that authors real pockets.
    units::volume ret = 0_ml;
    for( const item_pocket &pocket : pockets ) {
        ret += pocket.contents_volume();
    }
    return ret;
}

units::mass item_contents::item_weight_modifier() const
{
    units::mass ret = 0_gram;
    for( const item_pocket &pocket : pockets ) {
        ret += pocket.contents_weight();
    }
    return ret;
}

int item_contents::best_quality( const quality_id &id ) const
{
    int ret = INT_MIN;
    for( const item_pocket &pocket : pockets ) {
        for( const item * const &it : pocket.all_items_top() ) {
            ret = std::max( ret, it->get_quality( id ) );
        }
    }
    return ret;
}

void item_contents::remove_top_items_with( const std::function < detached_ptr<item>
        ( detached_ptr<item> && ) >
        &filter )
{
    remove_items_with( [&filter]( detached_ptr<item> &&e ) {
        e = filter( std::move( e ) );
        return VisitResponse::SKIP;
    } );
}

// ---------------------------------------------------------------------------
// The pocket organization menu
// ---------------------------------------------------------------------------

namespace
{

std::string describe_pocket( const item_pocket &pocket, const int number )
{
    const pocket_favorite_settings &settings = pocket.get_settings();
    std::string line = string_format( _( "Pocket %d: %s / %s" ), number,
                                      format_volume( pocket.contents_volume() ),
                                      format_volume( pocket.definition().max_contains_volume ) );
    line += string_format( vgettext( ", %d item", ", %d items",
                                     pocket.all_items_top().size() ),
                           pocket.all_items_top().size() );
    if( settings.priority() != 0 ) {
        line += string_format( _( ", priority %d" ), settings.priority() );
    }
    const size_t listed = settings.get_item_whitelist().size() +
                          settings.get_category_whitelist().size();
    const size_t barred = settings.get_item_blacklist().size() +
                          settings.get_category_blacklist().size();
    if( listed > 0 ) {
        line += string_format( _( ", %d allowed" ), static_cast<int>( listed ) );
    }
    if( barred > 0 ) {
        line += string_format( _( ", %d barred" ), static_cast<int>( barred ) );
    }
    if( settings.is_disabled() ) {
        line += _( ", disabled" );
    }
    return line;
}

/** none -> whitelisted -> blacklisted -> none, so one key drives the whole cycle. */
void cycle_item_filter( pocket_favorite_settings &settings, const itype_id &id )
{
    if( settings.get_item_whitelist().count( id ) ) {
        settings.blacklist_item( id );
    } else if( settings.get_item_blacklist().count( id ) ) {
        settings.clear_item( id );
    } else {
        settings.whitelist_item( id );
    }
}

void cycle_category_filter( pocket_favorite_settings &settings, const item_category_id &id )
{
    if( settings.get_category_whitelist().count( id ) ) {
        settings.blacklist_category( id );
    } else if( settings.get_category_blacklist().count( id ) ) {
        settings.clear_category( id );
    } else {
        settings.whitelist_category( id );
    }
}

std::string item_filter_state( const pocket_favorite_settings &settings, const itype_id &id )
{
    if( settings.get_item_whitelist().count( id ) ) {
        return _( "allowed" );
    }
    if( settings.get_item_blacklist().count( id ) ) {
        return _( "barred" );
    }
    return _( "no rule" );
}

std::string category_filter_state( const pocket_favorite_settings &settings,
                                   const item_category_id &id )
{
    if( settings.get_category_whitelist().count( id ) ) {
        return _( "allowed" );
    }
    if( settings.get_category_blacklist().count( id ) ) {
        return _( "barred" );
    }
    return _( "no rule" );
}

/**
 * Every item type the player could plausibly mean: what the container holds now,
 * plus whatever the pocket already names. Typing an item id would be exact but
 * unusable, and offering every itype in the game is worse.
 */
std::vector<itype_id> candidate_items( const item_contents &contents,
                                       const pocket_favorite_settings &settings )
{
    std::set<itype_id> ids;
    for( const item_pocket &pocket : contents.get_pockets() ) {
        for( const item * const it : pocket.all_items_top() ) {
            ids.insert( it->typeId() );
        }
    }
    ids.insert( settings.get_item_whitelist().begin(), settings.get_item_whitelist().end() );
    ids.insert( settings.get_item_blacklist().begin(), settings.get_item_blacklist().end() );
    return std::vector<itype_id>( ids.begin(), ids.end() );
}

std::vector<item_category_id> candidate_categories( const item_contents &contents,
        const pocket_favorite_settings &settings )
{
    std::set<item_category_id> ids;
    for( const item_pocket &pocket : contents.get_pockets() ) {
        for( const item * const it : pocket.all_items_top() ) {
            ids.insert( it->get_category().get_id() );
        }
    }
    ids.insert( settings.get_category_whitelist().begin(), settings.get_category_whitelist().end() );
    ids.insert( settings.get_category_blacklist().begin(), settings.get_category_blacklist().end() );
    return std::vector<item_category_id>( ids.begin(), ids.end() );
}

void item_filter_menu( const item_contents &contents, pocket_favorite_settings &settings )
{
    while( true ) {
        const std::vector<itype_id> ids = candidate_items( contents, settings );
        if( ids.empty() ) {
            popup( _( "Nothing to filter yet: put something in this container first." ) );
            return;
        }
        uilist menu;
        menu.title = _( "Item rules - select to cycle allowed / barred / no rule" );
        for( size_t i = 0; i < ids.size(); i++ ) {
            menu.addentry( static_cast<int>( i ), true, MENU_AUTOASSIGN, "%s [%s]",
                           item::nname( ids[i] ), item_filter_state( settings, ids[i] ) );
        }
        menu.query();
        if( menu.ret < 0 || static_cast<size_t>( menu.ret ) >= ids.size() ) {
            return;
        }
        cycle_item_filter( settings, ids[menu.ret] );
    }
}

void category_filter_menu( const item_contents &contents, pocket_favorite_settings &settings )
{
    while( true ) {
        const std::vector<item_category_id> ids = candidate_categories( contents, settings );
        if( ids.empty() ) {
            popup( _( "Nothing to filter yet: put something in this container first." ) );
            return;
        }
        uilist menu;
        menu.title = _( "Category rules - select to cycle allowed / barred / no rule" );
        for( size_t i = 0; i < ids.size(); i++ ) {
            menu.addentry( static_cast<int>( i ), true, MENU_AUTOASSIGN, "%s [%s]",
                           ids[i].is_valid() ? ids[i]->name() : ids[i].str(),
                           category_filter_state( settings, ids[i] ) );
        }
        menu.query();
        if( menu.ret < 0 || static_cast<size_t>( menu.ret ) >= ids.size() ) {
            return;
        }
        cycle_category_filter( settings, ids[menu.ret] );
    }
}

void save_preset_from( const pocket_favorite_settings &settings )
{
    if( settings.is_null() ) {
        popup( _( "There are no rules on this pocket to save." ) );
        return;
    }
    string_input_popup input;
    const std::string name = input
                             .title( _( "Name this preset" ) )
                             .width( 30 )
                             .text( settings.get_preset_name().value_or( std::string() ) )
                             .query_string();
    if( name.empty() ) {
        return;
    }
    if( pocket_presets::find( name ) != nullptr &&
        !query_yn( _( "Replace the preset named %s?" ), name ) ) {
        return;
    }
    // Store a copy under that name, so later edits to this pocket do not
    // quietly rewrite a preset the player saved and moved on from.
    pocket_favorite_settings preset = settings;
    preset.set_preset_name( name );
    pocket_presets::add( preset );
}

void apply_preset_to( pocket_favorite_settings &settings )
{
    const std::vector<pocket_favorite_settings> &presets = pocket_presets::all();
    if( presets.empty() ) {
        popup( _( "No presets saved yet.  Set a pocket up, then save its rules." ) );
        return;
    }
    uilist menu;
    menu.title = _( "Apply which preset?" );
    for( size_t i = 0; i < presets.size(); i++ ) {
        menu.addentry( static_cast<int>( i ), true, MENU_AUTOASSIGN, "%s",
                       presets[i].get_preset_name().value_or( _( "unnamed" ) ) );
    }
    // Deleting is rare enough to live behind its own entry rather than a key.
    menu.addentry( static_cast<int>( presets.size() ), true, 'd', _( "Delete a preset" ) );
    menu.query();
    if( menu.ret < 0 ) {
        return;
    }
    if( static_cast<size_t>( menu.ret ) == presets.size() ) {
        uilist which;
        which.title = _( "Delete which preset?" );
        for( size_t i = 0; i < presets.size(); i++ ) {
            which.addentry( static_cast<int>( i ), true, MENU_AUTOASSIGN, "%s",
                            presets[i].get_preset_name().value_or( _( "unnamed" ) ) );
        }
        which.query();
        if( which.ret >= 0 && static_cast<size_t>( which.ret ) < presets.size() ) {
            const std::string name = presets[which.ret].get_preset_name().value_or( std::string() );
            if( !name.empty() ) {
                pocket_presets::remove( name );
            }
        }
        return;
    }
    settings = presets[menu.ret];
}

void one_pocket_menu( const item_contents &contents, item_pocket &pocket, const int number )
{
    pocket_favorite_settings &settings = pocket.get_settings();
    while( true ) {
        uilist menu;
        menu.title = describe_pocket( pocket, number );
        menu.addentry( 0, true, 'p', string_format( _( "Set priority (now %d)" ),
                       settings.priority() ) );
        menu.addentry( 1, true, 'i', _( "Item rules" ) );
        menu.addentry( 2, true, 'c', _( "Category rules" ) );
        menu.addentry( 3, true, 'd', string_format( _( "Auto-insert: %s" ),
                       settings.is_disabled() ? _( "off" ) : _( "on" ) ) );
        menu.addentry( 4, true, 'l', string_format( _( "Show contents: %s" ),
                       settings.is_collapsed() ? _( "collapsed" ) : _( "expanded" ) ) );
        menu.addentry( 5, true, 'u', string_format( _( "Unload with the rest: %s" ),
                       settings.is_unloadable() ? _( "yes" ) : _( "no" ) ) );
        menu.addentry( 6, true, 's', _( "Save these rules as a preset" ) );
        menu.addentry( 7, true, 'a', _( "Apply a preset" ) );
        menu.addentry( 8, true, 'x', _( "Clear this pocket's rules" ) );
        menu.query();

        switch( menu.ret ) {
            case 0: {
                int priority = settings.priority();
                if( query_int( priority, priority,
                               _( "Priority?  Higher wins when something is stored automatically." ) ) ) {
                    settings.set_priority( priority );
                }
                break;
            }
            case 1:
                item_filter_menu( contents, settings );
                break;
            case 2:
                category_filter_menu( contents, settings );
                break;
            case 3:
                settings.set_disabled( !settings.is_disabled() );
                break;
            case 4:
                settings.set_collapse( !settings.is_collapsed() );
                break;
            case 5:
                settings.set_unloadable( !settings.is_unloadable() );
                break;
            case 6:
                save_preset_from( settings );
                break;
            case 7:
                apply_preset_to( settings );
                break;
            case 8:
                settings.clear();
                break;
            default:
                return;
        }
    }
}

} // namespace

void item_contents::favorite_settings_menu()
{
    // Presets live in a config file shared by every world, and this menu is the
    // only thing that reads them, so loading here keeps startup out of it.
    pocket_presets::load();

    // Classic mode ignores settings, so offering the menu would promise the
    // player something the mode does not honour.
    if( pockets_are_classic() ) {
        popup( _( "The classic pocket system has no pockets to organize." ) );
        return;
    }

    // General-purpose pockets only: a magazine well or a mod slot already knows
    // exactly what belongs in it, and nothing the player sets could improve on
    // that.
    std::vector<size_t> organizable;
    for( size_t i = 0; i < pockets.size(); i++ ) {
        if( pockets[i].definition().type == pocket_type::CONTAINER ) {
            organizable.push_back( i );
        }
    }
    if( organizable.empty() ) {
        popup( _( "This item has no pockets to organize." ) );
        return;
    }

    while( true ) {
        uilist menu;
        menu.title = string_format( _( "Organize %s" ), owner->tname() );
        for( size_t i = 0; i < organizable.size(); i++ ) {
            menu.addentry( static_cast<int>( i ), true, MENU_AUTOASSIGN, "%s",
                           describe_pocket( pockets[organizable[i]], static_cast<int>( i ) + 1 ) );
        }
        menu.query();
        if( menu.ret < 0 || static_cast<size_t>( menu.ret ) >= organizable.size() ) {
            return;
        }
        one_pocket_menu( *this, pockets[organizable[menu.ret]], menu.ret + 1 );
    }
}
