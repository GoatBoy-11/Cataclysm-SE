# Choosing an Item's Pocket Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let the player put an item in a pocket of their choosing - both for
something already carried, and optionally at the moment it is picked up.

**Architecture:** Two testable seams and one thin UI layer on top. A targeted
insert (`item_contents::insert_into`) puts an item in a named pocket rather than
the best one; an enumeration (`Character::pocket_destinations`) lists every
pocket that would accept it. A `uilist` prompt built on those two serves both
features, so the picker is written once. The pickup toggle needs no new routing:
pickup, unload and character creation already funnel through
`Character::i_add_to_worn_pockets`, so one branch there covers all three.

**Tech Stack:** C++20, MSVC, Catch2, `uilist` for menus, `detached_ptr` for item
ownership transfer.

**Spec:** `docs/superpowers/specs/2026-08-29-pocket-system-design.md`

## Global Constraints

- **Classic mode does neither.** `pockets_are_classic()` pools storage into one
  compartment; a destination picker would promise a choice the mode cannot
  honour. Both the menu entry and the pickup prompt are absent there.
- **Migration never destroys an item.** If a targeted insert is refused, the
  item returns to the caller intact and the caller falls back to existing
  behaviour. No path may drop or delete on refusal.
- **All 181 existing `.contents.` call sites must compile unchanged.** New
  behaviour goes behind `item_contents`, never into its callers.
- **Character creation never prompts.** `stow_loose_inventory_into_pockets`
  passes `quiet = true` to the routing seam; that flag suppresses the prompt.
  A new character is handed dozens of items and must not be asked about each.
- **Prompt only on a real choice.** Zero or one candidate destination means no
  prompt: the existing automatic behaviour runs.
- **The test avatar carries `DEBUG_STORAGE`**, so `g->u` cannot exercise any
  over-capacity path. Use `standard_npc` where a pocket must actually fill up.
- **Build and test with the Windows commands in CLAUDE.md**, not the Linux ones
  in AGENTS.md. Check the binary timestamp before trusting a test result, and
  never chain a test run after a build without checking the build's own exit
  code - a failed build silently leaves a stale binary that passes.

---

### Task 1: Insert into a named pocket

`item_contents::insert_item` picks a pocket via `best_pocket()`. Nothing can
currently say "this pocket, not that one", which both features need.

**Files:**
- Modify: `src/item_contents.h` (declaration, near `insert_item` at line 93)
- Modify: `src/item_contents.cpp` (definition, beside `insert_item_impl`)
- Test: `tests/item_pocket_test.cpp`

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces: `ret_val<bool> item_contents::insert_into( size_t pocket_index, detached_ptr<item> &&it )`.
  Returns success when the pocket took the item. On failure the item is left in
  `it` for the caller to deal with, and `ret_val::str()` carries the pocket's
  own refusal reason. An out-of-range index is a failure, not undefined
  behaviour.

- [x] **Step 1: Write the failing tests**

```cpp
TEST_CASE( "insert_into puts an item in the pocket it names", "[item][pocket][insert]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    REQUIRE( bag->contents.get_pockets().size() == 2 );

    // Pocket 1 holds 4 L; pocket 0 holds only 100 ml. Naming pocket 1 must put
    // it there even though best_pocket() prefers the tightest fit.
    const ret_val<bool> res = bag->contents.insert_into( 1, item::spawn( "test_rock" ) );

    CHECK( res.success() );
    CHECK( bag->contents.get_pockets()[1].size() == 1 );
    CHECK( bag->contents.get_pockets()[0].empty() );
}

TEST_CASE( "insert_into refuses a pocket that cannot hold the item", "[item][pocket][insert]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    detached_ptr<item> rock = item::spawn( "test_rock" );

    // Pocket 0 is 100 ml. A refusal must hand the item back, not eat it.
    const ret_val<bool> res = bag->contents.insert_into( 0, std::move( rock ) );

    CHECK_FALSE( res.success() );
    CHECK( rock );
    CHECK( bag->contents.get_pockets()[0].empty() );
}

TEST_CASE( "insert_into refuses an index that does not exist", "[item][pocket][insert]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    detached_ptr<item> rock = item::spawn( "test_rock" );

    const ret_val<bool> res = bag->contents.insert_into( 99, std::move( rock ) );

    CHECK_FALSE( res.success() );
    CHECK( rock );
}
```

