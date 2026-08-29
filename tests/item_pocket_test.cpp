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

// A whole item as a pre-pocket save wrote it: "contents" is an object holding a
// flat "items" array. Exercises item::deserialize's branch into the legacy path.
TEST_CASE( "an_item_saved_before_pockets_keeps_its_contents", "[item][pocket][save]" )
{
    const std::string legacy =
        R"([ { "typeid": "backpack", "contents": { "items": [ { "typeid": "sugar" } ] } } ])";
    std::istringstream is( legacy );
    JsonIn ji( is );

    std::vector<detached_ptr<item>> loaded;
    REQUIRE( ji.read( loaded ) );

    REQUIRE( loaded.size() == 1 );
    REQUIRE( loaded.front()->typeId() == itype_id( "backpack" ) );
    CHECK( loaded.front()->contents.all_items_top().size() == 1 );
    CHECK( loaded.front()->contents.all_items_top().front()->typeId() == itype_id( "sugar" ) );
}

// NOTE: an end-to-end g->save()/g->load() test was tried here and removed. A
// successful load mid-suite replaces the world and avatar, and the surrounding
// tests do not survive it (146 failures and a Lua panic). The plan's manual
// save-and-reload check stays manual.

// ---------------------------------------------------------------------------
// Phase 2: pocket_data loaded from JSON
// ---------------------------------------------------------------------------

TEST_CASE( "pocket_data_reads_every_field_from_json", "[item][pocket][json]" )
{
    const std::string text =
        R"({ "pocket_type": "MAGAZINE_WELL", "max_contains_volume": "2 L",
             "max_contains_weight": "3 kg", "max_item_length": "30 cm",
             "rigid": true, "watertight": true, "sealed": true,
             "spoil_multiplier": 0.5, "moves": 250 })";
    std::istringstream is( text );
    JsonIn jsin( is );

    pocket_data data;
    data.deserialize( jsin );

    CHECK( data.type == pocket_type::MAGAZINE_WELL );
    CHECK( data.max_contains_volume == 2_liter );
    CHECK( data.max_contains_weight == units::from_kilogram( 3 ) );
    CHECK( data.max_item_length == 30_cm );
    CHECK( data.rigid );
    CHECK( data.watertight );
    CHECK( data.sealed );
    CHECK( data.spoil_multiplier == Approx( 0.5f ) );
    CHECK( data.moves == 250 );
}

TEST_CASE( "omitted_pocket_data_fields_keep_their_defaults", "[item][pocket][json]" )
{
    const std::string text = R"({ "max_contains_volume": "1 L" })";
    std::istringstream is( text );
    JsonIn jsin( is );

    pocket_data data;
    data.deserialize( jsin );

    CHECK( data.type == pocket_type::CONTAINER );
    CHECK( data.max_contains_volume == 1_liter );
    CHECK( data.max_contains_weight == 0_gram );
    CHECK_FALSE( data.rigid );
    CHECK_FALSE( data.watertight );
    CHECK( data.moves == 100 );
}

TEST_CASE( "an_item_declaring_pocket_data_gets_those_pockets", "[item][pocket][json]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    const std::vector<item_pocket> &pockets = bag->contents.get_pockets();

    REQUIRE( pockets.size() == 2 );
    CHECK( pockets[0].definition().max_contains_volume == 100_ml );
    CHECK( pockets[0].definition().max_item_length == 5_cm );
    CHECK( pockets[1].definition().max_contains_volume == 4_liter );
    CHECK( pockets[1].definition().watertight );
    CHECK( pockets[1].definition().moves == 200 );
}

TEST_CASE( "authored_pocket_data_suppresses_legacy_synthesis", "[item][pocket][json]" )
{
    // A synthesized item still gets exactly one pocket, so base-game behaviour
    // is untouched by pocket_data loading existing.
    detached_ptr<item> backpack = item::spawn( "backpack" );
    CHECK( backpack->contents.get_pockets().size() == 1 );

    // The authored item keeps its two, rather than gaining a synthesized third.
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    CHECK( bag->contents.get_pockets().size() == 2 );
}

TEST_CASE( "an_item_too_big_for_the_small_pocket_lands_in_the_large_one",
           "[item][pocket][json]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );

    // 250 ml, so it cannot fit the 100 ml pocket but fits the 4 L one.
    detached_ptr<item> rock = item::spawn( "test_rock" );
    const units::volume rock_volume = rock->volume();
    REQUIRE( rock_volume > 100_ml );

    bag->contents.insert_item( std::move( rock ) );

    CHECK( bag->contents.get_pockets()[0].empty() );
    CHECK( bag->contents.get_pockets()[1].all_items_top().size() == 1 );
}

TEST_CASE( "multi_pocket_contents_survive_a_serialization_round_trip",
           "[item][pocket][json][save]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    bag->contents.insert_item( item::spawn( "test_rock" ) );
    REQUIRE( bag->contents.all_items_top().size() == 1 );

    std::ostringstream os;
    JsonOut jo( os );
    bag->contents.serialize( jo );

    detached_ptr<item> restored = item::spawn( "test_two_pocket_bag" );
    std::istringstream is( os.str() );
    JsonIn ji( is );
    restored->contents.deserialize( ji );

    REQUIRE( restored->contents.get_pockets().size() == 2 );
    CHECK( restored->contents.all_items_top().size() == 1 );
    // Still in the second pocket, not collapsed into the first.
    CHECK( restored->contents.get_pockets()[0].empty() );
    CHECK( restored->contents.get_pockets()[1].all_items_top().size() == 1 );
}
