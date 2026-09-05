#include <algorithm>
#include <climits>
#include <string>
#include <vector>

#include "advanced_inv_area.h"
#include "advanced_inv_listitem.h"
#include "advanced_inv_pane.h"
#include "avatar.h"
#include "catch/catch.hpp"
#include "detached_ptr.h"
#include "inventory_ui.h"
#include "item.h"
#include "item_contents.h"
#include "item_pocket.h"
#include "itype.h"
#include "options_helpers.h"
#include "player_helpers.h"
#include "pocket_destination_menu.h"
#include "type_id.h"

TEST_CASE(
    "inventory selector restores consume selection by item type", "[inventory][ui][consume]") {
    clear_avatar();
    auto& dummy = get_avatar();

    auto bandages = item::spawn("bandages");
    auto heroin = item::spawn("heroin");

    auto selector = inventory_selector(dummy);
    selector.add_item(selector.own_inv_column, heroin.get());
    selector.add_item(selector.own_inv_column, bandages.get());

    REQUIRE(selector.select_item_type(bandages->typeId()));
    CHECK(selector.get_selected().any_item()->typeId() == bandages->typeId());
}

TEST_CASE(
    "inventory selector restores saved position before same type in another column",
    "[inventory][ui][consume]") {
    clear_avatar();
    auto& dummy = get_avatar();

    auto inventory_bandages = item::spawn("bandages");
    auto map_bandages = item::spawn("bandages");

    auto selector = inventory_selector(dummy);
    selector.add_item(selector.own_inv_column, inventory_bandages.get());
    selector.add_item(selector.map_column, map_bandages.get());

    REQUIRE(selector.select_item_type(map_bandages->typeId(), 1));
    const auto saved_position = selector.get_selection_position();
    REQUIRE(selector.select(inventory_bandages.get()));

    REQUIRE(selector.restore_selection(saved_position, map_bandages->typeId()));
    CHECK(selector.get_selected().any_item() == map_bandages.get());
}

TEST_CASE(
    "inventory selector rejects saved position when the type changed", "[inventory][ui][consume]") {
    clear_avatar();
    auto& dummy = get_avatar();

    auto inventory_bandages = item::spawn("bandages");
    auto map_heroin = item::spawn("heroin");

    auto selector = inventory_selector(dummy);
    selector.add_item(selector.own_inv_column, inventory_bandages.get());
    selector.add_item(selector.map_column, map_heroin.get());

    REQUIRE(selector.select_item_type(map_heroin->typeId(), 1));
    const auto stale_position = selector.get_selection_position();
    REQUIRE(selector.select(inventory_bandages.get()));

    CHECK_FALSE(
        selector.select_position_if_item_type(stale_position, inventory_bandages->typeId()));
    CHECK(selector.get_selected().any_item() == inventory_bandages.get());
}

TEST_CASE(
    "inventory selector restores saved row before same type fallback in the same column",
    "[inventory][ui][consume]") {
    clear_avatar();
    auto& dummy = get_avatar();

    auto inventory_bandages = item::spawn("bandages");
    auto first_map_bandages = item::spawn("bandages");
    auto second_map_bandages = item::spawn("bandages");

    auto selector = inventory_selector(dummy);
    selector.add_item(selector.own_inv_column, inventory_bandages.get());
    selector.add_item(selector.map_column, first_map_bandages.get());
    selector.add_item(selector.map_column, second_map_bandages.get());

    REQUIRE(selector.select_item_type(first_map_bandages->typeId(), 1));
    REQUIRE(selector.select(second_map_bandages.get()));
    const auto saved_position = selector.get_selection_position();
    REQUIRE(selector.select(inventory_bandages.get()));

    REQUIRE(selector.restore_selection(saved_position, second_map_bandages->typeId()));
    CHECK(selector.get_selected().any_item() == second_map_bandages.get());
}

TEST_CASE(
    "inventory selector type fallback starts in the saved column", "[inventory][ui][consume]") {
    clear_avatar();
    auto& dummy = get_avatar();

    auto inventory_bandages = item::spawn("bandages");
    auto map_heroin = item::spawn("heroin");
    auto map_bandages = item::spawn("bandages");

    auto selector = inventory_selector(dummy);
    selector.add_item(selector.own_inv_column, inventory_bandages.get());
    selector.add_item(selector.map_column, map_heroin.get());
    selector.add_item(selector.map_column, map_bandages.get());

    REQUIRE(selector.select_item_type(map_heroin->typeId(), 1));
    const auto stale_position = selector.get_selection_position();
    REQUIRE(selector.select(inventory_bandages.get()));

    REQUIRE(selector.restore_selection(stale_position, map_bandages->typeId()));
    CHECK(selector.get_selected().any_item() == map_bandages.get());
}

TEST_CASE(
    "inventory selector type fallback leaves saved column when needed",
    "[inventory][ui][consume]") {
    clear_avatar();
    auto& dummy = get_avatar();

    auto inventory_bandages = item::spawn("bandages");
    auto map_heroin = item::spawn("heroin");

    auto selector = inventory_selector(dummy);
    selector.add_item(selector.own_inv_column, inventory_bandages.get());
    selector.add_item(selector.map_column, map_heroin.get());

    REQUIRE(selector.select_item_type(map_heroin->typeId(), 1));
    const auto stale_position = selector.get_selection_position();

    REQUIRE(selector.restore_selection(stale_position, inventory_bandages->typeId()));
    CHECK(selector.get_selected().any_item() == inventory_bandages.get());
}