- [x] **Step 2: Run the tests and watch them fail**

```sh
cmake --build out/build/cse-vcpkg --config RelWithDebInfo --parallel 6; echo "EXIT=$?"
```

Expected: the build fails with `error C2039: 'insert_into': is not a member of 'item_contents'`.

- [x] **Step 3: Declare it**

In `src/item_contents.h`, directly below `insert_item`:

```cpp
        /**
         * Insert into one named pocket rather than whichever best_pocket()
         * would choose. The player picking a destination is the whole point,
         * so a full or restricted pocket refuses instead of falling back.
         */
        ret_val<bool> insert_into( size_t pocket_index, detached_ptr<item> &&it );
```

- [x] **Step 4: Define it**

In `src/item_contents.cpp`:

```cpp
ret_val<bool> item_contents::insert_into( size_t pocket_index, detached_ptr<item> &&it )
{
    if( pocket_index >= pockets.size() ) {
        return ret_val<bool>::make_failure( _( "that pocket does not exist" ) );
    }
    item_pocket &pocket = pockets[pocket_index];
    const ret_val<item_pocket::contain_code> allowed = pocket.can_contain( *it );
    if( !allowed.success() ) {
        return ret_val<bool>::make_failure( allowed.str() );
    }
    pocket.insert_item( std::move( it ) );
    return ret_val<bool>::make_success();
}
```

If `item_pocket::insert_item` does not take a `detached_ptr<item> &&` under that
exact name, read the pocket's own insertion method in `src/item_pocket.h` and
use it - do not add a second one.

- [x] **Step 5: Run the tests and watch them pass**

```sh
cmake --build out/build/cse-vcpkg --config RelWithDebInfo --parallel 6; echo "EXIT=$?"
export CATA_TEST_COMPUTE_ACCELERATION=cpu
out/build/cse-vcpkg/tests/RelWithDebInfo/cata_test-tiles.exe "[insert]"
```

Expected: 3 test cases pass.

- [x] **Step 6: Commit**

```bash
git add src/item_contents.h src/item_contents.cpp tests/item_pocket_test.cpp
git commit -m "feat: let an item be inserted into a pocket by index"
```

---

### Task 2: List the pockets that would take an item

Both features need the same question answered: where could this go? Kept out of
the UI so it can be tested without a screen.

**Files:**
- Modify: `src/character.h` (struct plus declaration, near `i_add_to_worn_pockets`)
- Modify: `src/character.cpp` (definition, above `i_add_to_worn_pockets` at line 2690)
- Test: `tests/inventory_ui_test.cpp`

**Interfaces:**
- Consumes: nothing from Task 1 - this only reads.
- Produces:
  - `struct pocket_destination { item *container; size_t pocket_index; };`
  - `std::vector<pocket_destination> Character::pocket_destinations( const item &it, const item *exclude = nullptr ) const;`
    Every CONTAINER pocket on a worn item that would accept `it`, ordered by
    player priority descending, wear order breaking ties. `exclude` skips one
    container, which unload needs so a garment does not swallow its own
    contents back. Empty in classic mode.

- [x] **Step 1: Write the failing tests**

```cpp
TEST_CASE( "pocket destinations list every pocket that would take the item",
           "[inventory][pocket][destination]" ) {
    clear_avatar();
    auto& dummy = get_avatar();
    REQUIRE(!dummy.wear_item(item::spawn("test_pocket_vest")));
    REQUIRE(!dummy.wear_item(item::spawn("backpack")));

    auto rock = item::spawn("test_rock");
    const auto destinations = dummy.pocket_destinations(*rock);

    // At least the vest's pocket and the backpack's main pocket.
    CHECK(destinations.size() >= 2);
    for (const pocket_destination& dest : destinations) {
        REQUIRE(dest.container != nullptr);
        CHECK(dest.pocket_index < dest.container->contents.get_pockets().size());
    }
}

TEST_CASE( "pocket destinations skip the excluded container",
           "[inventory][pocket][destination]" ) {
    clear_avatar();
    auto& dummy = get_avatar();
    REQUIRE(!dummy.wear_item(item::spawn("test_pocket_vest")));
    item* vest = dummy.worn.front();

    auto rock = item::spawn("test_rock");
    for (const pocket_destination& dest : dummy.pocket_destinations(*rock, vest)) {
        CHECK(dest.container != vest);
    }
}

TEST_CASE( "classic mode offers no pocket destinations",
           "[inventory][pocket][destination][classic]" ) {
    override_option classic("POCKET_SYSTEM", "classic");
    clear_avatar();
    auto& dummy = get_avatar();
    REQUIRE(!dummy.wear_item(item::spawn("test_pocket_vest")));

    auto rock = item::spawn("test_rock");
    CHECK(dummy.pocket_destinations(*rock).empty());
}
```

