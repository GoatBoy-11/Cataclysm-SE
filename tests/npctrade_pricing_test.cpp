#include "calendar.h"
#include "catch/catch.hpp"
#include "item.h"
#include "npctrade.h"
#include "state_helpers.h"
#include "cata_utility.h"
#include "character_id.h"
#include "avatar.h"
#include "game.h"
#include "npc.h"
#include "item_contents.h"
#include "type_id.h"
#include <algorithm>

#include <string>
#include <vector>

// https://github.com/cataclysmbn/Cataclysm-BN/issues/6986
TEST_CASE("low_price_materials_not_free", "[npc][trade][pricing]") {
    SECTION("Materials with low per-unit prices should not be traded for free") {
        clear_all_state();

        // Test items that have very low per-unit prices
        auto test_items = std::vector<std::pair<std::string, int>>{
            {"solder_wire", 200},      // 50 cent / 200 = 0.25 cent per unit
            {"material_quicklime", 50} // 10 cent / 50 = 0.2 cent per unit
        };

        for (const auto& [name, amount] : test_items) {
            SECTION("Testing " + name) {
                // Create item with default stack size
                auto test_material = item{name, calendar::turn, amount};

                // Get the price for trade purposes
                auto price = test_material.price(true);

                // The total price should be positive
                CHECK(price > 0);

                // Create item_pricing to test the per-unit pricing logic
                const auto pricing = item_pricing{{&test_material}, price, 1};

                // The per-unit price should not be zero
                CHECK(pricing.price > 0);

                // Verify that even a single unit has a positive price
                item single_unit(name, calendar::turn, 1);
                auto single_price = single_unit.price(true);

                // Even a single unit should have a minimum price of 1 cent
                CHECK(single_price > 0);
            }
        }
    }
}

// Routing puts a character's goods into worn pockets. init_buying() used to
// walk only the flat inventory, so once everything was put away properly the
// trade window offered nothing at all.
TEST_CASE( "pocketed items are offered for trade", "[npc][trade][pocket]" )
{
    clear_all_state();
    avatar &u = g->u;
    // on_pickup() only assigns ownership once the character has a real id, and
    // the test avatar has none by default. Without this the item stays unowned
    // and the check below cannot tell a fixed build from a broken one.
    const character_id previous_id = u.getID();
    u.setID( character_id( 1 ), true );
    const auto restore_id = on_out_of_scope( [&u, previous_id]() {
        u.setID( previous_id, true );
    } );
    REQUIRE( !u.wear_item( item::spawn( "backpack" ) ) );
    item *pack = u.worn.front();

    // Route it in rather than inserting by hand, and do NOT set an owner: an
    // earlier version of this test set one, which is exactly what hid the bug.
    // Routing has to establish ownership itself or trade cannot see the item.
    u.i_add_routed( item::spawn( "sugar" ) );
    REQUIRE( !pack->contents.all_items_top().empty() );

    standard_npc merchant( "merchant" );
    const std::vector<item_pricing> offered =
        npc_trading::init_buying( merchant, u, false );

    item *stored = pack->contents.all_items_top().front();
    INFO( "avatar id valid: " << u.getID().is_valid()
          << ", owner null: " << stored->get_owner().is_null()
          << ", owned by avatar: " << stored->is_owned_by( u ) );
    CHECK( stored->is_owned_by( u ) );

    const bool sugar_offered = std::ranges::any_of( offered,
    []( const item_pricing & ip ) {
        return !ip.locs.empty() && ip.locs.front()->typeId() == itype_id( "sugar" );
    } );
    INFO( "entries offered: " << offered.size() );
    CHECK( sugar_offered );
}

// init_selling() walked only the NPC's flat inventory, so a shopkeeper who had
// put their wares away had nothing to sell at all. And init_buying()'s pocket
// walk read one level, so a bag inside a pocket hid whatever was in it.
TEST_CASE( "an NPC sells what is in its pockets", "[npc][trade][pocket][nesting]" )
{
    clear_all_state();
    standard_npc merchant( "merchant" );
    REQUIRE( !merchant.wear_item( item::spawn( "backpack" ) ) );
    item *pack = merchant.worn.front();
    REQUIRE( !pack->put_in( item::spawn( "sugar" ) ) );
    // The precondition the bug turned on, asserted rather than assumed.
    REQUIRE( merchant.inv_const_slice().empty() );

    const std::vector<item_pricing> offered = npc_trading::init_selling( merchant );

    const bool sugar_offered = std::ranges::any_of( offered,
    []( const item_pricing & ip ) {
        return !ip.locs.empty() && ip.locs.front()->typeId() == itype_id( "sugar" );
    } );
    INFO( "entries offered: " << offered.size() );
    CHECK( sugar_offered );
}

TEST_CASE( "a container inside a pocket does not hide its contents from trade",
           "[npc][trade][pocket][nesting]" )
{
    clear_all_state();
    avatar &u = g->u;
    const character_id previous_id = u.getID();
    u.setID( character_id( 1 ), true );
    const auto restore_id = on_out_of_scope( [&u, previous_id]() {
        u.setID( previous_id, true );
    } );
    REQUIRE( !u.wear_item( item::spawn( "backpack" ) ) );
    item *pack = u.worn.front();

    auto bag = item::spawn( "bag_plastic" );
    REQUIRE( !bag->put_in( item::spawn( "sugar" ) ) );
    REQUIRE( !pack->put_in( std::move( bag ) ) );
    item *stored = pack->contents.all_items_top().front()->contents.all_items_top().front();
    REQUIRE( stored->typeId() == itype_id( "sugar" ) );
    stored->set_owner( u );

    standard_npc merchant( "merchant" );
    const std::vector<item_pricing> offered =
        npc_trading::init_buying( merchant, u, false );

    const bool sugar_offered = std::ranges::any_of( offered,
    []( const item_pricing & ip ) {
        return !ip.locs.empty() && ip.locs.front()->typeId() == itype_id( "sugar" );
    } );
    INFO( "entries offered: " << offered.size() );
    CHECK( sugar_offered );
}