TEST_CASE(
    "inventory selector type fallback resumes default order after saved column",
    "[inventory][ui][consume]") {
    clear_avatar();
    auto& dummy = get_avatar();

    auto inventory_bandages = item::spawn("bandages");
    auto gear_bandages = item::spawn("bandages");
    auto map_heroin = item::spawn("heroin");

    auto selector = inventory_selector(dummy);
    selector.add_item(selector.own_inv_column, inventory_bandages.get());
    selector.add_item(selector.own_gear_column, gear_bandages.get());
    selector.add_item(selector.map_column, map_heroin.get());

    REQUIRE(selector.select_item_type(map_heroin->typeId(), 1));
    const auto stale_position = selector.get_selection_position();

    REQUIRE(selector.restore_selection(stale_position, inventory_bandages->typeId()));
    CHECK(selector.get_selected().any_item() == inventory_bandages.get());
}

TEST_CASE(
    "inventory selector does not restore a stale row when saved type is gone",
    "[inventory][ui][consume]") {
    clear_avatar();
    auto& dummy = get_avatar();

    auto inventory_aspirin = item::spawn("aspirin");
    auto inventory_heroin = item::spawn("heroin");
    auto map_heroin = item::spawn("heroin");
    auto missing_bandages = item::spawn("bandages");

    auto selector = inventory_selector(dummy);
    selector.add_item(selector.own_inv_column, inventory_aspirin.get());
    selector.add_item(selector.own_inv_column, inventory_heroin.get());
    selector.add_item(selector.map_column, map_heroin.get());

    REQUIRE(selector.select_item_type(map_heroin->typeId(), 1));
    const auto stale_position = selector.get_selection_position();

    selector.clear_items();
    selector.add_item(selector.own_inv_column, inventory_aspirin.get());
    selector.add_item(selector.own_inv_column, inventory_heroin.get());
    selector.add_item(selector.map_column, map_heroin.get());
    REQUIRE(selector.select_item_type(map_heroin->typeId(), 1));
    const auto expected_entries = selector.own_inv_column.get_entries([](const auto& entry) {
        return entry.is_selectable();
    });
    const auto expected_default = expected_entries.front()->any_item();

    CHECK_FALSE(selector.restore_selection(stale_position, missing_bandages->typeId()));
    CHECK(selector.get_selected().any_item() == expected_default);
}

TEST_CASE("inventory selector falls back from out of range saved row", "[inventory][ui][consume]") {
    clear_avatar();
    auto& dummy = get_avatar();

    auto inventory_bandages = item::spawn("bandages");
    auto map_bandages = item::spawn("bandages");

    auto selector = inventory_selector(dummy);
    selector.add_item(selector.own_inv_column, inventory_bandages.get());
    selector.add_item(selector.map_column, map_bandages.get());

    REQUIRE(selector.select_item_type(map_bandages->typeId(), 1));
    auto invalid_position = selector.get_selection_position();
    invalid_position.second = 999;
    REQUIRE(selector.select(inventory_bandages.get()));

    REQUIRE(selector.restore_selection(invalid_position, map_bandages->typeId()));
    CHECK(selector.get_selected().any_item() == map_bandages.get());
}

TEST_CASE(
    "inventory selector falls back from invalid saved consume column", "[inventory][ui][consume]") {
    clear_avatar();
    auto& dummy = get_avatar();

    auto inventory_bandages = item::spawn("bandages");
    auto map_bandages = item::spawn("bandages");

    auto selector = inventory_selector(dummy);
    selector.add_item(selector.own_inv_column, inventory_bandages.get());
    selector.add_item(selector.map_column, map_bandages.get());

    REQUIRE(selector.select(inventory_bandages.get()));
    const auto invalid_position = std::pair<size_t, size_t>{99, 0};

    REQUIRE(selector.restore_selection(invalid_position, map_bandages->typeId()));
    CHECK(selector.get_selected().any_item() == inventory_bandages.get());
}

TEST_CASE(
    "inventory selector populates the name cache and sorts deterministically",
    "[inventory][ui][consume]") {
    clear_avatar();
    auto& dummy = get_avatar();

    // Two comestibles share the "food" category, so their relative order is
    // decided purely by the name tiebreak in the sort comparator.
    auto orange_soda = item::spawn("orangesoda");
    auto maple_syrup = item::spawn("syrup");
    REQUIRE(orange_soda->tname(1) != maple_syrup->tname(1));

    auto selector = inventory_selector(dummy);
    // Add in non-alphabetical order so the sort has to reorder them.
    selector.add_item(selector.own_inv_column, orange_soda.get());
    selector.add_item(selector.own_inv_column, maple_syrup.get());

    // Triggers prepare_paging(): refreshes cached_name and sorts the column.
    REQUIRE(selector.select_item_type(maple_syrup->typeId()));

    const auto selectable = [](const auto& entry) { return entry.is_selectable(); };
    const auto entries = selector.own_inv_column.get_entries(selectable);
    REQUIRE(entries.size() == 2);
    // Precondition: both items must share a category so the name comparison (not the
    // category sort) decides their order; otherwise the order check below could pass
    // via category ordering and silently stop covering the regression.
    REQUIRE(entries.front()->get_category_ptr() == entries.back()->get_category_ptr());

    // Regression for #9713: PR #9038 dropped the only update_cache() call, leaving
    // cached_name empty so the alphabetic tiebreak was dead and equal-keyed comestibles
    // sorted in an unspecified order that differs across stdlib implementations.
    REQUIRE_FALSE(entries.front()->cached_name.empty());
    CHECK(entries.front()->cached_name == entries.front()->any_item()->tname(1));

    // The column is now ordered by name deterministically.
    CHECK(entries.front()->any_item()->tname(1) < entries.back()->any_item()->tname(1));
}