- [x] **Step 2: Run the tests and watch them fail**

```sh
cmake --build out/build/cse-vcpkg --config RelWithDebInfo --parallel 6; echo "EXIT=$?"
```

Expected: build failure, `'pocket_destinations': is not a member of 'Character'`.

- [x] **Step 3: Declare the struct and method**

In `src/character.h`, above the `Character` class:

```cpp
/** One place an item could go: a container the player has, and which of its pockets. */
struct pocket_destination {
    item *container = nullptr;
    size_t pocket_index = 0;
};
```

Inside `Character`, beside `i_add_to_worn_pockets`:

```cpp
        /**
         * Every worn pocket that would accept @p it, best first. @p exclude
         * skips one container, which unloading needs so a garment does not
         * take its own contents straight back. Empty in classic mode.
         */
        std::vector<pocket_destination> pocket_destinations(
            const item &it, const item *exclude = nullptr ) const;
```

- [x] **Step 4: Define it**

In `src/character.cpp`, above `i_add_to_worn_pockets`:

```cpp
std::vector<pocket_destination> Character::pocket_destinations(
    const item &it, const item *exclude ) const
{
    std::vector<pocket_destination> destinations;
    if( pockets_are_classic() ) {
        return destinations;
    }

    // Walk worn in order, then sort stably: wear order survives as the
    // tie-break among pockets of equal priority, matching how
    // i_add_to_worn_pockets already ranks them.
    for( item * const &garment : worn ) {
        if( garment == exclude ) {
            continue;
        }
        const std::vector<item_pocket> &pockets = garment->contents.get_pockets();
        for( size_t i = 0; i < pockets.size(); i++ ) {
            if( pockets[i].definition().type != pocket_type::CONTAINER ) {
                continue;
            }
            if( !pockets[i].can_contain( it ).success() ) {
                continue;
            }
            destinations.push_back( pocket_destination{ garment, i } );
        }
    }

    std::ranges::stable_sort( destinations,
    []( const pocket_destination & a, const pocket_destination & b ) {
        const int pa = a.container->contents.get_pockets()[a.pocket_index].get_settings().priority();
        const int pb = b.container->contents.get_pockets()[b.pocket_index].get_settings().priority();
        return pa > pb;
    } );
    return destinations;
}
```

- [x] **Step 5: Run the tests and watch them pass**

```sh
cmake --build out/build/cse-vcpkg --config RelWithDebInfo --parallel 6; echo "EXIT=$?"
export CATA_TEST_COMPUTE_ACCELERATION=cpu
out/build/cse-vcpkg/tests/RelWithDebInfo/cata_test-tiles.exe "[destination]"
```

Expected: 3 test cases pass.

- [x] **Step 6: Commit**

```bash
git add src/character.h src/character.cpp tests/inventory_ui_test.cpp
git commit -m "feat: list the worn pockets that would accept an item"
```

---

### Task 3: Move a carried item to a chosen pocket

The first player-facing half: select an item you already have, choose where it
goes. Mirrors the existing "Organize pockets" entry.

**Files:**
- Create: `src/pocket_destination_menu.h`
- Create: `src/pocket_destination_menu.cpp`
- Modify: `src/examine_item_menu.cpp` (new entry after the `ORGANIZE_POCKETS` block, lines 252-265)
- Modify: `data/raw/keybindings/keybindings.json` (after `ORGANIZE_POCKETS` at line 3716)
- Test: none - this is a `uilist` prompt, verified by playtest in Task 5

**Interfaces:**
- Consumes: `Character::pocket_destinations` (Task 2), `item_contents::insert_into` (Task 1).
- Produces: `bool choose_pocket_destination( Character &who, item &it, const item *exclude = nullptr );`
  Prompts for a destination and moves `it` there. Returns true when the item
  moved. Returns false without prompting when there is nothing to choose
  between.

