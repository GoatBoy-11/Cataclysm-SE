#include "behavior.h"
#include "behavior_strategy.h"
#include "catch/catch.hpp"
#include "character_oracle.h"
#include "item.h"
#include "map.h"
#include "map_helpers.h"
#include "monster.h"
#include "monster_oracle.h"
#include "mtype.h"
#include "npc.h"
#include "player.h"
#include "player_helpers.h"
#include "state_helpers.h"
#include "string_id.h"
#include "weather.h"

#include <climits>
#include <memory>
#include <string>
#include <vector>

namespace behavior {
class oracle_t;

extern sequential_t default_sequential;
extern fallback_t default_fallback;
extern sequential_until_done_t default_until_done;
} // namespace behavior

static behavior::node_t make_test_node(std::string goal, const behavior::status_t* status) {
    behavior::node_t node;
    if (!goal.empty()) { node.set_goal(goal); }
    node.set_predicate([status](const behavior::oracle_t*) { return *status; });
    return node;
}

TEST_CASE("behavior_tree", "[behavior]") {
    clear_all_state();
    behavior::status_t cold_state = behavior::running;
    behavior::status_t thirsty_state = behavior::running;
    behavior::status_t hungry_state = behavior::running;
    behavior::status_t clothes_state = behavior::running;
    behavior::status_t fire_state = behavior::running;
    behavior::status_t water_state = behavior::running;
    behavior::status_t clean_water_state = behavior::running;

    behavior::node_t clothes = make_test_node("wear", &clothes_state);
    behavior::node_t fire = make_test_node("fire", &fire_state);
    behavior::node_t cold = make_test_node("", &cold_state);
    cold.add_child(&clothes);
    cold.add_child(&fire);
    cold.set_strategy(&behavior::default_fallback);
    behavior::node_t water = make_test_node("get_water", &water_state);
    behavior::node_t clean_water = make_test_node("clean_water", &clean_water_state);
    behavior::node_t thirst = make_test_node("", &thirsty_state);
    thirst.add_child(&water);
    thirst.add_child(&clean_water);
    thirst.set_strategy(&behavior::default_sequential);
    behavior::node_t hunger = make_test_node("get_food", &hungry_state);

    behavior::node_t basic_needs;
    basic_needs.set_strategy(&behavior::default_until_done);

    basic_needs.add_child(&cold);
    basic_needs.add_child(&thirst);
    basic_needs.add_child(&hunger);

    behavior::tree maslows;
    maslows.add(&basic_needs);

    // First and therefore highest priority goal.
    CHECK(maslows.tick(nullptr) == "wear");
    thirsty_state = behavior::success;
    // Later states don't matter.
    CHECK(maslows.tick(nullptr) == "wear");
    hungry_state = behavior::success;
    // This one either.
    CHECK(maslows.tick(nullptr) == "wear");
    cold_state = behavior::success;
    thirsty_state = behavior::running;
    // First need met, second branch followed.
    CHECK(maslows.tick(nullptr) == "get_water");
    cold_state = behavior::failure;
    // First need failed, second branch followed.
    CHECK(maslows.tick(nullptr) == "get_water");
    water_state = behavior::success;
    // Got water, proceed to next goal.
    CHECK(maslows.tick(nullptr) == "clean_water");
    clean_water_state = behavior::success;
    hungry_state = behavior::running;
    // Got clean water, proceed to food.
    CHECK(maslows.tick(nullptr) == "get_food");
    water_state = behavior::failure;
    clean_water_state = behavior::running;
    // Failed to get water, give up.
    CHECK(maslows.tick(nullptr) == "get_food");
    water_state = behavior::running;
    CHECK(maslows.tick(nullptr) == "get_water");
    cold_state = behavior::success;
    thirsty_state = behavior::success;
    hungry_state = behavior::running;
    // Second need also met, third branch taken.
    CHECK(maslows.tick(nullptr) == "get_food");
    cold_state = behavior::success;
    // Failure also causes third branch to be taken.
    CHECK(maslows.tick(nullptr) == "get_food");
    thirsty_state = behavior::success;
    // Failure in second branch too.
    CHECK(maslows.tick(nullptr) == "get_food");
    thirsty_state = behavior::running;
    cold_state = behavior::running;
    // First need appears again and becomes highest priority again.
    CHECK(maslows.tick(nullptr) == "wear");
    clothes_state = behavior::failure;
    // First alternative failed, attempting second.
    CHECK(maslows.tick(nullptr) == "fire");
    fire_state = behavior::failure;
    // Both alternatives failed, check other needs.
    CHECK(maslows.tick(nullptr) == "get_water");
    clothes_state = behavior::success;
    // Either failure or success meets requirements.
    CHECK(maslows.tick(nullptr) == "get_water");
    clothes_state = behavior::failure;
    fire_state = behavior::success;
    // Either order does it.
    CHECK(maslows.tick(nullptr) == "get_water");
    hungry_state = behavior::running;
    // Still thirsty, so changes to hunger are irrelevant.
    CHECK(maslows.tick(nullptr) == "get_water");
    thirsty_state = behavior::success;
    hungry_state = behavior::success;
    // All needs met, so no goals remain.
    CHECK(maslows.tick(nullptr) == "idle");
}

