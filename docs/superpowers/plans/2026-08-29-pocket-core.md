# Pocket Core Implementation Plan (Phase 1)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace `item_contents`' flat item list with a vector of pockets, behind an unchanged public API, so the game plays identically while the pocket model exists underneath.

**Architecture:** A new `item_pocket` class owns a `location_vector<item>` constructed with the same `contents_item_location( owner )` that `item_contents` uses today. `item_contents` holds `std::vector<item_pocket>` and fans its existing methods out across them. Phase 1 gives every item exactly one synthesized pocket, so behaviour is unchanged and the 181 existing call sites never learn pockets exist.

**Tech Stack:** C++ (MSVC 19.51, VS 18), CMake presets, Catch2 via `tests/catch/catch.hpp`, BN's `detached_ptr`/`location_vector` ownership model.

**Spec:** `docs/superpowers/specs/2026-08-29-pocket-system-design.md`

## Global Constraints

- **All 181 existing `.contents.` call sites must compile unchanged.** Anything forcing a call-site edit is solved inside `item_contents` instead.
- **Migration never destroys an item.**
- CDDA reference implementation is pinned at `5b915aea09`. Do not track their `master`.
- `pocket_data` JSON schema stays identical to CDDA's. Do not rename fields.
- Configure: `cmake --preset cse-msvc`
- Build: `cmake --build out/build/cse-vcpkg --config RelWithDebInfo --parallel 6`
- Test binary: `out/build/cse-vcpkg/tests/RelWithDebInfo/cata_test-tiles.exe`
- `src/` is globbed with `CONFIGURE_DEPENDS`, so new source files need no CMake edit. **`tests/` is globbed WITHOUT `CONFIGURE_DEPENDS`** — after adding a new test file, re-run `cmake --preset cse-msvc` or it will not be compiled.
- Test style: `TEST_CASE( "name", "[item][pocket]" )`, items created with `item::spawn( "itype_id" )` returning `detached_ptr<item>` (`.get()`, `operator*`, `operator->` available).

## Phasing

This plan is Phase 1 only. Later phases each get their own plan:

| Phase | Contents | Blocked by |
|---|---|---|
| **1 (this plan)** | `item_pocket`, `item_contents` internals, serialization, synthesis from legacy fields | — |
| 2 | `pocket_data` JSON loading, curated multi-pocket items | 1 |
| 3 | `best_pocket()`, `favorite_settings`, inventory organization UI | 2 |
| 4 | Classic mode world option | 3 |
| 5 | Pocket templates, coverage measurement, debug listing command | 2 |

---

### Task 1: `pocket_data` and `item_pocket`

New, self-contained, not yet wired into anything. The game is untouched by this task.

**Files:**
- Create: `src/item_pocket.h`
- Create: `src/item_pocket.cpp`
- Test: `tests/item_pocket_test.cpp`

**Interfaces:**
- Consumes: `location_vector<item>`, `contents_item_location`, `detached_ptr<item>`, `ret_val<T>`, `units::volume`, `units::mass`.
- Produces:
  - `enum class pocket_type { CONTAINER, MAGAZINE, MAGAZINE_WELL, MOD, CORPSE, MIGRATION, LAST }`
  - `struct pocket_data` with public fields `type`, `max_contains_volume`, `max_contains_weight`, `max_item_length`, `rigid`, `watertight`, `sealed`, `spoil_multiplier`, `moves`
  - `class item_pocket` with `item_pocket( item *owner, const pocket_data *data )`, `bool empty() const`, `const std::vector<item *> &all_items_top() const`, `units::volume contents_volume() const`, `units::volume remaining_volume() const`, `ret_val<item_pocket::contain_code> can_contain( const item &it ) const`, `void insert( detached_ptr<item> &&it )`, `detached_ptr<item> remove( item *it )`, `enum class contain_code { SUCCESS, ERR_TOO_BIG, ERR_TOO_HEAVY, ERR_NO_SPACE }`

- [ ] **Step 1: Write the failing tests**

Create `tests/item_pocket_test.cpp`:

```cpp
#include "catch/catch.hpp"
#include "item.h"
#include "item_pocket.h"
#include "ret_val.h"
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
```