A new file rather than an addition to `examine_item_menu.cpp`, per the fork
discipline in CLAUDE.md: a new path conflicts only if upstream adds the same
one, while edits inside an existing function body are the most expensive kind.

- [x] **Step 1: Write the header**

`src/pocket_destination_menu.h`:

```cpp
#pragma once

class Character;
class item;

/**
 * Ask which pocket an item should go into, and put it there.
 *
 * Returns true when the item moved. Returns false without prompting when the
 * player has no real choice: classic mode, or fewer than two destinations.
 */
bool choose_pocket_destination( Character &who, item &it, const item *exclude = nullptr );
```

- [x] **Step 2: Write the menu**

`src/pocket_destination_menu.cpp`:

```cpp
#include "pocket_destination_menu.h"

#include "character.h"
#include "item.h"
#include "item_contents.h"
#include "item_pocket.h"
#include "output.h"
#include "translations.h"
#include "units_utility.h"

bool choose_pocket_destination( Character &who, item &it, const item *exclude )
{
    const std::vector<pocket_destination> destinations = who.pocket_destinations( it, exclude );
    if( destinations.size() < 2 ) {
        return false;
    }

    uilist menu;
    menu.title = string_format( _( "Where should the %s go?" ), it.tname() );

    for( const pocket_destination &dest : destinations ) {
        const item_pocket &pocket = dest.container->contents.get_pockets()[dest.pocket_index];
        // Name the pocket by container and remaining room: two pockets on one
        // garment are otherwise indistinguishable in a list.
        menu.addentry( menu.entries.size(), true, MENU_AUTOASSIGN,
                       _( "%1$s - %2$s free" ),
                       dest.container->tname(),
                       format_volume( pocket.remaining_volume() ) );
    }

    menu.query();
    if( menu.ret < 0 || static_cast<size_t>( menu.ret ) >= destinations.size() ) {
        return false;
    }

    const pocket_destination &chosen = destinations[menu.ret];
    detached_ptr<item> moved = it.detach();
    const std::string moved_name = moved->tname();
    const std::string container_name = chosen.container->tname();

    ret_val<bool> inserted =
        chosen.container->contents.insert_into( chosen.pocket_index, std::move( moved ) );
    if( !inserted.success() ) {
        // The item is still in `moved`; hand it back rather than lose it.
        who.i_add( std::move( moved ) );
        popup( _( "The %1$s will not go in there: %2$s" ), moved_name, inserted.str() );
        return false;
    }

    who.add_msg_if_player( _( "You put the %1$s in your %2$s." ), moved_name, container_name );
    return true;
}
```

If `format_volume` or `it.detach()` do not resolve, check the includes in
`src/item_contents.cpp`, which calls both.

- [x] **Step 3: Add the menu entry**

In `src/examine_item_menu.cpp`, immediately after the `ORGANIZE_POCKETS` block:

```cpp
    // Moving an item needs somewhere to move it to. The picker declines on its
    // own when there is nothing to choose between, but an entry that always
    // declines reads as broken, so gate it here too.
    if( !pockets_are_classic() && you.pocket_destinations( itm ).size() >= 2 ) {
        add_entry( "MOVE_TO_POCKET", hint_rating::good, [&]() {
            choose_pocket_destination( you, itm );
            return true;
        } );
    }
```

Return `true`, not `false`: the item has moved, so the menu it was opened from
is stale and must close. Add `#include "pocket_destination_menu.h"` at the top.

- [x] **Step 4: Add the keybinding**

In `data/raw/keybindings/keybindings.json`, after the `ORGANIZE_POCKETS` object:

```json
  {
    "id": "MOVE_TO_POCKET",
    "type": "keybinding",
    "category": "INVENTORY_ITEM",
    "name": "Move to pocket",
    "bindings": [ { "input_method": "keyboard", "key": "m" } ]
  },
```

- [x] **Step 5: Build**

`src/` globs with `CONFIGURE_DEPENDS`, so a new source file needs no CMake
change. If the build cannot find it, re-run `cmake --preset cse-msvc`.

```sh
cmake --build out/build/cse-vcpkg --config RelWithDebInfo --parallel 6; echo "EXIT=$?"
```

Expected: EXIT=0.

- [x] **Step 6: Lint the JSON**

