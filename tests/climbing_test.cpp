#include "catch/catch.hpp"

#include "climbing.h"
#include "state_helpers.h"
#include "type_id.h"

#include <string>
#include <vector>

// The climbing aids are content: a typo in an id or a flag fails silently, because
// an aid that never matches simply never appears in the climb-down menu.  These
// tests exist to make that silence noisy.

TEST_CASE("climbing aid data loads", "[climbing]") {
    clear_all_state();
    REQUIRE(!climbing_aid::get_all().empty());

    // The default is what you fall back on with nothing to hold, and the code
    // dereferences it unconditionally.
    const climbing_aid &fallback = climbing_aid::get_default();
    CHECK(fallback.id.str() == "default");
    CHECK(fallback.was_loaded);
    CHECK(fallback.slip_chance_mod == 0);
    CHECK(!fallback.down.deploys_furniture());
}

TEST_CASE("every climbing aid names things that exist", "[climbing]") {
    clear_all_state();
    int item_aids = 0;
    int trait_aids = 0;
    int deploying_aids = 0;

    for (const climbing_aid &aid : climbing_aid::get_all()) {
        INFO("climbing aid: " << aid.id.str());

        if (aid.down.deploys_furniture()) {
            ++deploying_aids;
            CHECK(aid.down.deploy_furn.is_valid());
        }

        switch (aid.base_condition.cat) {
            case climbing_aid::category::item:
                ++item_aids;
                CHECK(itype_id(aid.base_condition.flag).is_valid());
                // An item aid that consumes nothing would be free to use forever.
                CHECK(aid.base_condition.uses_item > 0);
                break;
            case climbing_aid::category::trait:
                ++trait_aids;
                CHECK(trait_id(aid.base_condition.flag).is_valid());
                break;
            default:
                break;
        }
    }

    // Guard against the checks above passing because nothing was examined.
    CHECK(item_aids > 0);
    CHECK(trait_aids > 0);
    CHECK(deploying_aids > 0);
}

TEST_CASE("the safest available aid is the one chosen", "[climbing]") {
    clear_all_state();
    const climbing_aid &fallback = climbing_aid::get_default();

    const climbing_aid::condition_list ladder = {
        { climbing_aid::category::ter_furn, "LADDER" }
    };
    const climbing_aid &safest = climbing_aid::get_safest(ladder, true);
    INFO("chose: " << safest.id.str());
    CHECK(safest.id != fallback.id);
    CHECK(safest.slip_chance_mod < fallback.slip_chance_mod);
    // Asked for no deployables, it must not answer with one.
    CHECK(!safest.down.deploys_furniture());

    // Nothing to hold on to leaves only the bare ledge.
    const climbing_aid::condition_list nothing = {
        { climbing_aid::category::ter_furn, "NO_SUCH_CLIMBING_FLAG" }
    };
    CHECK(climbing_aid::get_safest(nothing, true).id == fallback.id);
}

TEST_CASE("a deployable aid is offered alongside the safest", "[climbing]") {
    clear_all_state();

    const climbing_aid::condition_list grapnel = {
        { climbing_aid::category::item, "grapnel" }
    };
    const climbing_aid::aid_list offered = climbing_aid::list(grapnel);
    REQUIRE(!offered.empty());

    bool offers_deployable = false;
    for (const climbing_aid *aid : offered) {
        REQUIRE(aid != nullptr);
        if (aid->down.deploys_furniture()) { offers_deployable = true; }
    }
    CHECK(offers_deployable);

    // list() appends the safest non-deploying option last, so the player always has
    // a way down that leaves nothing behind.
    CHECK(!offered.back()->down.deploys_furniture());
}
