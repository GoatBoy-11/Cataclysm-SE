#include "catch/catch.hpp"

#include "avatar.h"
#include "character_id.h"
#include "game.h"
#include "item.h"
#include "map.h"
#include "options_helpers.h"
#include "item_pocket.h"
#include "pickup_pocket_ui.h"
#include "pickup_token.h"
#include "pickup.h"
#include "state_helpers.h"
#include "type_id.h"

TEST_CASE( "pickup list expands pocket contents as child rows", "[pickup][pocket]" )
{
    clear_all_state();

    detached_ptr<item> jeans = item::spawn( "jeans" );
    REQUIRE( !jeans->put_in( item::spawn( "withered" ) ) );

    map &here = get_map();
    const tripoint_bub_ms pos{ 60, 60, 0 };
    here.i_clear( pos );
    here.add_item_or_charges( pos, std::move( jeans ) );

    map_stack stack = here.i_at( pos );
    REQUIRE( stack.size() == 1 );

    std::vector<item_stack::iterator> ground_items{ stack.begin() };
    const std::vector<std::list<item_stack::iterator>> ground_stacks =
        pickup::flatten( pickup::stack_for_pickup_ui( ground_items ) );

    std::vector<pickup_pocket_ui::pickup_stack_entry> entries;
    for( const std::list<item_stack::iterator> &ground : ground_stacks ) {
        entries.push_back( { ground, {} } );
    }

    const std::vector<std::optional<size_t>> token_parents = pickup::calculate_parents( ground_stacks );
    std::vector<std::optional<size_t>> parents;
    std::vector<std::vector<size_t>> children;
    std::vector<int> indents;
    pickup_pocket_ui::expand_with_pocket_contents( entries, parents, children, indents, token_parents );

    REQUIRE( entries.size() == 2 );
    REQUIRE( entries[1].is_pocket_row() );
    REQUIRE( entries[1].pocket_items.size() == 1 );
    REQUIRE( entries[1].pocket_items.front()->typeId() == itype_id( "withered" ) );
    REQUIRE( parents[1].has_value() );
    CHECK( *parents[1] == 0 );
    CHECK( children[0].size() == 1 );
    CHECK( children[0].front() == 1 );
}

TEST_CASE( "spilling container pockets leaves items on the tile", "[pickup][pocket]" )
{
    clear_all_state();

    detached_ptr<item> jeans = item::spawn( "jeans" );
    REQUIRE( !jeans->put_in( item::spawn( "withered" ) ) );

    map &here = get_map();
    const tripoint_bub_ms pos{ 60, 60, 0 };
    here.i_clear( pos );
    here.add_item_or_charges( pos, std::move( jeans ) );
    item *jeans_on_ground = nullptr;
    for( item *it : here.i_at( pos ) ) {
        if( it->typeId() == itype_id( "jeans" ) ) {
            jeans_on_ground = it;
        }
    }
    REQUIRE( jeans_on_ground != nullptr );
    REQUIRE( !jeans_on_ground->contents.empty() );

    pickup_pocket_ui::spill_container_pockets_onto_tile( *jeans_on_ground, pos );

    CHECK( jeans_on_ground->contents.empty() );
    bool found_withered = false;
    for( item *it : here.i_at( pos ) ) {
        if( it->typeId() == itype_id( "withered" ) ) {
            found_withered = true;
        }
    }
    CHECK( found_withered );
}