```sh
out/build/cse-vcpkg/tools/format/RelWithDebInfo/json_formatter.exe data/raw/keybindings/keybindings.json
git diff --stat data/raw/keybindings/keybindings.json
```

Expected: either no diff, or only formatting the tool applied. Commit whatever
it produces.

- [x] **Step 7: Commit**

```bash
git add src/pocket_destination_menu.h src/pocket_destination_menu.cpp \
        src/examine_item_menu.cpp data/raw/keybindings/keybindings.json
git commit -m "feat: move a carried item into a pocket of your choosing"
```

---

### Task 4: A world option for choosing at pickup

**Files:**
- Modify: `src/options.cpp` (new option after `POCKET_SYSTEM` at line 2895)
- Modify: `src/item_pocket.h` (declaration beside `pockets_are_classic` at line 125)
- Modify: `src/item_pocket.cpp` (definition beside `pockets_are_classic`)
- Modify: `src/character.cpp` (branch in `i_add_to_worn_pockets` at line 2690)
- Test: `tests/inventory_ui_test.cpp`

**Interfaces:**
- Consumes: `Character::pocket_destinations` (Task 2), `choose_pocket_destination` (Task 3).
- Produces: `bool pockets_prompt_on_pickup();` - true when `POCKET_PICKUP` is
  `choose` and the pocket system is not classic.

`i_add_to_worn_pockets` is the one seam pickup, unload and character creation
all call, so the branch goes there and covers all three. `quiet` already marks
the non-interactive caller, which is exactly the flag needed to keep character
creation silent.

- [x] **Step 1: Write the failing test**

```cpp
TEST_CASE( "the pickup prompt is off by default and follows the option",
           "[inventory][pocket][option]" ) {
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
```

- [x] **Step 2: Run it and watch it fail**

```sh
cmake --build out/build/cse-vcpkg --config RelWithDebInfo --parallel 6; echo "EXIT=$?"
```

Expected: build failure, `'pockets_prompt_on_pickup': identifier not found`.

- [x] **Step 3: Add the option**

In `src/options.cpp`, directly after the `POCKET_SYSTEM` block:

```cpp
    add( "POCKET_PICKUP", world_default, translate_marker( "Choosing an item's pocket" ),
    translate_marker( "Auto puts a picked-up item in the best pocket, preferring any priority you have set.  Choose asks which pocket each item should go in.  Ignored by the classic pocket system." ), {
        { "auto", translate_marker( "Auto" ) }, { "choose", translate_marker( "Choose" ) }
    }, "auto"
       );
```

- [x] **Step 4: Add the predicate**

In `src/item_pocket.h`, beside `pockets_are_classic()`:

```cpp
/** True when the player has asked to pick a pocket for each item picked up. */
bool pockets_prompt_on_pickup();
```

In `src/item_pocket.cpp`, beside `pockets_are_classic()`:

```cpp
bool pockets_prompt_on_pickup()
{
    // Classic mode pools storage, so there is nothing to choose between.
    return !pockets_are_classic() && get_option<std::string>( "POCKET_PICKUP" ) == "choose";
}
```

- [x] **Step 5: Run the test and watch it pass**

```sh
cmake --build out/build/cse-vcpkg --config RelWithDebInfo --parallel 6; echo "EXIT=$?"
export CATA_TEST_COMPUTE_ACCELERATION=cpu
out/build/cse-vcpkg/tests/RelWithDebInfo/cata_test-tiles.exe "[option]"
```

Expected: PASS.

- [x] **Step 6: Branch the routing seam**

In `src/character.cpp`, inside `i_add_to_worn_pockets`, after the liquid and
casing guard and before the automatic ranking loop:

```cpp
    // Choose mode asks, but only for a deliberate single action. Character
    // creation passes quiet and is handed dozens of items; prompting for each
    // would be unusable, and the player has not started playing yet.
    if( !quiet && pockets_prompt_on_pickup() ) {
        if( choose_pocket_destination( *this, *it, exclude ) ) {
            return detached_ptr<item>();
        }
        // Declined or refused: fall through to automatic routing below.
    }
```

Add `#include "pocket_destination_menu.h"` to the top of `character.cpp`.