// Pickup and unload both route items into worn pockets, but no inventory screen
// rendered a pocket, so a routed item was invisible and unreachable. These cover
// the nested display that makes it visible again.
TEST_CASE("worn pocket contents appear nested under the garment", "[inventory][ui][pocket]") {
    clear_avatar();
    auto& dummy = get_avatar();
    REQUIRE(!dummy.wear_item(item::spawn("test_pocket_vest")));
    item* vest = dummy.worn.front();
    REQUIRE(!vest->put_in(item::spawn("test_rock")));

    auto selector = inventory_selector(dummy);
    selector.add_character_items(dummy);

    const auto is_rock = [](const inventory_entry& entry) {
        return entry.is_item() && entry.any_item()->typeId() == itype_id("test_rock");
    };
    const auto entries = selector.own_gear_column.get_all_entries(is_rock);
    REQUIRE(entries.size() == 1);
    CHECK(entries.front()->topmost_parent == vest);
    CHECK(entries.front()->indent == 1);
}

// Classic mode pools storage into one compartment and must look exactly like
// stock BN, so nothing nests there.
TEST_CASE("classic mode nests nothing", "[inventory][ui][pocket][classic]") {
    override_option classic("POCKET_SYSTEM", "classic");
    clear_avatar();
    auto& dummy = get_avatar();
    REQUIRE(!dummy.wear_item(item::spawn("test_pocket_vest")));
    item* vest = dummy.worn.front();
    REQUIRE(!vest->put_in(item::spawn("test_rock")));

    auto selector = inventory_selector(dummy);
    selector.add_character_items(dummy);

    const auto is_rock = [](const inventory_entry& entry) {
        return entry.is_item() && entry.any_item()->typeId() == itype_id("test_rock");
    };
    CHECK(selector.own_gear_column.get_all_entries(is_rock).empty());
}

// The payoff of BN's raw pointers over CDDA's item_location: a nested entry is
// already an item*, so every existing action path works on it unchanged.
TEST_CASE("an item in a worn pocket can be selected", "[inventory][ui][pocket]") {
    clear_avatar();
    auto& dummy = get_avatar();
    REQUIRE(!dummy.wear_item(item::spawn("test_pocket_vest")));
    item* vest = dummy.worn.front();
    REQUIRE(!vest->put_in(item::spawn("test_rock")));
    item* rock = vest->contents.all_items_top().front();

    auto selector = inventory_selector(dummy);
    selector.add_character_items(dummy);

    REQUIRE(selector.select(rock));
    CHECK(selector.get_selected().any_item() == rock);
}

// A garment holding nothing must not sprout an empty nested row.
TEST_CASE("an empty worn pocket nests nothing", "[inventory][ui][pocket]") {
    clear_avatar();
    auto& dummy = get_avatar();
    REQUIRE(!dummy.wear_item(item::spawn("test_pocket_vest")));

    auto selector = inventory_selector(dummy);
    selector.add_character_items(dummy);

    const auto nested = [](const inventory_entry& entry) { return entry.indent > 0; };
    CHECK(selector.own_gear_column.get_all_entries(nested).empty());
}

// prepare_paging() sorts a column by category then name. Nested entries share
// their garment's category, so the sort pulls children away from the container
// they belong to ("TEST pocket vest" sorts before "TEST rock", so both rocks
// would end up below both garments). A post-sort pass lifts them back.
// A container carried rather than worn is where a taken-off garment lands, and
// its contents were listed nowhere: the entry showed a count with nothing to
// open under it, so the items inside were held but unreachable.
TEST_CASE("a carried container's contents appear nested under it", "[inventory][ui][pocket]") {
    clear_avatar();
    auto& dummy = get_avatar();
    auto jeans = item::spawn("jeans");
    REQUIRE(!jeans->put_in(item::spawn("test_rock")));
    item& carried = dummy.i_add(std::move(jeans));
    REQUIRE(carried.typeId() == itype_id("jeans"));
    REQUIRE(carried.contents.all_items_top().size() == 1);

    auto selector = inventory_selector(dummy);
    selector.add_character_items(dummy);

    const auto is_rock = [](const inventory_entry& entry) {
        return entry.is_item() && entry.any_item()->typeId() == itype_id("test_rock");
    };
    const auto entries = selector.own_inv_column.get_all_entries(is_rock);
    REQUIRE(entries.size() == 1);
    CHECK(entries.front()->topmost_parent == &carried);
    CHECK(entries.front()->indent == 1);
}

TEST_CASE("classic mode nests nothing in a carried container",
          "[inventory][ui][pocket][classic]") {
    override_option classic("POCKET_SYSTEM", "classic");
    clear_avatar();
    auto& dummy = get_avatar();
    auto jeans = item::spawn("jeans");
    REQUIRE(!jeans->put_in(item::spawn("test_rock")));
    dummy.i_add(std::move(jeans));

    auto selector = inventory_selector(dummy);
    selector.add_character_items(dummy);

    const auto is_nested = [](const inventory_entry& entry) {
        return entry.is_item() && entry.indent > 0;
    };
    CHECK(selector.own_inv_column.get_all_entries(is_nested).empty());
}

TEST_CASE("nested entries stay under their own container", "[inventory][ui][pocket]") {
    clear_avatar();
    auto& dummy = get_avatar();
    REQUIRE(!dummy.wear_item(item::spawn("test_pocket_vest")));
    REQUIRE(!dummy.wear_item(item::spawn("backpack")));

    item* vest = nullptr;
    item* pack = nullptr;
    for (item* worn : dummy.worn) {
        (worn->typeId() == itype_id("test_pocket_vest") ? vest : pack) = worn;
    }
    REQUIRE(vest != nullptr);
    REQUIRE(pack != nullptr);
    REQUIRE(!vest->put_in(item::spawn("test_rock")));
    REQUIRE(!pack->put_in(item::spawn("test_ear_plugs")));

    auto selector = inventory_selector(dummy);
    selector.add_character_items(dummy);
    // select() alone leaves the entries in insertion order, which is already
    // parent-then-child and would make this test vacuous. select_item_type()
    // triggers prepare_paging(), which is what actually sorts the column.
    REQUIRE(selector.select_item_type(itype_id("test_rock")));

    const auto is_item = [](const inventory_entry& entry) { return entry.is_item(); };
    const auto entries = selector.own_gear_column.get_entries(is_item);

    // Every child must sit immediately after its own parent.
    for (size_t i = 0; i < entries.size(); ++i) {
        item* parent = entries[i]->topmost_parent;
        if (parent == nullptr) {
            continue;
        }
        REQUIRE(i > 0);
        const inventory_entry& before = *entries[i - 1];
        const bool follows_parent = before.any_item() == parent
            || before.topmost_parent == parent;
        CHECK(follows_parent);
    }
    // And both children must actually be present, or the loop above is vacuous.
    const auto nested = [](const inventory_entry& entry) { return entry.indent > 0; };
    CHECK(selector.own_gear_column.get_entries(nested).size() == 2);
}

