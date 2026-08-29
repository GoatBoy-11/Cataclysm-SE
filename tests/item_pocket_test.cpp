#include "catch/catch.hpp"

#include <utility>

#include "detached_ptr.h"
#include "item.h"
#include "item_pocket.h"
#include "ret_val.h"
#include "units.h"

TEST_CASE( "empty_pocket_reports_empty_and_full_remaining_volume", "[item][pocket]" )
{
    detached_ptr<item> holder = item::spawn( "backpack" );
    pocket_data data;
    data.type = pocket_type::CONTAINER;
    data.max_contains_volume = 2500_ml;

    item_pocket pocket( holder.get(), &data );

    CHECK( pocket.empty() );
    CHECK( pocket.contents_volume() == 0_ml );
    CHECK( pocket.remaining_volume() == 2500_ml );
}

TEST_CASE( "pocket_accepts_and_holds_an_item_that_fits", "[item][pocket]" )
{
    detached_ptr<item> holder = item::spawn( "backpack" );
    pocket_data data;
    data.type = pocket_type::CONTAINER;
    data.max_contains_volume = 2500_ml;
    item_pocket pocket( holder.get(), &data );

    detached_ptr<item> sugar = item::spawn( "sugar" );
    const units::volume sugar_volume = sugar->volume();
    REQUIRE( pocket.can_contain( *sugar ).success() );

    pocket.insert( std::move( sugar ) );

    CHECK_FALSE( pocket.empty() );
    CHECK( pocket.all_items_top().size() == 1 );
    CHECK( pocket.contents_volume() == sugar_volume );
}

TEST_CASE( "pocket_rejects_an_item_that_is_too_large", "[item][pocket]" )
{
    detached_ptr<item> holder = item::spawn( "backpack" );
    pocket_data data;
    data.type = pocket_type::CONTAINER;
    data.max_contains_volume = 1_ml;
    item_pocket pocket( holder.get(), &data );

    detached_ptr<item> sugar = item::spawn( "sugar" );
    ret_val<item_pocket::contain_code> res = pocket.can_contain( *sugar );

    CHECK_FALSE( res.success() );
    CHECK( res.value() == item_pocket::contain_code::ERR_TOO_BIG );
}

TEST_CASE( "removing_an_item_empties_the_pocket", "[item][pocket]" )
{
    detached_ptr<item> holder = item::spawn( "backpack" );
    pocket_data data;
    data.type = pocket_type::CONTAINER;
    data.max_contains_volume = 2500_ml;
    item_pocket pocket( holder.get(), &data );

    detached_ptr<item> sugar = item::spawn( "sugar" );
    item *raw = sugar.get();
    pocket.insert( std::move( sugar ) );
    REQUIRE_FALSE( pocket.empty() );

    detached_ptr<item> taken = pocket.remove( raw );

    CHECK( taken );
    CHECK( pocket.empty() );
    CHECK( pocket.contents_volume() == 0_ml );
}

TEST_CASE( "item_contents_round_trips_through_a_single_pocket", "[item][pocket][contents]" )
{
    detached_ptr<item> backpack = item::spawn( "backpack" );
    detached_ptr<item> sugar = item::spawn( "sugar" );
    item *raw = sugar.get();

    REQUIRE( backpack->contents.empty() );

    ret_val<bool> inserted = backpack->contents.insert_item( std::move( sugar ) );
    REQUIRE( inserted.success() );

    CHECK_FALSE( backpack->contents.empty() );
    CHECK( backpack->contents.all_items_top().size() == 1 );
    CHECK( backpack->contents.all_items_top().front() == raw );
    CHECK( backpack->contents.num_item_stacks() == 1 );

    detached_ptr<item> taken = backpack->contents.remove_top( raw );

    CHECK( taken );
    CHECK( backpack->contents.empty() );
}

TEST_CASE( "item_contents_exposes_exactly_one_pocket_in_phase_one", "[item][pocket][contents]" )
{
    detached_ptr<item> backpack = item::spawn( "backpack" );

    CHECK( backpack->contents.get_pockets().size() == 1 );
    CHECK( backpack->contents.get_pockets().front().definition().type == pocket_type::CONTAINER );
}
