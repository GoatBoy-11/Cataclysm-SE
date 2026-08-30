#include "item_pocket.h"

#include <map>
#include <string>
#include <utility>

#include "debug.h"
#include "enum_conversions.h"
#include "generic_factory.h"
#include "item.h"
#include "item_category.h"
#include "itype.h"
#include "json.h"
#include "locations.h"
#include "options.h"
#include "translations.h"

namespace io
{
template<>
std::string enum_to_string<pocket_type>( pocket_type data )
{
    switch( data ) {
        case pocket_type::CONTAINER:
            return "CONTAINER";
        case pocket_type::MAGAZINE:
            return "MAGAZINE";
        case pocket_type::MAGAZINE_WELL:
            return "MAGAZINE_WELL";
        case pocket_type::MOD:
            return "MOD";
        case pocket_type::CORPSE:
            return "CORPSE";
        case pocket_type::MIGRATION:
            return "MIGRATION";
        case pocket_type::LAST:
            break;
    }
    debugmsg( "Invalid pocket_type" );
    abort();
}
} // namespace io

namespace
{

struct audit_key {
    itype_id container;
    itype_id inserted;

    bool operator<( const audit_key &rhs ) const {
        if( container != rhs.container ) {
            return container < rhs.container;
        }
        return inserted < rhs.inserted;
    }
};

std::map<audit_key, int> &audit_misses()
{
    static std::map<audit_key, int> misses;
    return misses;
}

} // namespace

void record_pocket_audit_miss( const item *container, const item &inserted )
{
    if( container == nullptr || container->type == nullptr ) {
        return;
    }
    audit_misses()[ { container->typeId(), inserted.typeId() } ]++;
}

void clear_pocket_audit()
{
    audit_misses().clear();
}

std::string pocket_audit_report()
{
    int total = 0;
    for( const auto &entry : audit_misses() ) {
        total += entry.second;
    }

    std::string report = string_format(
                             "Pocket insertion audit\n"
                             "  Insertions that would be rejected if can_contain() were enforced.\n"
                             "  distinct container/item pairs: %d\n"
                             "  total insertions:              %d\n\n",
                             static_cast<int>( audit_misses().size() ), total );

    if( audit_misses().empty() ) {
        report += "No misses recorded. Enforcement looks safe for everything exercised so far.\n";
        return report;
    }

    for( const auto &entry : audit_misses() ) {
        std::string pockets;
        const itype &def = *entry.first.container;
        for( const pocket_data &pocket : def.pockets ) {
            if( !pockets.empty() ) {
                pockets += ", ";
            }
            pockets += io::enum_to_string<pocket_type>( pocket.type );
        }
        if( pockets.empty() ) {
            pockets = "none";
        }
        report += string_format( "%s [pockets: %s] <- %s (x%d)\n",
                                 entry.first.container.str(), pockets,
                                 entry.first.inserted.str(), entry.second );
    }
    return report;
}

void pocket_favorite_settings::clear()
{
    *this = pocket_favorite_settings();
}

bool pocket_favorite_settings::is_null() const
{
    return !player_edited;
}

void pocket_favorite_settings::whitelist_item( const itype_id &id )
{
    item_blacklist.erase( id );
    item_whitelist.insert( id );
    player_edited = true;
}

void pocket_favorite_settings::blacklist_item( const itype_id &id )
{
    item_whitelist.erase( id );
    item_blacklist.insert( id );
    player_edited = true;
}

void pocket_favorite_settings::clear_item( const itype_id &id )
{
    item_whitelist.erase( id );
    item_blacklist.erase( id );
    player_edited = true;
}

void pocket_favorite_settings::whitelist_category( const item_category_id &id )
{
    category_blacklist.erase( id );
    category_whitelist.insert( id );
    player_edited = true;
}

void pocket_favorite_settings::blacklist_category( const item_category_id &id )
{
    category_whitelist.erase( id );
    category_blacklist.insert( id );
    player_edited = true;
}

void pocket_favorite_settings::clear_category( const item_category_id &id )
{
    category_whitelist.erase( id );
    category_blacklist.erase( id );
    player_edited = true;
}