- [ ] **Step 2: Reconfigure so the new test file is picked up**

Run: `cmake --preset cse-msvc`
Expected: `Generating done`. Required because the `tests/` glob has no `CONFIGURE_DEPENDS`.

- [ ] **Step 3: Run the tests to verify they fail**

Run: `cmake --build out/build/cse-vcpkg --config RelWithDebInfo --parallel 6`
Expected: BUILD FAILS with `Cannot open include file: 'item_pocket.h'`.

- [ ] **Step 4: Write the header**

Create `src/item_pocket.h`:

```cpp
#pragma once

#include <vector>

#include "location_vector.h"
#include "ret_val.h"
#include "units.h"

class item;
class JsonIn;
class JsonOut;

enum class pocket_type {
    CONTAINER,
    MAGAZINE,
    MAGAZINE_WELL,
    MOD,
    CORPSE,
    MIGRATION,
    LAST
};

/**
 * Immutable, shared definition of one pocket. Lives on itype.
 * Field names match CDDA's JSON schema; do not rename.
 */
struct pocket_data {
    pocket_type type = pocket_type::CONTAINER;
    units::volume max_contains_volume = 0_ml;
    /** zero means unbounded */
    units::mass max_contains_weight = 0_gram;
    /** zero means unbounded */
    units::length max_item_length = 0_mm;
    bool rigid = false;
    bool watertight = false;
    bool sealed = false;
    float spoil_multiplier = 1.0f;
    int moves = 100;
};

/**
 * One compartment of an item. Owns its contents through the same
 * contents_item_location the owning item uses, so item ownership and
 * location tracking are unchanged by pockets.
 */
class item_pocket
{
    public:
        enum class contain_code {
            SUCCESS,
            ERR_TOO_BIG,
            ERR_TOO_HEAVY,
            ERR_NO_SPACE
        };

        item_pocket( item *owner, const pocket_data *data );

        bool empty() const;
        const std::vector<item *> &all_items_top() const;

        units::volume contents_volume() const;
        units::volume remaining_volume() const;
        units::mass contents_weight() const;

        ret_val<contain_code> can_contain( const item &it ) const;
        void insert( detached_ptr<item> &&it );
        detached_ptr<item> remove( item *it );

        std::vector<detached_ptr<item>> clear();
        void on_destroy();

        const pocket_data &definition() const {
            return *data;
        }

    private:
        const pocket_data *data;
        location_vector<item> contents;
};
```

- [ ] **Step 5: Write the implementation**

Create `src/item_pocket.cpp`:

```cpp
#include "item_pocket.h"

#include "item.h"
#include "locations.h"
#include "translations.h"

item_pocket::item_pocket( item *owner, const pocket_data *data )
    : data( data ), contents( new contents_item_location( owner ) ) {}

bool item_pocket::empty() const
{
    return contents.empty();
}

const std::vector<item *> &item_pocket::all_items_top() const
{
    return contents.as_vector();
}

units::volume item_pocket::contents_volume() const
{
    units::volume total = 0_ml;
    for( const item * const it : contents ) {
        total += it->volume();
    }
    return total;
}

units::volume item_pocket::remaining_volume() const
{
    return data->max_contains_volume - contents_volume();
}

units::mass item_pocket::contents_weight() const
{
    units::mass total = 0_gram;
    for( const item * const it : contents ) {
        total += it->weight();
    }
    return total;
}

ret_val<item_pocket::contain_code> item_pocket::can_contain( const item &it ) const
{
    if( it.volume() > remaining_volume() ) {
        return ret_val<contain_code>::make_failure( contain_code::ERR_TOO_BIG,
                _( "does not fit" ) );
    }
    if( data->max_contains_weight > 0_gram &&
        contents_weight() + it.weight() > data->max_contains_weight ) {
        return ret_val<contain_code>::make_failure( contain_code::ERR_TOO_HEAVY,
                _( "is too heavy" ) );
    }
    return ret_val<contain_code>::make_success( contain_code::SUCCESS );
}

void item_pocket::insert( detached_ptr<item> &&it )
{
    contents.push_back( std::move( it ) );
}

detached_ptr<item> item_pocket::remove( item *it )
{
    detached_ptr<item> removed;
    for( auto iter = contents.begin(); iter != contents.end(); ) {
        if( *iter == it ) {
            contents.erase( iter, &removed );
            return removed;
        }
        ++iter;
    }
    return removed;
}

std::vector<detached_ptr<item>> item_pocket::clear()
{
    return contents.clear();
}

void item_pocket::on_destroy()
{
    contents.on_destroy();
}
```

