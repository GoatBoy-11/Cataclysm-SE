#include "item_pocket.h"

#include <utility>

#include "item.h"
#include "locations.h"
#include "translations.h"

item_pocket::item_pocket( item *owner, const pocket_data *data )
    : data( data ), contents( new contents_item_location( owner ) ) {}

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