bool pocket_favorite_settings::accepts_item( const item &it ) const
{
    // Precedence ported from CDDA's favorite_settings::accepts_item; the order
    // matters and the last two rules are not obvious.
    if( disabled ) {
        return false;
    }

    const itype_id &id = it.typeId();
    if( item_blacklist.count( id ) ) {
        return false;
    }
    if( item_whitelist.count( id ) ) {
        return true;
    }

    if( !category_blacklist.empty() || !category_whitelist.empty() ) {
        const item_category_id cat = it.get_category().get_id();
        if( category_blacklist.count( cat ) ) {
            return false;
        }
        if( category_whitelist.count( cat ) ) {
            return true;
        }
    }

    // A container is judged by what is inside it, not by itself, unless the
    // container's own type was explicitly listed above.
    if( it.is_container() && !it.contents.empty() ) {
        for( const item * const inner : it.contents.all_items_top() ) {
            if( !accepts_item( *inner ) ) {
                return false;
            }
        }
        return true;
    }

    // Nothing matched. A category whitelist means "only these categories".
    if( !category_whitelist.empty() ) {
        return false;
    }
    // An item whitelist means "only these items" - but only when it stands
    // alone. Alongside a category blacklist it reads as an exception to that
    // blacklist rather than an exclusive list.
    if( !item_whitelist.empty() && category_blacklist.empty() ) {
        return false;
    }
    return true;
}

void pocket_data::load( const JsonObject &jo )
{
    optional( jo, false, "pocket_type", type, pocket_type::CONTAINER );
    optional( jo, false, "max_contains_volume", max_contains_volume, volume_reader(),
              max_contains_volume );
    optional( jo, false, "max_contains_weight", max_contains_weight, mass_reader(),
              max_contains_weight );
    optional( jo, false, "max_item_length", max_item_length, length_reader(), max_item_length );
    optional( jo, false, "rigid", rigid, rigid );
    optional( jo, false, "watertight", watertight, watertight );
    optional( jo, false, "sealed", sealed, sealed );
    optional( jo, false, "spoil_multiplier", spoil_multiplier, spoil_multiplier );
    optional( jo, false, "moves", moves, moves );

    if( jo.has_object( "ammo_restriction" ) ) {
        ammo_restriction.clear();
        for( const JsonMember member : jo.get_object( "ammo_restriction" ) ) {
            ammo_restriction[ ammotype( member.name() ) ] = member.get_int();
        }
    }
    if( jo.has_array( "item_restriction" ) ) {
        item_restriction.clear();
        jo.read( "item_restriction", item_restriction );
    }
    if( jo.has_array( "flag_restriction" ) ) {
        flag_restriction.clear();
        jo.read( "flag_restriction", flag_restriction );
    }
    optional( jo, false, "holster", holster, holster );
}

void pocket_data::deserialize( JsonIn &jsin )
{
    const JsonObject jo = jsin.get_object();
    load( jo );
}

void pocket_favorite_settings::serialize( JsonOut &json ) const
{
    // Keys match CDDA's so their saves and ours describe settings the same way.
    // "name" is theirs alone for now: presets are not ported, and writing an
    // empty one would claim a preset the player never made.
    json.start_object();
    json.member( "priority", priority_rating );
    json.member( "item_whitelist", item_whitelist );
    json.member( "item_blacklist", item_blacklist );
    json.member( "category_whitelist", category_whitelist );
    json.member( "category_blacklist", category_blacklist );
    json.member( "collapsed", collapsed );
    json.member( "disabled", disabled );
    json.member( "unload", unload );
    json.member( "player_edited", player_edited );
    json.end_object();
}

void pocket_favorite_settings::deserialize( JsonIn &jsin )
{
    JsonObject data = jsin.get_object();
    data.allow_omitted_members();
    data.read( "priority", priority_rating );
    data.read( "item_whitelist", item_whitelist );
    data.read( "item_blacklist", item_blacklist );
    data.read( "category_whitelist", category_whitelist );
    data.read( "category_blacklist", category_blacklist );
    data.read( "collapsed", collapsed );
    data.read( "disabled", disabled );
    data.read( "unload", unload );
    if( !data.read( "player_edited", player_edited ) ) {
        // Settings only reach the save because someone edited them, so a block
        // from before the flag existed - CDDA's included - was player made.
        player_edited = true;
    }
}