- [ ] **Step 6: Run the tests to verify they pass**

Run: `cmake --build out/build/cse-vcpkg --config RelWithDebInfo --parallel 6`
then: `out/build/cse-vcpkg/tests/RelWithDebInfo/cata_test-tiles.exe "[pocket]"`
Expected: 4 test cases pass.

If `ret_val::make_failure` or `location_vector::erase` signatures differ from the above, correct the call to match the real signature in `src/ret_val.h` and `src/location_vector.h` — do not change the test expectations.

- [ ] **Step 7: Commit**

```bash
git add src/item_pocket.h src/item_pocket.cpp tests/item_pocket_test.cpp
git commit -m "feat: add item_pocket and pocket_data"
```

---

### Task 2: Swap `item_contents` internals to a vector of pockets

The atomic change. Every method fans out; the public API is byte-identical.

**Files:**
- Modify: `src/item_contents.h` (replace `location_vector<item> items` with `std::vector<item_pocket> pockets`)
- Modify: `src/item_contents.cpp` (rewrite every method body)
- Test: `tests/item_pocket_test.cpp` (add contents-level cases)

**Interfaces:**
- Consumes: everything Task 1 produced.
- Produces: `item_contents` gains private `std::vector<item_pocket> pockets` and `pocket_data default_pocket_data` (a single unbounded CONTAINER pocket used until Task 4 supplies real ones). Public API unchanged. Adds one new public method: `std::vector<item_pocket> &get_pockets()` for later phases.

- [ ] **Step 1: Write the failing test**

Append to `tests/item_pocket_test.cpp`:

```cpp
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
```

- [ ] **Step 2: Run to verify it fails**

Run: `cmake --build out/build/cse-vcpkg --config RelWithDebInfo --parallel 6`
Expected: BUILD FAILS with `'get_pockets': is not a member of 'item_contents'`.

- [ ] **Step 3: Change the member declaration**

In `src/item_contents.h`, add `#include "item_pocket.h"` and replace the private members:

```cpp
    private:
        auto update_processing_cache() const -> void;

        item *owner;
        pocket_data default_pocket_data;
        std::vector<item_pocket> pockets;
        mutable bool processing_cache_dirty = true;
        mutable std::vector<item *> cached_processing_items;
        mutable std::vector<item *> cached_all_items_top;
```

and add to the public section:

```cpp
        /** later phases operate on pockets directly; phase 1 has exactly one */
        std::vector<item_pocket> &get_pockets() {
            return pockets;
        }
        const std::vector<item_pocket> &get_pockets() const {
            return pockets;
        }
```

- [ ] **Step 4: Rewrite the constructors**

In `src/item_contents.cpp`:

```cpp
item_contents::item_contents( item *container ) : owner( container )
{
    default_pocket_data.type = pocket_type::CONTAINER;
    default_pocket_data.max_contains_volume = units::from_liter( 100000 );
    pockets.emplace_back( container, &default_pocket_data );
}

/** used to aid migration */
item_contents::item_contents( item *container,
                              std::vector<detached_ptr<item>> &items ) : item_contents( container )
{
    for( detached_ptr<item> &it : items ) {
        pockets.front().insert( std::move( it ) );
    }
}
```

The 100000 litre placeholder keeps phase 1 behaviour identical to today's unbounded list. Task 4 replaces it with the itype's real capacity.

- [ ] **Step 5: Fan out every remaining method**

Rewrite each method in `src/item_contents.cpp` to loop over `pockets` instead of touching `items`. The four shapes:

