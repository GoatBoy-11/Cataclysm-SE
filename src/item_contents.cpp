#include "item_contents.h"

#include <algorithm>
#include <limits>
#include <algorithm>
#include <memory>

#include "character.h"
#include "enums.h"
#include "handle_liquid.h"
#include "item.h"
#include "itype.h"
#include "locations.h"
#include "map.h"

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

item_pocket *item_contents::best_pocket( const item &it )
{
    // Classic mode: first pocket that will take it, no ranking. Combined with
    // can_contain()'s relaxed checks this reproduces pre-pocket inventory
    // behaviour without changing what is stored on disk.
    if( pockets_are_classic() ) {
        for( item_pocket &pocket : pockets ) {
            if( pocket.can_contain( it ).success() ) {
                return &pocket;
            }
        }
        return nullptr;
    }

    item_pocket *best = nullptr;
    int best_rank = -1;

    for( item_pocket &pocket : pockets ) {
        if( !pocket.can_contain( it ).success() ) {
            continue;
        }
        // A pocket that names what may go in it is the item's proper home; a
        // general-purpose pocket only happens to have room. Rank the former
        // higher so a magazine reaches the magazine well rather than the
        // roomy cargo pocket on the same item.
        const pocket_data &def = pocket.definition();
        const bool restricted = !def.ammo_restriction.empty() ||
                                !def.item_restriction.empty() ||
                                !def.mod_restriction.empty();
        const int rank = restricted ? 1 : 0;

        if( best == nullptr || rank > best_rank ||
            ( rank == best_rank &&
              pocket.remaining_volume() < best->remaining_volume() ) ) {
            best = &pocket;
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
