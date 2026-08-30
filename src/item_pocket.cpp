#include "item_pocket.h"

#include <map>
#include <string>
#include <utility>

#include "debug.h"
#include "enum_conversions.h"
#include "generic_factory.h"
#include "item.h"
#include "itype.h"
#include "json.h"
#include "locations.h"
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
}

void pocket_data::deserialize( JsonIn &jsin )
{
    const JsonObject jo = jsin.get_object();
    load( jo );
}

item_pocket::item_pocket( item *owner, const pocket_data *data )
    : owner( owner ), data( data ), contents( new contents_item_location( owner ) ) {}

item_pocket::item_pocket( item_pocket &&other ) noexcept
    : owner( other.owner ), data( other.data ),
      contents( new contents_item_location( other.owner ) )
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

ret_val<item_pocket::contain_code> item_pocket::can_contain( const item &it ) const
{
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