// Make assertions about loaded behaviors.
TEST_CASE("check_npc_behavior_tree", "[npc][behavior]") {
    clear_all_state();
    behavior::tree npc_needs;
    npc_needs.add(&string_id<behavior::node_t>("npc_needs").obj());
    npc& test_npc = spawn_npc({50, 50, 0}, "test_talker");
    clear_character(test_npc);
    behavior::character_oracle_t oracle(&test_npc);
    CHECK(npc_needs.tick(&oracle) == "idle");
    SECTION("Freezing") {
        weather_manager& weather = get_weather();
        weather.temperature = -100_c;
        weather.clear_temp_cache();
        test_npc.update_bodytemp(get_map(), weather);
        CHECK(npc_needs.tick(&oracle) == "idle");
        detached_ptr<item> det = item::spawn(itype_id("sweater"));
        item& sweater = *det;
        test_npc.i_add(std::move(det));
        CHECK(npc_needs.tick(&oracle) == "wear_warmer_clothes");
        det = sweater.detach();
        test_npc.wear_item(std::move(det));
        CHECK(npc_needs.tick(&oracle) == "idle");
        test_npc.i_add(item::spawn(itype_id("lighter")));
        test_npc.i_add(item::spawn(itype_id("2x4")));
        CHECK(npc_needs.tick(&oracle) == "start_fire");
    }
    SECTION("Hungry") {
        test_npc.set_stored_kcal(1000);
        test_npc.set_thirst(100);
        CHECK(oracle.needs_food_badly() == behavior::status_t::running);
        REQUIRE(npc_needs.tick(&oracle) == "idle");
        detached_ptr<item> det = item::spawn(itype_id("pelmeni"));
        item& food = *det;
        test_npc.i_add(std::move(det));
        REQUIRE(oracle.has_food() == behavior::status_t::running);
        CHECK(npc_needs.tick(&oracle) == "eat_food");
        test_npc.consume(food);
        CHECK(npc_needs.tick(&oracle) == "idle");
    }
    SECTION("Thirsty") {
        test_npc.set_thirst(700);
        CHECK(npc_needs.tick(&oracle) == "idle");
        detached_ptr<item> det = item::spawn(itype_id("water"));
        item& water = *det;
        test_npc.i_add(std::move(det));
        CHECK(npc_needs.tick(&oracle) == "drink_water");
        test_npc.consume(water);
        CHECK(npc_needs.tick(&oracle) == "idle");
    }
}