// ---------------------------------------------------------------------------
// Pocket contents in the category list
// ---------------------------------------------------------------------------

// get_caption is protected; a derived preset widens it for these tests only.
struct caption_probe : inventory_selector_preset {
    using inventory_selector_preset::get_caption;
};

static bool is_test_rock(const inventory_entry& entry) {
    return entry.is_item() && entry.any_item()->typeId() == itype_id("test_rock");
}

TEST_CASE("pocket contents also appear under their own category", "[inventory][ui][pocket]") {
    clear_avatar();
    auto& dummy = get_avatar();
    REQUIRE(!dummy.wear_item(item::spawn("test_pocket_vest")));
    item* vest = dummy.worn.front();
    REQUIRE(!vest->put_in(item::spawn("test_rock")));

    auto selector = inventory_selector(dummy);
    selector.add_character_items(dummy);

    const auto nested = selector.own_gear_column.get_all_entries(is_test_rock);
    REQUIRE(nested.size() == 1);
    CHECK(nested.front()->indent == 1);

    // The same rock, listed again on the left under the category it declares.
    const auto listed = selector.own_inv_column.get_all_entries(is_test_rock);
    REQUIRE(listed.size() == 1);
    CHECK(listed.front()->indent == 0);
    CHECK(listed.front()->topmost_parent == vest);
    CHECK(listed.front()->any_item() == nested.front()->any_item());
}

TEST_CASE("a pocketed item names its container in the category list",
          "[inventory][ui][pocket]") {
    clear_avatar();
    auto& dummy = get_avatar();
    REQUIRE(!dummy.wear_item(item::spawn("test_pocket_vest")));
    item* vest = dummy.worn.front();
    REQUIRE(!vest->put_in(item::spawn("test_rock")));

    auto selector = inventory_selector(dummy);
    selector.add_character_items(dummy);

    const auto listed = selector.own_inv_column.get_all_entries(is_test_rock);
    REQUIRE(listed.size() == 1);

    caption_probe preset;
    const std::string vest_name = vest->type->nname(1);
    const std::string caption = preset.get_caption(*listed.front());
    INFO(caption);
    CHECK(caption.find(vest_name) != std::string::npos);
    // The bare type name, with no "with 1 item" contents summary trailing it.
    CHECK(caption.find(vest_name + " with") == std::string::npos);

    // The nested copy is drawn under its parent already, so it must not repeat
    // it. Its own name carries brackets of its own, so match the vest's name.
    const auto nested = selector.own_gear_column.get_all_entries(is_test_rock);
    REQUIRE(nested.size() == 1);
    const std::string nested_caption = preset.get_caption(*nested.front());
    INFO(nested_caption);
    CHECK(nested_caption.find(vest_name) == std::string::npos);
}

TEST_CASE("classic mode adds no category copy", "[inventory][ui][pocket][classic]") {
    override_option classic("POCKET_SYSTEM", "classic");
    clear_avatar();
    auto& dummy = get_avatar();
    REQUIRE(!dummy.wear_item(item::spawn("test_pocket_vest")));
    item* vest = dummy.worn.front();
    REQUIRE(!vest->put_in(item::spawn("test_rock")));

    auto selector = inventory_selector(dummy);
    selector.add_character_items(dummy);

    CHECK(selector.own_inv_column.get_all_entries(is_test_rock).empty());
}

TEST_CASE("the category copy keeps the item's own category", "[inventory][ui][pocket]") {
    clear_avatar();
    auto& dummy = get_avatar();
    REQUIRE(!dummy.wear_item(item::spawn("test_pocket_vest")));
    item* vest = dummy.worn.front();
    REQUIRE(!vest->put_in(item::spawn("test_rock")));

    auto selector = inventory_selector(dummy);
    selector.add_character_items(dummy);
    // Force prepare_paging. The reordering pass used to key on parentage, which
    // swept this entry to the end of the column as an orphan of a parent that
    // lives in a different column.
    REQUIRE(selector.select_item_type(itype_id("test_rock")));

    const auto listed = selector.own_inv_column.get_all_entries(is_test_rock);
    REQUIRE(listed.size() == 1);
    CHECK(listed.front()->get_category_ptr() ==
          &listed.front()->any_item()->get_category());
}

TEST_CASE("the copy the cursor is not on echoes the highlight", "[inventory][ui][pocket]") {
    clear_avatar();
    auto& dummy = get_avatar();
    REQUIRE(!dummy.wear_item(item::spawn("test_pocket_vest")));
    item* vest = dummy.worn.front();
    REQUIRE(!vest->put_in(item::spawn("test_rock")));

    auto selector = inventory_selector(dummy);
    selector.add_character_items(dummy);

    const auto listed = selector.own_inv_column.get_all_entries(is_test_rock);
    const auto nested = selector.own_gear_column.get_all_entries(is_test_rock);
    REQUIRE(listed.size() == 1);
    REQUIRE(nested.size() == 1);

    // Cursor on the nested copy: the category-list copy is its companion.
    selector.own_inv_column.set_companion_item(nested.front()->any_item());
    CHECK(selector.own_inv_column.is_companion(*listed.front()));

    // The garment holding it is a different item and must not echo.
    const auto is_vest = [](const inventory_entry& entry) {
        return entry.is_item() && entry.any_item()->typeId() == itype_id("test_pocket_vest");
    };
    for (const inventory_entry* entry : selector.own_gear_column.get_all_entries(is_vest)) {
        CHECK_FALSE(selector.own_gear_column.is_companion(*entry));
    }

    // With no cursor item, nothing echoes.
    selector.own_inv_column.set_companion_item(nullptr);
    CHECK_FALSE(selector.own_inv_column.is_companion(*listed.front()));
}