**Ownership warning - read before writing this.** `it` is a
`detached_ptr<item>` and `choose_pocket_destination` takes an `item &`, so the
picker calls `it.detach()` on an item this function still owns. Getting that
wrong either double-frees or leaks, and the standing constraint is that
migration never destroys an item. Before implementing, check what
`detached_ptr::detach()` does to the original pointer. If the two ownership
models cannot be reconciled cleanly, **change Task 3's signature** to take and
return `detached_ptr<item>` instead of a reference, and adjust the Task 3 call
site in `examine_item_menu.cpp` to match. That is the safer shape; do not fight
the ownership model to preserve a signature written before this was known.

- [x] **Step 7: Build and run the whole pocket suite**

```sh
cmake --build out/build/cse-vcpkg --config RelWithDebInfo --parallel 6; echo "EXIT=$?"
export CATA_TEST_COMPUTE_ACCELERATION=cpu
out/build/cse-vcpkg/tests/RelWithDebInfo/cata_test-tiles.exe "[pocket]"
```

Expected: all pass. Routing tests must not regress - the option defaults to
`auto`, so every existing test takes the old path.

- [x] **Step 8: Commit**

```bash
git add src/options.cpp src/item_pocket.h src/item_pocket.cpp \
        src/character.cpp tests/inventory_ui_test.cpp
git commit -m "feat: add a world option to choose an item's pocket at pickup"
```

---

### Task 5: Verification

**Files:** none - this task runs things.

- [x] **Step 1: Full suite**

```sh
export CATA_TEST_COMPUTE_ACCELERATION=cpu
out/build/cse-vcpkg/tests/RelWithDebInfo/cata_test-tiles.exe > full.log 2>&1; echo "EXIT=$?"
tail -6 full.log
```

Expected: only the four environmental `vision_*` failures named in CLAUDE.md -
`vision_wall_obstructs_light`, `vision_single_tile_skylight`,
`vision_see_out_of_vehicle`, `vision_see_into_vehicle`. Any other failure is
real.

- [x] **Step 2: Rotate the playtest exe**

```sh
bash .claude/rotate-game-exe.sh
```

- [x] **Step 3: Playtest in `auto` mode**

Nothing below is covered by a test; a `uilist` needs eyes.

- Wear two containers. Examine a carried item and press `m`. The list names each
  destination by container and free space, and picking one moves the item.
- The moved item appears under its new parent in ITEMS WORN, and its category
  line on the left names the new container in brackets.
- An item with fewer than two destinations offers no `m` entry at all.
- A classic world offers neither `m` nor the option.

- [x] **Step 4: Playtest in `choose` mode**

Set "Choosing an item's pocket" to Choose in world options, then:

- Pick up one item: the prompt appears and the item lands where you said.
- Pick up a pile of several items: confirm the prompt is bearable. **If it asks
  once per item and that is miserable, stop and report it** - the fix is a
  "put the rest here too" entry, deliberately left out of this plan because it
  should be designed against the real feel rather than guessed at.
- Unload a gun while wearing a backpack: the prompt appears for the magazine.
- Roll a new character: **no prompt at any point.** This is the constraint most
  likely to break; if creation prompts, `quiet` is not reaching the branch.
- Escape out of a prompt: the item still ends up somewhere sensible, never lost.

- [x] **Step 5: Tick this plan's boxes and commit**

```bash
git add docs/superpowers/plans/2026-09-01-pocket-destination-choice.md
git commit -m "docs: tick the pocket destination plan"
```

---

## Out of scope

- **Batch answers at pickup** ("put the rest here too"). Task 5 tests whether it
  is needed; designing it before feeling the prompt would be guesswork.
- **Non-worn destinations** - a container on the ground or in a vehicle. The
  enumeration is deliberately worn-only, matching what routing already does.
- **Reordering or renaming pockets.** Priority in the organize menu already
  covers the intent.
- **A remembered per-item default.** That is what the whitelist in "Organize
  pockets" is for, and two systems for one job would disagree.

## Prior art in this repo

- `item_contents::favorite_settings_menu()` at `src/item_contents.cpp:887` is the
  closest existing menu. Copy its shape, its classic-mode guard, and its habit
  of filtering to general-purpose pockets.
- `examine_item_menu.cpp:252-265` is the entry-point pattern, including how it
  counts container pockets before offering anything.
- `Character::i_add_to_worn_pockets` at `src/character.cpp:2690` already ranks
  pockets by player priority; Task 2's sort must not contradict it.
