#include "avatar.h"
#include "catch/catch.hpp"
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

    auto rock = item::spawn("test_rock");
    detached_ptr<item> refused =
        dummy.i_add_to_worn_pockets(std::move(rock), nullptr, true, false);
    CHECK_FALSE(refused);
    CHECK(vest->contents.all_items_top().size() == 1);
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