TEST_CASE("check_monster_behavior_tree", "[monster][behavior]") {
    clear_all_state();
    behavior::tree monster_goals;
    monster_goals.add(&string_id<behavior::node_t>("monster_special").obj());
    monster& test_monster = spawn_test_monster("mon_zombie", {5, 5, 0});
    for (const std::string& special_name : test_monster.type->special_attacks_names) {
        test_monster.reset_special(special_name);
    }
    behavior::monster_oracle_t oracle(&test_monster);
    CHECK(monster_goals.tick(&oracle) == "idle");
    SECTION("Special Attack") {
        test_monster.set_special("bite", 0);
        CHECK(monster_goals.tick(&oracle) == "do_special");
        test_monster.set_special("bite", 1);
        CHECK(monster_goals.tick(&oracle) == "idle");
    }
}

// The oracle predicates below walked Character::inv_const_slice() and looked at
// front() of each stack: the flat inventory, top level only. Since routing gives
// worn pockets first refusal on anything the character acquires, that is no
// longer where a picked-up lighter lives, and an NPC would sit in the cold next
// to its own firestarter. See SESSION_HANDOFF_2026-09-05.md, open finding 3.
TEST_CASE("npc_behavior_reads_items_held_in_pockets", "[npc][behavior][pocket]") {
    clear_all_state();
    npc& test_npc = spawn_npc({50, 50, 0}, "test_talker");
    clear_character(test_npc);
    behavior::character_oracle_t oracle(&test_npc);

    // A backpack worn on the back is the ordinary destination routing picks.
    test_npc.wear_item(item::spawn(itype_id("backpack")));

    SECTION("firestarter and fuel routed into a worn pocket") {
        REQUIRE(oracle.can_make_fire() == behavior::status_t::success);

        detached_ptr<item> lighter = item::spawn(itype_id("lighter"));
        item& lighter_ref = *lighter;
        detached_ptr<item> fuel = item::spawn(itype_id("2x4"));
        item& fuel_ref = *fuel;
        // quiet, and no prompt: this is the non-interactive routing path.
        REQUIRE(!test_npc.i_add_to_worn_pockets(std::move(lighter), nullptr, true, false));
        REQUIRE(!test_npc.i_add_to_worn_pockets(std::move(fuel), nullptr, true, false));

        // Assert the precondition rather than trusting it: both items must have
        // gone into a pocket, not into the flat inventory, or the test proves
        // nothing about pocketed items.
        REQUIRE(test_npc.inv_position_by_item(&lighter_ref) == INT_MIN);
        REQUIRE(test_npc.inv_position_by_item(&fuel_ref) == INT_MIN);

        CHECK(oracle.can_make_fire() == behavior::status_t::running);
    }

    SECTION("warmer clothes routed into a worn pocket") {
        REQUIRE(oracle.can_wear_warmer_clothes() == behavior::status_t::failure);

        detached_ptr<item> sweater = item::spawn(itype_id("sweater"));
        item& sweater_ref = *sweater;
        REQUIRE(!test_npc.i_add_to_worn_pockets(std::move(sweater), nullptr, true, false));
        REQUIRE(test_npc.inv_position_by_item(&sweater_ref) == INT_MIN);

        CHECK(oracle.can_wear_warmer_clothes() == behavior::status_t::running);
    }

    SECTION("a firestarter nested two containers deep is still found") {
        detached_ptr<item> bag = item::spawn(itype_id("bag_plastic"));
        item& bag_ref = *bag;
        REQUIRE(!bag_ref.put_in(item::spawn(itype_id("lighter"))));
        REQUIRE(!bag_ref.put_in(item::spawn(itype_id("rag"))));
        REQUIRE(!test_npc.i_add_to_worn_pockets(std::move(bag), nullptr, true, false));
        REQUIRE(test_npc.inv_position_by_item(&bag_ref) == INT_MIN);

        CHECK(oracle.can_make_fire() == behavior::status_t::running);
    }
}