```cpp
// 1. ANY — empty(), has_any_with(), item_has_uses_recursive()
bool item_contents::empty() const
{
    for( const item_pocket &pocket : pockets ) {
        if( !pocket.empty() ) {
            return false;
        }
    }
    return true;
}

// 2. CONCATENATE — all_items_top(), all_items_ptr(), gunmods()
const std::vector<item *> &item_contents::all_items_top() const
{
    cached_all_items_top.clear();
    for( const item_pocket &pocket : pockets ) {
        const std::vector<item *> &top = pocket.all_items_top();
        cached_all_items_top.insert( cached_all_items_top.end(), top.begin(), top.end() );
    }
    return cached_all_items_top;
}

// 3. SUM — item_size_modifier(), item_weight_modifier(), num_item_stacks()
units::volume item_contents::item_size_modifier() const
{
    units::volume total = 0_ml;
    for( const item_pocket &pocket : pockets ) {
        if( !pocket.definition().rigid ) {
            total += pocket.contents_volume();
        }
    }
    return total;
}

// 4. FIND THE OWNING POCKET — remove_top(), remove_items_with()
detached_ptr<item> item_contents::remove_top( item *it )
{
    for( item_pocket &pocket : pockets ) {
        detached_ptr<item> removed = pocket.remove( it );
        if( removed ) {
            invalidate_processing_cache();
            return removed;
        }
    }
    return detached_ptr<item>();
}
```

`insert_item()` in phase 1 always targets `pockets.front()`; Phase 3 replaces this with `best_pocket()`:

```cpp
ret_val<bool> item_contents::insert_item( detached_ptr<item> &&it )
{
    ret_val<item_pocket::contain_code> ok = pockets.front().can_contain( *it );
    if( !ok.success() ) {
        return ret_val<bool>::make_failure( false, ok.str() );
    }
    pockets.front().insert( std::move( it ) );
    invalidate_processing_cache();
    return ret_val<bool>::make_success( true );
}
```

`front()`, `back()`, `size()` and `erase()` currently delegate straight to `items`. Route each to the first non-empty pocket, preserving current behaviour for the single-pocket case.

- [ ] **Step 6: Build and run the full suite**

Run: `cmake --build out/build/cse-vcpkg --config RelWithDebInfo --parallel 6`
Expected: builds with zero edits to any file outside `src/item_contents.*` and `src/item_pocket.*`. **If any other file fails to compile, the fan-out is wrong — fix `item_contents`, do not edit the call site.**

Run: `out/build/cse-vcpkg/tests/RelWithDebInfo/cata_test-tiles.exe`
Expected: the entire suite passes, not just `[pocket]`. This is the real gate for this task.

- [ ] **Step 7: Commit**

```bash
git add src/item_contents.h src/item_contents.cpp tests/item_pocket_test.cpp
git commit -m "refactor: hold item contents in pockets"
```

---

### Task 3: Serialization and save migration

**Files:**
- Modify: `src/item_contents.cpp` (`serialize`, `deserialize`)
- Modify: `src/savegame.cpp` (version constant)
- Test: `tests/item_pocket_test.cpp`

**Interfaces:**
- Consumes: `item_contents::get_pockets()`, `item_pocket::insert()`, `item_pocket::all_items_top()`.
- Produces: save format `{ "pockets": [ { "pocket_type": <int>, "contents": [ <item>, ... ] } ] }`. Legacy saves present `contents` as a bare item array and are read into pocket 0.

- [ ] **Step 1: Write the failing tests**

```cpp
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

TEST_CASE( "a_legacy_flat_contents_array_loads_into_the_first_pocket", "[item][pocket][save]" )
{
    const std::string legacy = R"({ "contents": [ { "typeid": "sugar" } ] })";

    detached_ptr<item> backpack = item::spawn( "backpack" );
    std::istringstream is( legacy );
    JsonIn ji( is );
    backpack->contents.deserialize( ji );

    CHECK( backpack->contents.all_items_top().size() == 1 );
    CHECK( backpack->contents.get_pockets().front().all_items_top().size() == 1 );
}
```

Add `#include "json.h"` and `#include <sstream>` to the test file.

- [ ] **Step 2: Run to verify they fail**

Run: `out/build/cse-vcpkg/tests/RelWithDebInfo/cata_test-tiles.exe "[save]"`
Expected: FAIL — the legacy array is not read, or the round trip loses the item.

