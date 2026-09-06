#include "avatar.h"
#include "catch/catch.hpp"
#include "character_id.h"
#include "game.h"
#include "item.h"
#include "item_contents.h"
#include "npc.h"
#include "npctrade.h"
#include "player_helpers.h"
#include "state_helpers.h"
#include "trade_pocket_ui.h"
#include "type_id.h"

TEST_CASE( "trade display nests pocket contents under their container",
           "[npc][trade][pocket][nesting]" )
{
    clear_all_state();
    auto bag = item::spawn( "bag_plastic" );
    REQUIRE( !bag->put_in( item::spawn( "test_rock" ) ) );
    item *rock = bag->contents.all_items_top().front();

    std::vector<item_pricing> offered;
    offered.emplace_back( std::vector<item *>{bag.get()}, 100, 1 );
    offered.emplace_back( std::vector<item *>{rock}, 10, 1 );

    trade_pocket_ui::trade_display_tree tree;
    trade_pocket_ui::build_trade_display_tree( tree, offered );
    REQUIRE( tree.rows.size() == 2 );

    CHECK( tree.rows[0].pricing_index == 0 );
    CHECK( tree.rows[0].indent == 0 );
    CHECK( tree.rows[1].pricing_index == 1 );
    CHECK( tree.rows[1].indent == 1 );
    REQUIRE( tree.parents.size() == 2 );
    CHECK_FALSE( tree.parents[0] );
    REQUIRE( tree.parents[1] );
    CHECK( *tree.parents[1] == 0 );
}

TEST_CASE( "collapsed trade rows hide nested pocket contents",
           "[npc][trade][pocket][nesting]" )
{
    clear_all_state();
    auto bag = item::spawn( "bag_plastic" );
    REQUIRE( !bag->put_in( item::spawn( "test_rock" ) ) );
    item *rock = bag->contents.all_items_top().front();

    std::vector<item_pricing> offered;
    offered.emplace_back( std::vector<item *>{bag.get()}, 100, 1 );
    offered.emplace_back( std::vector<item *>{rock}, 10, 1 );

    trade_pocket_ui::trade_display_tree tree;
    trade_pocket_ui::build_trade_display_tree( tree, offered );

    const auto visible_collapsed = trade_pocket_ui::build_visible_trade_rows( tree, offered, {} );
    CHECK( visible_collapsed.size() == 1 );
    CHECK( tree.rows[visible_collapsed.front()].pricing_index == 0 );

    trade_pocket_ui::toggle_row_collapse( tree, 0 );
    const auto visible_expanded = trade_pocket_ui::build_visible_trade_rows( tree, offered, {} );
    CHECK( visible_expanded.size() == 2 );
}

TEST_CASE( "trade display adds unlisted pocket contents under their container",
           "[npc][trade][pocket][nesting]" )
{
    clear_all_state();
    auto bag = item::spawn( "bag_plastic" );
    REQUIRE( !bag->put_in( item::spawn( "test_rock" ) ) );

    std::vector<item_pricing> offered;
    offered.emplace_back( std::vector<item *>{bag.get()}, 100, 1 );

    trade_pocket_ui::trade_display_tree tree;
    trade_pocket_ui::build_trade_display_tree( tree, offered );
    REQUIRE( tree.rows.size() == 2 );

    CHECK( tree.rows[0].pricing_index == 0 );
    CHECK( tree.rows[1].is_pocket_row() );
    CHECK( trade_pocket_ui::row_has_collapsible_children( tree, 0 ) );

    const auto visible_collapsed = trade_pocket_ui::build_visible_trade_rows( tree, offered, {} );
    CHECK( visible_collapsed.size() == 1 );

    trade_pocket_ui::toggle_row_collapse( tree, 0 );
    const auto visible_expanded = trade_pocket_ui::build_visible_trade_rows( tree, offered, {} );
    CHECK( visible_expanded.size() == 2 );
}

TEST_CASE( "transfer skips pocket contents when their container is traded",
           "[npc][trade][pocket][nesting]" )
{
    clear_all_state();
    auto bag = item::spawn( "bag_plastic" );
    REQUIRE( !bag->put_in( item::spawn( "test_rock" ) ) );
    item *rock = bag->contents.all_items_top().front();

    std::vector<item_pricing> offered;
    offered.emplace_back( std::vector<item *>{bag.get()}, 100, 1 );
    offered.emplace_back( std::vector<item *>{rock}, 10, 1 );
    offered[0].selected = true;
    offered[0].npc_has = 1;
    offered[1].selected = true;
    offered[1].npc_has = 1;

    CHECK( trade_pocket_ui::skip_because_container_traded( *rock, offered, false ) );
    CHECK_FALSE( trade_pocket_ui::skip_because_container_traded( *bag.get(), offered, false ) );
}
