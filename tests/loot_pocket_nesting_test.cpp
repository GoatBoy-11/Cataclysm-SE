#include "catch/catch.hpp"

#include "flag.h"
#include "game.h"
#include "item.h"
#include "loot_pocket_nesting.h"
#include "map.h"
#include "options_helpers.h"
#include "rng.h"
#include "state_helpers.h"
#include "type_id.h"

TEST_CASE( "spawned loot can nest loose items into container pockets in the batch",
           "[loot][pocket][nesting]" )
{
    clear_all_state();

    auto items = std::vector<detached_ptr<item>> {};
    items.push_back( item::spawn( "jeans" ) );
    items.push_back( item::spawn( "pen" ) );

    loot_pocket_nesting::nest_spawned_loot_in_containers( items,
    { .numerator = 1, .denominator = 1 } );
    REQUIRE( items.size() == 1 );
    CHECK( items.front()->typeId() == itype_id( "jeans" ) );
    REQUIRE( items.front()->contents.all_items_top().size() == 1 );
    CHECK( items.front()->contents.all_items_top().front()->typeId() == itype_id( "pen" ) );
}

TEST_CASE( "classic pocket mode leaves spawned loot batches flat",
           "[loot][pocket][nesting][classic]" )
{
    override_option classic( "POCKET_SYSTEM", "classic" );
    clear_all_state();

    auto items = std::vector<detached_ptr<item>> {};
    items.push_back( item::spawn( "jeans" ) );
    items.push_back( item::spawn( "pen" ) );

    loot_pocket_nesting::nest_spawned_loot_in_containers( items,
    { .numerator = 1, .denominator = 1 } );
    CHECK( items.size() == 2 );
}

TEST_CASE( "map spawn_items nests pocket loot before placing on the tile",
           "[loot][pocket][nesting]" )
{
    clear_all_state();
    rng_set_engine_seed( 4 );

    map &here = get_map();
    const tripoint_bub_ms pos{ 60, 60, 0 };
    here.i_clear( pos );

    auto items = std::vector<detached_ptr<item>> {};
    items.push_back( item::spawn( "jeans" ) );
    items.push_back( item::spawn( "pen" ) );
    here.spawn_items( pos, std::move( items ) );

    REQUIRE( here.i_at( pos ).size() == 1 );
    item *const jeans = *here.i_at( pos ).begin();
    REQUIRE( jeans->typeId() == itype_id( "jeans" ) );
    REQUIRE( jeans->contents.all_items_top().size() == 1 );
    CHECK( jeans->contents.all_items_top().front()->typeId() == itype_id( "pen" ) );
}

TEST_CASE( "classic map spawn_items keeps loot piles flat",
           "[loot][pocket][nesting][classic]" )
{
    override_option classic( "POCKET_SYSTEM", "classic" );
    clear_all_state();

    map &here = get_map();
    const tripoint_bub_ms pos{ 60, 60, 0 };
    here.i_clear( pos );

    auto items = std::vector<detached_ptr<item>> {};
    items.push_back( item::spawn( "jeans" ) );
    items.push_back( item::spawn( "pen" ) );
    here.spawn_items( pos, std::move( items ) );

    CHECK( here.i_at( pos ).size() == 2 );
}

TEST_CASE( "spawned loot nesting skips mission items",
           "[loot][pocket][nesting]" )
{
    clear_all_state();

    auto items = std::vector<detached_ptr<item>> {};
    items.push_back( item::spawn( "jeans" ) );
    detached_ptr<item> mission_pen = item::spawn( "pen" );
    mission_pen->set_flag( flag_MISSION_ITEM );
    items.push_back( std::move( mission_pen ) );

    loot_pocket_nesting::nest_spawned_loot_in_containers( items,
    { .numerator = 1, .denominator = 1 } );
    CHECK( items.size() == 2 );
}

TEST_CASE( "spawned loot nesting keeps clothing out of container pockets",
           "[loot][pocket][nesting]" )
{
    clear_all_state();

    auto items = std::vector<detached_ptr<item>> {};
    items.push_back( item::spawn( "backpack" ) );
    items.push_back( item::spawn( "jeans" ) );

    loot_pocket_nesting::nest_spawned_loot_in_containers( items,
    { .numerator = 1, .denominator = 1 } );
    CHECK( items.size() == 2 );
}