TEST_CASE("pocket destinations list every pocket that would take the item",
          "[inventory][pocket][destination]") {
    clear_avatar();
    auto& dummy = get_avatar();
    REQUIRE(!dummy.wear_item(item::spawn("test_pocket_vest")));
    REQUIRE(!dummy.wear_item(item::spawn("backpack")));
    item* vest = dummy.worn.front();
    item* pack = dummy.worn.back();

    auto rock = item::spawn("test_rock");
    const auto destinations = dummy.pocket_destinations(*rock);

    // The vest's small pocket (100 ml) is too small for the 250 ml rock and
    // must not appear; its large pocket and all four of the backpack's
    // pockets (25 L main plus three holsters, none item-restricted) do fit
    // and must all appear - exactly five, no more, no fewer.
    REQUIRE(destinations.size() == 5);
    std::vector<std::pair<item*, size_t>> found;
    for (const pocket_destination& dest : destinations) {
        found.emplace_back(dest.container, dest.pocket_index);
    }
    const auto has = [&found](item* container, size_t pocket_index) {
        for (const std::pair<item*, size_t>& entry : found) {
            if (entry.first == container && entry.second == pocket_index) {
                return true;
            }
        }
        return false;
    };
    CHECK(has(vest, 1));
    CHECK(has(pack, 0));
    CHECK(has(pack, 1));
    CHECK(has(pack, 2));
    CHECK(has(pack, 3));
}

TEST_CASE("pocket destinations rank higher player priority first",
          "[inventory][pocket][destination]") {
    clear_avatar();
    auto& dummy = get_avatar();
    REQUIRE(!dummy.wear_item(item::spawn("test_pocket_vest")));
    REQUIRE(!dummy.wear_item(item::spawn("backpack")));
    item* pack = dummy.worn.back();

    // The backpack's main pocket (25 L) is far roomier than the vest's large
    // pocket (4 L), so on remaining volume alone it would sort last, not
    // first. Raising its priority must still put it first.
    pack->contents.get_pockets()[0].get_settings().set_priority(5);

    auto rock = item::spawn("test_rock");
    const auto destinations = dummy.pocket_destinations(*rock);
    REQUIRE(!destinations.empty());
    CHECK(destinations.front().container == pack);
    CHECK(destinations.front().pocket_index == 0);
}

TEST_CASE("pocket destinations break a priority tie by smaller remaining volume",
          "[inventory][pocket][destination]") {
    clear_avatar();
    auto& dummy = get_avatar();
    REQUIRE(!dummy.wear_item(item::spawn("test_pocket_vest")));
    REQUIRE(!dummy.wear_item(item::spawn("backpack")));
    // worn is ordered by clothing layer, not by wear_item() call order - the
    // backpack's BELTED flag puts it outside the vest's regular layer no
    // matter which is worn first, so the vest's pockets are always pushed
    // into pocket_destinations' pre-sort vector before the backpack's.
    item* vest = dummy.worn.front();
    item* pack = dummy.worn.back();

    // Tie priority between the vest's large pocket (4 L, pushed first) and
    // the backpack's small holster (510 ml, pushed after it). Push order and
    // the expected volume order disagree here - vest's larger pocket is
    // pushed first but must sort *second* once the tie-break is honoured -
    // so this is the pairing that actually distinguishes "break ties by
    // volume" from "break ties by push order"; the vest's own main pocket
    // versus the backpack's main pocket would not, since push order already
    // agrees with volume order for that pair. Do not swap this pairing back
    // to the two main pockets - that version stays green even without the
    // volume tie-break.
    vest->contents.get_pockets()[1].get_settings().set_priority(3);
    pack->contents.get_pockets()[1].get_settings().set_priority(3);

    auto rock = item::spawn("test_rock");
    const auto destinations = dummy.pocket_destinations(*rock);
    REQUIRE(destinations.size() >= 2);
    CHECK(destinations[0].container == pack);
    CHECK(destinations[0].pocket_index == 1);
    CHECK(destinations[1].container == vest);
    CHECK(destinations[1].pocket_index == 1);
}

TEST_CASE("pocket destinations skip the excluded container",
          "[inventory][pocket][destination]") {
    clear_avatar();
    auto& dummy = get_avatar();
    REQUIRE(!dummy.wear_item(item::spawn("test_pocket_vest")));
    item* vest = dummy.worn.front();

    auto rock = item::spawn("test_rock");
    for (const pocket_destination& dest : dummy.pocket_destinations(*rock, vest)) {
        CHECK(dest.container != vest);
    }
}

TEST_CASE("pocket destinations reach a container nested in a worn pocket",
          "[inventory][pocket][destination][nesting]") {
    clear_avatar();
    auto& dummy = get_avatar();
    REQUIRE(!dummy.wear_item(item::spawn("test_pocket_vest")));
    item* vest = dummy.worn.front();

    // A wallet carried in the vest's large pocket, which is how a player has
    // one: not worn, not wielded, just sitting in a pocket. Its sleeves are
    // still somewhere an item can go, so the pocket manager must offer them.
    REQUIRE(!vest->put_in(item::spawn("wallet")));
    item* wallet = vest->contents.all_items_top().front();
    REQUIRE(wallet->typeId() == itype_id("wallet"));

    auto coin = item::spawn("coin_quarter");
    bool offered = false;
    for (const pocket_destination& dest : dummy.pocket_destinations(*coin)) {
        if (dest.container == wallet) {
            offered = true;
        }
    }
    CHECK(offered);
}