- [ ] **Step 3: Implement serialization**

```cpp
void item_contents::serialize( JsonOut &json ) const
{
    json.start_object();
    json.member( "pockets" );
    json.start_array();
    for( const item_pocket &pocket : pockets ) {
        json.start_object();
        json.member( "pocket_type", static_cast<int>( pocket.definition().type ) );
        json.member( "contents", pocket.all_items_top() );
        json.end_object();
    }
    json.end_array();
    json.end_object();
}

void item_contents::deserialize( JsonIn &jsin )
{
    JsonObject data = jsin.get_object();

    if( data.has_array( "pockets" ) ) {
        size_t index = 0;
        for( JsonObject jo : data.get_array( "pockets" ) ) {
            if( index >= pockets.size() ) {
                break;
            }
            for( detached_ptr<item> &it : read_contents_array( jo, "contents" ) ) {
                pockets[index].insert( std::move( it ) );
            }
            index++;
        }
        return;
    }

    // Legacy: a flat contents array from a pre-pocket save.
    // Everything lands in pocket 0. Phase 3 redistributes via best_pocket().
    for( detached_ptr<item> &it : read_contents_array( data, "contents" ) ) {
        pockets.front().insert( std::move( it ) );
    }
}
```

`read_contents_array` stands for whatever item-array read helper the current `item_contents::deserialize` body already uses. Read that function before writing this step and reuse it verbatim rather than writing a new one; if it is inline, extract it to a static helper with this name.

- [ ] **Step 4: Bump the savegame version**

In `src/savegame.cpp`, increment `savegame_version` by one and add a comment naming this change, matching the format of the existing entries.

- [ ] **Step 5: Run the tests**

Run: `out/build/cse-vcpkg/tests/RelWithDebInfo/cata_test-tiles.exe "[save]"`
Expected: PASS. Then run the whole suite; it must stay green.

- [ ] **Step 6: Commit**

```bash
git add src/item_contents.cpp src/savegame.cpp tests/item_pocket_test.cpp
git commit -m "feat: serialize contents as pockets and read legacy saves"
```

---

### Task 4: Synthesize pockets from legacy item fields

**Files:**
- Modify: `src/itype.h` (add `std::vector<pocket_data> pockets;` to `itype`)
- Modify: `src/item_factory.cpp` (add `synthesize_pockets_from_legacy`, call it during finalization)
- Modify: `src/item_contents.cpp` (build pockets from the itype rather than one hardcoded pocket)
- Test: `tests/item_pocket_test.cpp`

**Interfaces:**
- Consumes: `pocket_data`, `itype::container` (`islot_container::contains`, `watertight`, `seals`), `itype::armor` (`islot_armor::storage`).
- Produces: `itype::pockets`; `bool has_only_special_pockets( const itype &def )`; `void synthesize_pockets_from_legacy( itype &def )`.

- [ ] **Step 1: Write the failing tests**

```cpp
TEST_CASE( "a_container_gains_one_pocket_sized_from_its_legacy_storage", "[item][pocket][synthesis]" )
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
```

- [ ] **Step 2: Run to verify they fail**

Run: `out/build/cse-vcpkg/tests/RelWithDebInfo/cata_test-tiles.exe "[synthesis]"`
Expected: FAIL — remaining volume is the 100000 litre placeholder, not the item's real capacity.

- [ ] **Step 3: Add the synthesis pass**

In `src/item_factory.cpp`:

```cpp
static bool has_only_special_pockets( const itype &def )
{
    if( def.pockets.empty() ) {
        return true;
    }
    for( const pocket_data &pocket : def.pockets ) {
        if( pocket.type == pocket_type::CONTAINER ) {
            return false;
        }
    }
    return true;
}

static void synthesize_pockets_from_legacy( itype &def )
{
    if( !has_only_special_pockets( def ) ) {
        if( def.container || ( def.armor && def.armor->storage > 0_ml ) ) {
            debugmsg( "%s defines both legacy storage and pocket_data; pocket_data wins.",
                      def.get_id().str() );
        }
        return;
    }

    pocket_data pocket;
    pocket.type = pocket_type::CONTAINER;

    if( def.container ) {
        pocket.max_contains_volume = def.container->contains;
        pocket.watertight = def.container->watertight;
        pocket.sealed = def.container->seals;
        pocket.rigid = true;
    } else if( def.armor && def.armor->storage > 0_ml ) {
        pocket.max_contains_volume = def.armor->storage;
        pocket.rigid = false;
    } else {
        return;
    }

    def.pockets.push_back( pocket );
}
```

