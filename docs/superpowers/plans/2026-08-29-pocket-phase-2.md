# Pocket Phase 2 Implementation Plan

**Goal:** Load `pocket_data` from JSON with CDDA's schema, and make an item with more than one pocket behave correctly, so CDDA content and curated CSE items can declare real pockets as data.

**Blocked by:** Phase 1 (`phase-1-pocket-core`, complete).

**Spec:** `docs/superpowers/specs/2026-08-29-pocket-system-design.md`

## Scope

In:
- `units::length` JSON parsing (Phase 1 added the type with no reader).
- `pocket_type` enum string mapping.
- `pocket_data` JSON deserialization, schema identical to CDDA's.
- `itype` loading of a `"pocket_data"` array, interacting correctly with `copy-from`
  and with the Phase 1 legacy synthesis guard.
- Multi-pocket insertion: choose the first pocket that accepts the item.
- A curated multi-pocket proof item in `data/mods/TEST_DATA`.

Out, deliberately:
- **Mass-authoring 50-150 curated base-game items.** That is balance work, and the
  spec warns that over-granting pockets is "a balance bug wearing the costume of a
  feature". Base-game content stays on synthesis until a human sets the balance.
- `best_pocket()` ranking, `favorite_settings`, the organization UI (Phase 3).
- Classic mode (Phase 4), pocket templates (Phase 5).

## Global Constraints

- **Behaviour for existing items must not change.** Nothing in base-game JSON declares
  `pocket_data`, so every existing item stays on synthesis and keeps exactly one pocket.
- **Insertion must never fail.** Phase 1 removed `can_contain` enforcement from
  `insert_item` because synthesis produces only CONTAINER pockets, leaving gunmods,
  magazines and corpse contents with nowhere that fits. Phase 2 may *prefer* a pocket
  that fits, but must still fall back to pocket 0 rather than reject.
- `pocket_data` field names stay CDDA's. Do not rename.
- Configure: `cmake --preset cse-msvc`
- Build: `cmake --build out/build/cse-vcpkg --config RelWithDebInfo --parallel 6`
- Tests: `out/build/cse-vcpkg/tests/RelWithDebInfo/cata_test-tiles.exe`
- `tests/` glob has no `CONFIGURE_DEPENDS`; re-run the preset after adding a test file.
- Assertion counts vary by several thousand between runs of the same binary. Judge the
  suite by pass/fail and test-case count, not assertion count.

---

### Task 1: `units::length` JSON support

**Files:** `src/units.h`, `src/generic_readers.h`, `tests/item_pocket_test.cpp`

- [ ] Add `units::length_units` table: `mm`, `cm`, `m`.
- [ ] Add `length_reader` alongside `volume_reader` in `generic_readers.h`.
- [ ] Test: reading `"30 cm"` yields `30_cm`; `"5 mm"` yields `5_mm`.

### Task 2: `pocket_type` enum strings and `pocket_data` deserialization

**Files:** `src/item_pocket.h`, `src/item_pocket.cpp`, `tests/item_pocket_test.cpp`

- [ ] `io::enum_to_string<pocket_type>` covering CONTAINER, MAGAZINE, MAGAZINE_WELL,
      MOD, CORPSE, MIGRATION, plus `enum_traits<pocket_type>::last`.
- [ ] `void pocket_data::deserialize( JsonIn & )` / `load( const JsonObject & )` reading
      `pocket_type`, `max_contains_volume`, `max_contains_weight`, `max_item_length`,
      `rigid`, `watertight`, `sealed`, `spoil_multiplier`, `moves`.
- [ ] Test: a JSON object round-trips into the expected field values, and an omitted
      field keeps its default.

### Task 3: Load `pocket_data` on `itype`

**Files:** `src/item_factory.cpp`, `tests/item_pocket_test.cpp`

- [ ] Read a `"pocket_data"` array in `load_basic_info` into `def.pockets`.
- [ ] Confirm the Phase 1 guard still holds: an item declaring `pocket_data` is skipped
      by `synthesize_pockets_from_legacy`, and declaring both fires the existing debugmsg.
- [ ] Confirm `copy-from` propagates `pocket_data` like any other field.

### Task 4: Multi-pocket insertion and a curated proof item

**Files:** `src/item_contents.cpp`, `data/mods/TEST_DATA/items.json`,
`tests/item_pocket_test.cpp`

- [ ] `insert_item` walks pockets and uses the first whose `can_contain` succeeds,
      falling back to `pockets.front()` when none do. Never returns failure.
- [ ] Author one multi-pocket test item in TEST_DATA with two differently sized pockets.
- [ ] Test: the item exposes two pockets with the authored volumes; a large item lands
      in the large pocket; contents survive a serialization round trip across pockets.

### Task 5: Verification

- [ ] Clean build, no new warnings in the changed files.
- [ ] Full suite green, test-case count up by the new tests and nothing skipped.
- [ ] Confirm a synthesized item still reports exactly one pocket, proving base-game
      behaviour is untouched.