TEST_CASE("pocket destinations never offer a pocket inside the item being moved",
          "[inventory][pocket][destination][nesting]") {
    clear_avatar();
    auto& dummy = get_avatar();
    REQUIRE(!dummy.wear_item(item::spawn("test_pocket_vest")));
    item* vest = dummy.worn.front();

    REQUIRE(!vest->put_in(item::spawn("wallet")));
    item* wallet = vest->contents.all_items_top().front();

    // Moving the wallet into its own sleeve would detach it from the world.
    for (const pocket_destination& dest : dummy.pocket_destinations(*wallet, wallet)) {
        CHECK(dest.container != wallet);
    }
}

TEST_CASE("pocket destinations exclude the whole subtree of the item being moved",
          "[inventory][pocket][destination][nesting]") {
    clear_avatar();
    auto& dummy = get_avatar();
    REQUIRE(!dummy.wear_item(item::spawn("test_pocket_vest")));
    item* vest = dummy.worn.front();

    // vest -> bag -> wallet. Moving the bag must not offer the wallet either:
    // the exclusion is a subtree, not a single item.
    auto bag = item::spawn("bag_plastic");
    REQUIRE(!bag->put_in(item::spawn("wallet")));
    REQUIRE(!vest->put_in(std::move(bag)));
    item* outer = vest->contents.all_items_top().front();
    REQUIRE(outer->typeId() == itype_id("bag_plastic"));
    item* inner = outer->contents.all_items_top().front();
    REQUIRE(inner->typeId() == itype_id("wallet"));

    for (const pocket_destination& dest : dummy.pocket_destinations(*outer, outer)) {
        CHECK(dest.container != outer);
        CHECK(dest.container != inner);
    }
}

TEST_CASE("a single pocket destination needs no menu",
          "[inventory][pocket][destination]") {
    clear_avatar();
    auto& dummy = get_avatar();
    REQUIRE(!dummy.wear_item(item::spawn("test_pocket_vest")));

    // The vest's small pocket is 100 ml and the rock is 250 ml, so exactly one
    // pocket takes it. The move menu is offered whenever any pocket would, so
    // declining here left the entry doing nothing at all.
    auto rock = item::spawn("test_rock");
    REQUIRE(dummy.pocket_destinations(*rock).size() == 1);

    const auto dest = ask_pocket_destination(dummy, *rock);
    REQUIRE(dest.has_value());
    CHECK(dest->container == dummy.worn.front());
    CHECK(dest->pocket_index == 1);
}

TEST_CASE("classic mode offers no pocket destinations",
          "[inventory][pocket][destination][classic]") {
    override_option classic("POCKET_SYSTEM", "classic");
    clear_avatar();
    auto& dummy = get_avatar();
    REQUIRE(!dummy.wear_item(item::spawn("test_pocket_vest")));

    auto rock = item::spawn("test_rock");
    CHECK(dummy.pocket_destinations(*rock).empty());
}

TEST_CASE("the pickup prompt is off by default and follows the option",
          "[inventory][pocket][option]") {
    CHECK_FALSE(pockets_prompt_on_pickup());

    {
        override_option choose("POCKET_PICKUP", "choose");
        CHECK(pockets_prompt_on_pickup());
    }

    // Classic mode ignores pockets, so it must ignore the prompt too.
    {
        override_option choose("POCKET_PICKUP", "choose");
        override_option classic("POCKET_SYSTEM", "classic");
        CHECK_FALSE(pockets_prompt_on_pickup());
    }
}

// stow_loose_inventory_into_pockets makes exactly this call (quiet=true,
// allow_prompt=false). It must never block on a menu regardless of the
// world option - there is no game yet for a new character to be asked
// anything in - and the item must still end up routed, not lost.
TEST_CASE("character creation routes into pockets without prompting even when choose is set",
          "[inventory][pocket][option]") {
    override_option choose("POCKET_PICKUP", "choose");
    clear_avatar();
    auto& dummy = get_avatar();
    REQUIRE(!dummy.wear_item(item::spawn("test_pocket_vest")));
    item* vest = dummy.worn.front();

    // quiet=false deliberately: quiet only suppresses the "you put it away"
    // message. allow_prompt is what must hold the prompt back, and passing
    // quiet=true here would let the old `!quiet &&` gate pass this test too.
    auto rock = item::spawn("test_rock");
    detached_ptr<item> refused =
        dummy.i_add_to_worn_pockets(std::move(rock), nullptr, false, false);
    CHECK_FALSE(refused);
    CHECK(vest->contents.all_items_top().size() == 1);
}

// A container must never be offered its own pockets: putting an item inside
// itself detaches it from the world entirely.
TEST_CASE("a container is not a destination for itself", "[inventory][pocket][destination]") {
    clear_avatar();
    auto& dummy = get_avatar();
    REQUIRE(!dummy.wear_item(item::spawn("test_pocket_vest")));
    REQUIRE(!dummy.wear_item(item::spawn("backpack")));

    item* vest = nullptr;
    for (item* worn : dummy.worn) {
        if (worn->typeId() == itype_id("test_pocket_vest")) {
            vest = worn;
        }
    }
    REQUIRE(vest != nullptr);

    // Excluding the vest is what stops its own pockets being offered as
    // somewhere to put the vest.
    for (const pocket_destination& dest : dummy.pocket_destinations(*vest, vest)) {
        CHECK(dest.container != vest);
    }
}

TEST_CASE("ask_pocket_destination declines before any menu when there are fewer than two destinations",
          "[inventory][pocket][destination]") {
    clear_avatar();
    auto& dummy = get_avatar();
    // A naked avatar has zero worn pockets, well under the two needed to be
    // worth asking about. If this ever tried to build and query a real menu
    // instead of declining first, the test would hang on input.
    auto rock = item::spawn("test_rock");
    const std::optional<pocket_destination> dest = ask_pocket_destination(dummy, *rock);
    CHECK_FALSE(dest.has_value());
    CHECK(rock->typeId() == itype_id("test_rock"));
}

