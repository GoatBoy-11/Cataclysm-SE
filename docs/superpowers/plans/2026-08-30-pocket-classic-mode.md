# Pocket Classic Mode Implementation Plan

**Goal:** A world option that collapses every item's pockets into a single pooled
compartment, so players who prefer BN's original inventory can opt out of the
curated per-pocket storage.

**Blocked by:** Phase 3 restrictions and the curated import (both landed on `main`).

**Spec:** `docs/superpowers/specs/2026-08-29-pocket-system-design.md` (Phase 4)

## Why this matters now

114 wearables carry CDDA's per-pocket layouts as of `f2672614b4`. That is a real
balance change with no off switch. This plan builds the switch.

The legacy `storage` / `container->contains` fields were deliberately kept on every
curated item for exactly this purpose, and the "declares both" debugmsg was retired
so the dual declaration is not treated as a mistake.

## Global Constraints

- **Full mode must be untouched.** With the option at its default, every item must
  produce byte-identical pockets to today. A test asserts this.
- **Switching an existing world must not destroy items.** `item_contents::deserialize`
  already folds contents of surplus pockets into pocket 0, so a save written in full
  mode loads correctly in classic mode. Prove it with a test rather than trusting it.
- Configure: `cmake --preset cse-msvc`
- Build: `cmake --build out/build/cse-vcpkg --config RelWithDebInfo --parallel 6`
- Tests: `out/build/cse-vcpkg/tests/RelWithDebInfo/cata_test-tiles.exe`
- `tests/` glob has no `CONFIGURE_DEPENDS`; re-run the preset after adding a test file.
- Judge the suite by pass/fail and case count, never assertion count.

---

### Task 1: The world option

**Files:** `src/options.cpp`

- [ ] `add( "POCKET_SYSTEM", world_default, ... )` with choices `full` (default) and
      `classic`, following the `WORLD_END` string-choice pattern.
- [ ] Wording should say plainly that classic pools each item's pockets into one
      compartment using its original storage value.

### Task 2: Collapse pockets at construction

**Files:** `src/item_contents.cpp`, `tests/item_pocket_test.cpp`

- [ ] In `item_contents::item_contents( item * )`, when the option is `classic`,
      skip `itype::pockets` and build a single CONTAINER pocket.
- [ ] Its capacity comes from the legacy fields — `container->contains`, else
      `armor->storage` — exactly as Phase 1 synthesis did, so classic mode reproduces
      pre-pocket behaviour rather than summing the authored pockets. Summing would
      inherit CDDA's balance, which is the thing being opted out of.
- [ ] Non-CONTAINER pockets (MAGAZINE, MAGAZINE_WELL, MOD, CORPSE) are still needed
      for guns and magazines to function; keep synthesizing those. Classic mode is
      about *storage*, not about breaking reloading.
- [ ] Tests: with the option set to classic, cargo pants expose one pocket sized from
      legacy storage; a Glock still has its magazine well and mod pocket; with the
      option at full, everything matches today.

### Task 3: Existing-world safety

**Files:** `tests/item_pocket_test.cpp`

- [ ] Test: serialize a curated item with contents distributed across pockets in full
      mode, then deserialize it under classic mode, and assert every item survives in
      the single pocket.
- [ ] Test the reverse direction too — classic save loaded in full mode.

### Task 4: Verification

- [ ] Full suite green in the default (full) configuration.
- [ ] Full suite green with the option forced to classic, if the harness allows
      setting it; otherwise cover classic purely by targeted tests and say so.
- [ ] Coverage report run in both modes for a manual eyeball.

## Out of scope

- Any UI for switching mid-game beyond the standard world options screen.
- Collapsing MAGAZINE/MOD pockets; those are function, not storage balance.