item_pocket::item_pocket( item *owner, const pocket_data *data )
    : owner( owner ), data( data ), contents( new contents_item_location( owner ) ) {}

item_pocket::item_pocket( item_pocket &&other ) noexcept
    : owner( other.owner ), data( other.data ),
      contents( new contents_item_location( other.owner ) ),
      settings( std::move( other.settings ) )
{
    // Must happen after contents has a location: location_vector's move
    // assignment repoints every item at the *target's* location.
    contents = std::move( other.contents );
}

bool item_pocket::empty() const
{
    return contents.empty();
}

const std::vector<item *> &item_pocket::all_items_top() const
{
    return contents.as_vector();
}

units::volume item_pocket::contents_volume() const
{
    units::volume total = 0_ml;
    for( const item * const it : contents ) {
        total += it->volume();
    }
    return total;
}

units::volume item_pocket::remaining_volume() const
{
    return data->max_contains_volume - contents_volume();
}

units::mass item_pocket::contents_weight() const
{
    units::mass total = 0_gram;
    for( const item * const it : contents ) {
        total += it->weight();
    }
    return total;
}

bool pockets_are_classic()
{
    return get_options().has_option( "POCKET_SYSTEM" ) &&
           get_option<std::string>( "POCKET_SYSTEM" ) == "classic";
}

/**
 * Whether a magazine only fits because of a conversion mod fitted to the gun.
 *
 * A pocket is built from the gun's definition, which predates any mod, so it
 * cannot know that a caliber conversion or a magazine adapter has rewritten
 * what the gun takes. magazine_compatible() asks that of the gun as it is now.
 *
 * Both magazine pocket kinds qualify. A gun defined with an internal clip never
 * got a well at all, so an adapter's magazine would otherwise have nowhere in
 * the gun to go - which is exactly what happened to a converted handmade
 * carbine offered its BAR magazine.
 */
static bool accepts_converted_magazine( const item *owner, const pocket_data &data,
                                        const item &it )
{
    if( owner == nullptr || !it.is_magazine() ) {
        return false;
    }
    if( data.type != pocket_type::MAGAZINE_WELL && data.type != pocket_type::MAGAZINE ) {
        return false;
    }
    return owner->magazine_compatible().count( it.typeId() ) > 0;
}