// The advanced inventory manager built its inventory pane from the flat
// inventory alone, so an item routed into a worn pocket appeared in neither
// pane: not in AIM_INVENTORY, which could not see it, and not in AIM_WORN,
// which lists garments rather than their contents.
static advanced_inv_area aim_inventory_square()
{
    advanced_inv_area square(AIM_INVENTORY);
    // init() reads the real map around the player; the inventory square needs
    // nothing from it beyond being allowed to hold items.
    square.canputitemsloc = true;
    return square;
}

static std::vector<advanced_inv_listitem> aim_inventory_entries(advanced_inv_area& square)
{
    advanced_inventory_pane pane;
    pane.set_area(square, false);
    pane.add_items_from_area(square);
    return pane.items;
}

TEST_CASE("AIM's inventory pane lists an item in a worn pocket", "[inventory][ui][pocket][aim]") {
    clear_avatar();
    auto& dummy = get_avatar();
    REQUIRE(!dummy.wear_item(item::spawn("test_pocket_vest")));
    item* vest = dummy.worn.front();
    REQUIRE(!vest->put_in(item::spawn("test_rock")));
    item* rock = vest->contents.all_items_top().front();
    // The precondition the bug turned on, asserted rather than manufactured:
    // the rock is carried but the flat inventory does not hold it.
    REQUIRE(dummy.inv_size() == 0);

    advanced_inv_area square = aim_inventory_square();
    const std::vector<advanced_inv_listitem> entries = aim_inventory_entries(square);

    const auto holds = [&entries](const item* target) {
        return std::ranges::any_of(entries, [target](const advanced_inv_listitem& entry) {
            return entry.is_item_entry() && entry.items.front() == target;
        });
    };
    CHECK(holds(rock));
    // The garment belongs to AIM_WORN. Listing it here too would offer to move
    // a worn item out of the inventory pane.
    CHECK_FALSE(holds(vest));
}

TEST_CASE("AIM's inventory pane addresses a pocketed item by pointer",
          "[inventory][ui][pocket][aim]") {
    clear_avatar();
    auto& dummy = get_avatar();
    REQUIRE(!dummy.wear_item(item::spawn("test_pocket_vest")));
    item* vest = dummy.worn.front();
    REQUIRE(!vest->put_in(item::spawn("test_rock")));
    item* rock = vest->contents.all_items_top().front();

    advanced_inv_area square = aim_inventory_square();
    const std::vector<advanced_inv_listitem> entries = aim_inventory_entries(square);

    const auto entry = std::ranges::find_if(entries, [](const advanced_inv_listitem& e) {
        return e.is_item_entry() && e.items.front()->typeId() == itype_id("test_rock");
    });
    REQUIRE(entry != entries.end());
    // This is the whole point of the fix. The entry carries the item itself, so
    // every action path acts on the rock.
    CHECK(entry->items.front() == rock);
    // And the addressing it replaced, shown failing on the same entry: move and
    // examine both resolved sitem->idx through the flat inventory, which does
    // not hold the rock, so the index reached something else entirely.
    CHECK(&dummy.i_at(entry->idx) != rock);
}

TEST_CASE("AIM's inventory pane leaves an empty garment out",
          "[inventory][ui][pocket][aim]") {
    clear_avatar();
    auto& dummy = get_avatar();
    REQUIRE(!dummy.wear_item(item::spawn("test_pocket_vest")));
    item* vest = dummy.worn.front();

    advanced_inv_area square = aim_inventory_square();
    const std::vector<advanced_inv_listitem> entries = aim_inventory_entries(square);

    // An empty garment contributes nothing, and in particular must not sprout a
    // row for the garment itself.
    CHECK(entries.empty());
    CHECK(vest->contents.all_items_top().empty());
}


// ---------------------------------------------------------------------------
// Containers nested more than one level deep
// ---------------------------------------------------------------------------

// Every inventory screen nested exactly one level: a garment showed what was in
// its pockets, but a bag in one of those pockets showed nothing. Coins in a
// wallet in your jeans were carried and completely invisible.

// Wear a vest holding a plastic bag, and put a rock in the bag. Returns the bag.
static item* wear_vest_with_bagged_rock(avatar& dummy) {
    if (dummy.wear_item(item::spawn("test_pocket_vest"))) {
        return nullptr;
    }
    item* vest = dummy.worn.front();
    if (vest->put_in(item::spawn("bag_plastic"))) {
        return nullptr;
    }
    item* bag = vest->contents.all_items_top().front();
    if (bag->put_in(item::spawn("test_rock"))) {
        return nullptr;
    }
    return bag;
}

TEST_CASE("a container nested in a worn pocket shows its own contents",
          "[inventory][ui][pocket][nesting]") {
    clear_avatar();
    auto& dummy = get_avatar();
    item* bag = wear_vest_with_bagged_rock(dummy);
    REQUIRE(bag != nullptr);
    item* vest = dummy.worn.front();

    auto selector = inventory_selector(dummy);
    selector.add_character_items(dummy);

    const auto entries = selector.own_gear_column.get_all_entries(is_test_rock);
    REQUIRE(entries.size() == 1);
    // One indent per container between the rock and the garment.
    CHECK(entries.front()->indent == 2);
    // topmost_parent stays the outermost container, which is what names the
    // garment to hunt through in the category list.
    CHECK(entries.front()->topmost_parent == vest);
}