Call `synthesize_pockets_from_legacy( obj );` from the same finalization function that resolves `copy-from`, immediately after that resolution completes.

- [ ] **Step 4: Build pockets from the itype**

In `src/item_contents.cpp`, replace the hardcoded single pocket:

```cpp
item_contents::item_contents( item *container ) : owner( container )
{
    if( container->type && !container->type->pockets.empty() ) {
        for( const pocket_data &data : container->type->pockets ) {
            pockets.emplace_back( container, &data );
        }
        return;
    }
    // Items with no storage at all still get one pocket, so every
    // fan-out method has something to iterate.
    default_pocket_data.type = pocket_type::CONTAINER;
    default_pocket_data.max_contains_volume = 0_ml;
    pockets.emplace_back( container, &default_pocket_data );
}
```

The `pocket_data` pointers are stable because `itype::pockets` is populated during finalization and never mutated afterwards.

- [ ] **Step 5: Run the full suite**

Run: `out/build/cse-vcpkg/tests/RelWithDebInfo/cata_test-tiles.exe`
Expected: all green. The volume and encumbrance tests are the ones most likely to catch a wrong mapping here.

- [ ] **Step 6: Commit**

```bash
git add src/itype.h src/item_factory.cpp src/item_contents.cpp tests/item_pocket_test.cpp
git commit -m "feat: synthesize pockets from legacy storage fields"
```

---

### Task 5: Whole-game verification

**Files:** none modified. This task is evidence gathering.

- [ ] **Step 1: Clean build**

Run: `cmake --build out/build/cse-vcpkg --config RelWithDebInfo --parallel 6`
Expected: zero errors, and no new warnings in `item_contents.cpp` or `item_pocket.cpp`.

- [ ] **Step 2: Full test suite**

Run: `out/build/cse-vcpkg/tests/RelWithDebInfo/cata_test-tiles.exe`
Expected: all tests pass. Record the assertion count and compare it against the count from before Task 1 — a large drop means tests were silently skipped.

- [ ] **Step 3: Launch and load a world**

Run the game, create a new world, spawn a character, put an item in a backpack, drop it, pick it up, save and reload.
Expected: no debug messages, contents intact after reload.

- [ ] **Step 4: Load a pre-pocket save**

Load a save created before Task 3.
Expected: the character's inventory is intact and no item is lost. This is the Global Constraint "migration never destroys an item" being checked for real.

- [ ] **Step 5: Commit any fixes and tag the phase**

```bash
git commit -am "fix: <whatever step 3 or 4 turned up>"
git tag phase-1-pocket-core
```

---

## Self-Review

**Spec coverage.** Architecture → Tasks 1 and 2. Legacy synthesis → Task 4. Save migration → Task 3. Testing → tests embedded in every task, plus Task 5. Classic mode, `pocket_data` JSON loading, `best_pocket()`, `favorite_settings`, the organization UI, and pocket templates are deferred to Phases 2–5 and listed in the phasing table — out of scope for this plan, not missing from it.

**Type consistency.** `pocket_type`, `pocket_data`, `item_pocket::contain_code`, `get_pockets()`, `definition()`, `remaining_volume()`, `all_items_top()`, `synthesize_pockets_from_legacy()` and `has_only_special_pockets()` are spelled identically everywhere they appear.

**Known soft spots**, flagged rather than hidden. Three call signatures were written from the API surface rather than from reading the function bodies: `ret_val::make_failure`, `location_vector::erase( iter, &detached )`, and the item-array read helper in `item_contents::deserialize`. Each carries an explicit instruction to match the real signature and not to change the test expectations. These are the only places where the executing engineer must check before typing.