TEST_CASE( "picking a pocketed item alone leaves the container on the tile", "[pickup][pocket][routing]" )
{
    clear_all_state();
    avatar &u = g->u;
    u.setID( character_id( 1 ), true );
    u.moves = 100;

    detached_ptr<item> jeans = item::spawn( "jeans" );
    REQUIRE( !jeans->put_in( item::spawn( "withered" ) ) );

    map &here = get_map();
    const tripoint_bub_ms pos = u.bub_pos();
    here.i_clear( pos );
    here.add_item_or_charges( pos, std::move( jeans ) );

    item *withered = nullptr;
    for( item *ground : here.i_at( pos ) ) {
        for( const item_pocket &pocket : ground->contents.get_pockets() ) {
            for( item *stored : pocket.all_items_top() ) {
                if( stored->typeId() == itype_id( "withered" ) ) {
                    withered = stored;
                }
            }
        }
    }
    REQUIRE( withered != nullptr );

    std::vector<pickup::pick_drop_selection> targets{ { *withered, std::nullopt, {} } };
    pickup::do_pickup( targets, false );

    CHECK( u.amount_of( itype_id( "withered" ) ) == 1 );
    bool jeans_still_on_ground = false;
    for( item *it : here.i_at( pos ) ) {
        if( it->typeId() == itype_id( "jeans" ) ) {
            jeans_still_on_ground = true;
            CHECK( it->contents.empty() );
        }
    }
    CHECK( jeans_still_on_ground );
}

TEST_CASE( "classic mode does not expand pickup pocket rows", "[pickup][pocket][classic]" )
{
    override_option classic( "POCKET_SYSTEM", "classic" );
    clear_all_state();

    detached_ptr<item> jeans = item::spawn( "jeans" );
    REQUIRE( !jeans->put_in( item::spawn( "withered" ) ) );

    map &here = get_map();
    const tripoint_bub_ms pos{ 60, 60, 0 };
    here.i_clear( pos );
    here.add_item_or_charges( pos, std::move( jeans ) );

    map_stack stack = here.i_at( pos );
    std::vector<item_stack::iterator> ground_items{ stack.begin() };
    const std::vector<std::list<item_stack::iterator>> ground_stacks =
        pickup::flatten( pickup::stack_for_pickup_ui( ground_items ) );

    std::vector<pickup_pocket_ui::pickup_stack_entry> entries;
    for( const std::list<item_stack::iterator> &ground : ground_stacks ) {
        entries.push_back( { ground, {} } );
    }

    const std::vector<std::optional<size_t>> token_parents = pickup::calculate_parents( ground_stacks );
    std::vector<std::optional<size_t>> parents;
    std::vector<std::vector<size_t>> children;
    std::vector<int> indents;
    pickup_pocket_ui::expand_with_pocket_contents( entries, parents, children, indents, token_parents );

    CHECK( entries.size() == 1 );
}

TEST_CASE( "pocket hierarchies start collapsed in pickup expansion", "[pickup][pocket]" )
{
    clear_all_state();

    detached_ptr<item> jeans = item::spawn( "jeans" );
    REQUIRE( !jeans->put_in( item::spawn( "withered" ) ) );

    map &here = get_map();
    const tripoint_bub_ms pos{ 60, 60, 0 };
    here.i_clear( pos );
    here.add_item_or_charges( pos, std::move( jeans ) );

    map_stack stack = here.i_at( pos );
    std::vector<item_stack::iterator> ground_items{ stack.begin() };
    const std::vector<std::list<item_stack::iterator>> ground_stacks =
        pickup::flatten( pickup::stack_for_pickup_ui( ground_items ) );

    std::vector<pickup_pocket_ui::pickup_stack_entry> entries;
    for( const std::list<item_stack::iterator> &ground : ground_stacks ) {
        entries.push_back( { ground, {} } );
    }

    const std::vector<std::optional<size_t>> token_parents = pickup::calculate_parents( ground_stacks );
    std::vector<std::optional<size_t>> parents;
    std::vector<std::vector<size_t>> children;
    std::vector<int> indents;
    pickup_pocket_ui::expand_with_pocket_contents( entries, parents, children, indents, token_parents );

    std::vector<bool> collapsed;
    pickup_pocket_ui::apply_initial_collapse( collapsed, children );

    REQUIRE( entries.size() == 2 );
    CHECK( collapsed[0] );
    CHECK_FALSE( collapsed[1] );
}