ret_val<item_pocket::contain_code> item_pocket::can_contain( const item &it ) const
{
    // Classic mode: volume and weight only. The pockets still exist and still
    // hold their contents, but none of the type, length or rigidity rules apply,
    // so inventory behaves as it did before pockets landed.
    if( pockets_are_classic() ) {
        if( it.volume() > remaining_volume() ) {
            return ret_val<contain_code>::make_failure( contain_code::ERR_TOO_BIG,
                    _( "does not fit" ) );
        }
        if( data->max_contains_weight > 0_gram &&
            contents_weight() + it.weight() > data->max_contains_weight ) {
            return ret_val<contain_code>::make_failure( contain_code::ERR_TOO_HEAVY,
                    _( "is too heavy" ) );
        }
        return ret_val<contain_code>::make_success( contain_code::SUCCESS );
    }

    // A holster carries one item at a time.
    if( data->holster && !contents.empty() ) {
        return ret_val<contain_code>::make_failure( contain_code::ERR_NO_SPACE,
                _( "already holds something" ) );
    }

    // A flag-restricted pocket takes only items carrying one of its flags.
    if( !data->flag_restriction.empty() ) {
        bool any = false;
        for( const std::string &fl : data->flag_restriction ) {
            if( it.has_flag( flag_id( fl ) ) ) {
                any = true;
                break;
            }
        }
        if( !any ) {
            return ret_val<contain_code>::make_failure( contain_code::ERR_ITEM,
                    _( "does not belong in this pocket" ) );
        }
    }

    // Length: a long item will not go into a short pocket, however much room
    // it has by volume. Zero means the pocket sets no length limit.
    if( data->max_item_length > 0_mm && it.length() > data->max_item_length ) {
        return ret_val<contain_code>::make_failure( contain_code::ERR_TOO_BIG,
                _( "is too long" ) );
    }

    // A pocket that names specific items takes only those. This is what keeps a
    // magazine well from accepting anything that happens to fit.
    if( !data->item_restriction.empty() &&
        !data->item_restriction.contains( it.typeId() ) ) {
        if( !accepts_converted_magazine( owner, *data, it ) ) {
            return ret_val<contain_code>::make_failure( contain_code::ERR_ITEM,
                    _( "does not belong in this pocket" ) );
        }
    }

    // An internal clip is ammo-restricted, which would turn away the very
    // magazine an adapter just made valid, so that case is settled here.
    if( accepts_converted_magazine( owner, *data, it ) ) {
        return ret_val<contain_code>::make_success( contain_code::SUCCESS );
    }

    // A mod-restricted pocket takes only gunmods for a location this gun has, and
    // only as many per location as the gun allows.
    if( !data->mod_restriction.empty() ) {
        if( !it.is_gunmod() ) {
            return ret_val<contain_code>::make_failure( contain_code::ERR_MOD,
                    _( "is not a gun modification" ) );
        }
        const std::string location = it.type->gunmod->location.str();
        const auto allowed = data->mod_restriction.find( location );
        if( allowed == data->mod_restriction.end() ) {
            return ret_val<contain_code>::make_failure( contain_code::ERR_MOD,
                    _( "cannot be attached there" ) );
        }
        int installed = 0;
        for( const item * const existing : contents ) {
            if( existing->is_gunmod() &&
                existing->type->gunmod->location.str() == location ) {
                installed++;
            }
        }
        if( installed >= allowed->second ) {
            return ret_val<contain_code>::make_failure( contain_code::ERR_MOD,
                    _( "has no room left there" ) );
        }
        return ret_val<contain_code>::make_success( contain_code::SUCCESS );
    }

    // An ammo-restricted pocket takes nothing but the ammo it names. This is what
    // stops a magazine doubling as general storage.
    if( !data->ammo_restriction.empty() ) {
        if( !it.is_ammo() ) {
            return ret_val<contain_code>::make_failure( contain_code::ERR_AMMO,
                    _( "is not ammunition" ) );
        }
        const auto allowed = data->ammo_restriction.find( it.ammo_type() );
        if( allowed == data->ammo_restriction.end() ) {
            return ret_val<contain_code>::make_failure( contain_code::ERR_AMMO,
                    _( "is the wrong ammunition" ) );
        }
        int already = 0;
        for( const item * const existing : contents ) {
            already += existing->charges;
        }
        if( already + it.charges > allowed->second ) {
            return ret_val<contain_code>::make_failure( contain_code::ERR_AMMO,
                    _( "does not have room for that many rounds" ) );
        }
        // Ammo capacity is counted in charges, not volume, so stop here.
        return ret_val<contain_code>::make_success( contain_code::SUCCESS );
    }

    if( it.volume() > remaining_volume() ) {
        return ret_val<contain_code>::make_failure( contain_code::ERR_TOO_BIG,
                _( "does not fit" ) );
    }
    if( data->max_contains_weight > 0_gram &&
        contents_weight() + it.weight() > data->max_contains_weight ) {
        return ret_val<contain_code>::make_failure( contain_code::ERR_TOO_HEAVY,
                _( "is too heavy" ) );
    }
    return ret_val<contain_code>::make_success( contain_code::SUCCESS );
}

void item_pocket::insert( detached_ptr<item> &&it )
{
    contents.push_back( std::move( it ) );
}

detached_ptr<item> item_pocket::remove( item *it )
{
    detached_ptr<item> removed;
    // Deliberately not location_vector::remove(), which debugmsgs when the
    // item is absent. Callers fan out across pockets and expect a silent miss.
    for( auto iter = contents.begin(); iter != contents.end(); ++iter ) {
        if( *iter == it ) {
            contents.erase( iter, &removed );
            return removed;
        }
    }
    return removed;
}

std::vector<detached_ptr<item>> item_pocket::clear()
{
    return contents.clear();
}

void item_pocket::on_destroy()
{
    contents.on_destroy();
}
