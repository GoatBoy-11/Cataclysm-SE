#include "catch/catch.hpp"

#include <sstream>
#include <utility>

#include "detached_ptr.h"
#include "item.h"
#include "item_pocket.h"
#include "itype.h"
#include "json.h"
#include "ret_val.h"
#include "type_id.h"
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

TEST_CASE( "a_container_gains_one_pocket_sized_from_its_legacy_storage",
           "[item][pocket][synthesis]" )
{
    detached_ptr<item> bottle = item::spawn( "bottle_plastic" );
    const std::vector<item_pocket> &pockets = bottle->contents.get_pockets();

    REQUIRE( pockets.size() == 1 );
    CHECK( pockets.front().definition().type == pocket_type::CONTAINER );
    CHECK( pockets.front().definition().watertight );
    CHECK( pockets.front().remaining_volume() == bottle->type->container->contains );
}

TEST_CASE( "worn_storage_synthesizes_a_non_rigid_pocket", "[item][pocket][synthesis]" )
{
    detached_ptr<item> backpack = item::spawn( "backpack" );
    const std::vector<item_pocket> &pockets = backpack->contents.get_pockets();

    REQUIRE( pockets.size() == 1 );
    CHECK_FALSE( pockets.front().definition().rigid );
    CHECK( pockets.front().remaining_volume() == backpack->type->armor->storage );
}

TEST_CASE( "pocket_contents_survive_a_serialization_round_trip", "[item][pocket][save]" )
{
    detached_ptr<item> backpack = item::spawn( "backpack" );
    backpack->contents.insert_item( item::spawn( "sugar" ) );

    std::ostringstream os;
    JsonOut jo( os );
    backpack->contents.serialize( jo );

    detached_ptr<item> restored = item::spawn( "backpack" );
    std::istringstream is( os.str() );
    JsonIn ji( is );
    restored->contents.deserialize( ji );

    CHECK( restored->contents.all_items_top().size() == 1 );
    CHECK( restored->contents.all_items_top().front()->typeId() == itype_id( "sugar" ) );
}

TEST_CASE( "serialized_contents_use_the_pocket_format", "[item][pocket][save]" )
{
    detached_ptr<item> backpack = item::spawn( "backpack" );
    backpack->contents.insert_item( item::spawn( "sugar" ) );

    std::ostringstream os;
    JsonOut jo( os );
    backpack->contents.serialize( jo );

    CHECK( os.str().find( "\"pockets\"" ) != std::string::npos );
    CHECK( os.str().find( "\"pocket_type\"" ) != std::string::npos );
}

TEST_CASE( "a_legacy_flat_contents_array_loads_into_the_first_pocket", "[item][pocket][save]" )
{
    // The pre-pocket item_contents format: a bare "items" array. The plan named
    // this key "contents", but that is the *item*-level ancient array, which
    // item::deserialize already handles before item_contents ever sees it.
    const std::string legacy = R"({ "items": [ { "typeid": "sugar" } ] })";

    detached_ptr<item> backpack = item::spawn( "backpack" );
    std::istringstream is( legacy );
    JsonIn ji( is );
    backpack->contents.deserialize( ji );

    CHECK( backpack->contents.all_items_top().size() == 1 );
    CHECK( backpack->contents.get_pockets().front().all_items_top().size() == 1 );
}