TEST_CASE("a nested child is drawn under its own container, not its garment",
          "[inventory][ui][pocket][nesting]") {
    clear_avatar();
    auto& dummy = get_avatar();
    item* bag = wear_vest_with_bagged_rock(dummy);
    REQUIRE(bag != nullptr);
    // A second, shallower item in the same garment gives the sort something to
    // interleave: without a depth-first pass the rock can land beside the ear
    // plugs instead of under the bag.
    item* vest = dummy.worn.front();
    REQUIRE(!vest->put_in(item::spawn("test_ear_plugs")));

    auto selector = inventory_selector(dummy);
    selector.add_character_items(dummy);
    // select_item_type() triggers prepare_paging(), which is what sorts.
    REQUIRE(selector.select_item_type(itype_id("test_rock")));

    const auto is_item = [](const inventory_entry& entry) { return entry.is_item(); };
    const auto entries = selector.own_gear_column.get_entries(is_item);

    const auto index_of = [&entries](const item* target) {
        for (size_t i = 0; i < entries.size(); ++i) {
            if (entries[i]->any_item() == target) {
                return static_cast<int>(i);
            }
        }
        return -1;
    };
    item* rock = bag->contents.all_items_top().front();
    const int bag_at = index_of(bag);
    const int rock_at = index_of(rock);
    REQUIRE(bag_at >= 0);
    REQUIRE(rock_at >= 0);
    // The rock sits immediately below the bag that holds it.
    CHECK(rock_at == bag_at + 1);
    CHECK(index_of(vest) < bag_at);
}

TEST_CASE("collapsing an inner container hides only what it holds",
          "[inventory][ui][pocket][nesting]") {
    clear_avatar();
    auto& dummy = get_avatar();
    item* bag = wear_vest_with_bagged_rock(dummy);
    REQUIRE(bag != nullptr);
    item* rock = bag->contents.all_items_top().front();

    const auto is_item = [](const inventory_entry& entry) { return entry.is_item(); };
    const auto shows = [&is_item](inventory_selector& sel, const item* target) {
        const auto entries = sel.own_gear_column.get_entries(is_item);
        return std::ranges::any_of(entries, [target](const inventory_entry* e) {
            return e->any_item() == target;
        });
    };

    // Open first. Without this the test passes on code that never draws the
    // rock at all, which is exactly the state the fix is meant to end.
    auto open_selector = inventory_selector(dummy);
    open_selector.add_character_items(dummy);
    REQUIRE(open_selector.select_item_type(itype_id("bag_plastic")));
    REQUIRE(shows(open_selector, bag));
    REQUIRE(shows(open_selector, rock));

    for (item_pocket& pocket : bag->contents.get_pockets()) {
        if (pocket.definition().type == pocket_type::CONTAINER
            && !pocket.all_items_top().empty()) {
            pocket.get_settings().set_collapse(true);
        }
    }

    auto shut_selector = inventory_selector(dummy);
    shut_selector.add_character_items(dummy);
    REQUIRE(shut_selector.select_item_type(itype_id("bag_plastic")));

    // The bag stays visible; only its contents fold away. Collapsing an inner
    // container used to do nothing at all, because hiding asked the outermost
    // container whether it was collapsed.
    CHECK(shows(shut_selector, bag));
    CHECK_FALSE(shows(shut_selector, rock));
    // The garment above it is untouched by shutting the bag.
    CHECK(shows(shut_selector, dummy.worn.front()));
}

TEST_CASE("a container nested in a pocket is marked collapsible",
          "[inventory][ui][pocket][nesting]") {
    clear_avatar();
    auto& dummy = get_avatar();
    item* bag = wear_vest_with_bagged_rock(dummy);
    REQUIRE(bag != nullptr);

    auto selector = inventory_selector(dummy);
    selector.add_character_items(dummy);

    const auto is_bag = [bag](const inventory_entry& entry) {
        return entry.is_item() && entry.any_item() == bag && entry.indent > 0;
    };
    const auto entries = selector.own_gear_column.get_all_entries(is_bag);
    REQUIRE(entries.size() == 1);

    // The marker says whether a container is open or shut, and only top-level
    // containers ever carried one, so a nested bag looked like an ordinary item.
    caption_probe probe;
    const std::string caption = probe.get_caption(*entries.front());
    CHECK(caption.find("[-]") != std::string::npos);
}

TEST_CASE("AIM's inventory pane lists an item two containers deep",
          "[inventory][ui][pocket][aim][nesting]") {
    clear_avatar();
    auto& dummy = get_avatar();
    item* bag = wear_vest_with_bagged_rock(dummy);
    REQUIRE(bag != nullptr);
    item* rock = bag->contents.all_items_top().front();
    REQUIRE(dummy.inv_size() == 0);

    advanced_inv_area square = aim_inventory_square();
    const std::vector<advanced_inv_listitem> entries = aim_inventory_entries(square);

    const auto holds = [&entries](const item* target) {
        return std::ranges::any_of(entries, [target](const advanced_inv_listitem& entry) {
            return entry.is_item_entry() && entry.items.front() == target;
        });
    };
    CHECK(holds(bag));
    CHECK(holds(rock));
}

TEST_CASE("AIM's inventory pane lists the contents of a carried container",
          "[inventory][ui][pocket][aim][nesting]") {
    clear_avatar();
    auto& dummy = get_avatar();
    detached_ptr<item> det = item::spawn("bag_plastic");
    item& bag = *det;
    REQUIRE(!bag.put_in(item::spawn("test_rock")));
    dummy.i_add(std::move(det));
    // The bag really is in the flat inventory; the rock in it is not.
    REQUIRE(dummy.inv_position_by_item(&bag) != INT_MIN);
    item* rock = bag.contents.all_items_top().front();

    advanced_inv_area square = aim_inventory_square();
    const std::vector<advanced_inv_listitem> entries = aim_inventory_entries(square);

    const auto holds = [&entries](const item* target) {
        return std::ranges::any_of(entries, [target](const advanced_inv_listitem& entry) {
            return entry.is_item_entry() && entry.items.front() == target;
        });
    };
    CHECK(holds(&bag));
    CHECK(holds(rock));
}
