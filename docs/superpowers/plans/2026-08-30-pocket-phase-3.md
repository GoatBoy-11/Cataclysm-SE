# Pocket Phase 3 Implementation Plan

**Goal:** Make pockets *selective* — each pocket type accepts only what belongs in it — then turn `can_contain()` enforcement on. This is the phase where pockets first affect play.

**Blocked by:** Phase 2 (`phase-2-pocket-json`) and the post-Phase-2 work through `91af89eced`.

**Spec:** `docs/superpowers/specs/2026-08-29-pocket-system-design.md`

## Why enforcement cannot simply be switched on

The dry-run audit added in `91af89eced` reports clean, but that result is weaker
than it looks and must not be read as a green light on its own:

- Synthesized MAGAZINE, MAGAZINE_WELL, MOD and CORPSE pockets are **unbounded**
  (`100000000 ml`). They accept anything, so "some pocket accepted it" is nearly
  always true and proves little.
- Only CONTAINER pockets carry real limits. For those the audit is meaningful:
  nothing in 944 tests or a human play session tripped a volume limit.

So enforcement today would be a near-no-op for guns and magazines, and the real
risk lives in the *restrictions this phase adds*. Those are new logic; no audit
can pre-validate them. The audit becomes the regression net once they exist.

## Global Constraints

- **The audit must stay clean.** After each task, `[audit]` passes and the report
  shows no misses. A miss means the restriction is wrong, not that the audit is.
- Restriction field names follow CDDA's `pocket_data` schema: `ammo_restriction`,
  `flag_restriction`, `item_restriction`. Do not invent names.
- Enforcement goes on **last**, in one reversible commit, with a human playing
  afterwards. Its failure mode is "the game feels wrong", which no test catches.
- Configure: `cmake --preset cse-msvc` (also refreshes the stale version stamp)
- Build: `cmake --build out/build/cse-vcpkg --config RelWithDebInfo --parallel 6`
- Tests: `out/build/cse-vcpkg/tests/RelWithDebInfo/cata_test-tiles.exe`
- Judge the suite by pass/fail and case count; assertion counts drift by thousands
  between identical runs.

---

### Task 1: Ammo restrictions on MAGAZINE pockets

**Files:** `src/item_pocket.{h,cpp}`, `src/item_factory.cpp`, `tests/item_pocket_test.cpp`

- [ ] Add `std::map<ammotype, int> ammo_restriction` to `pocket_data`, loaded from
      CDDA's `ammo_restriction` object.
- [ ] `can_contain()` rejects an item whose ammo type is absent from a non-empty
      `ammo_restriction`, and rejects charges beyond the mapped capacity.
- [ ] Synthesis fills it: from `islot_magazine::type` + `capacity` for magazines,
      and from the gun's own ammo + `clip` for internal-clip guns.
- [ ] Tests: a 9mm magazine accepts 9mm and rejects .45; capacity is respected;
      the `[audit]` sweep stays clean.

### Task 2: Magazine restrictions on MAGAZINE_WELL pockets

**Files:** `src/item_pocket.{h,cpp}`, `src/item_factory.cpp`, `tests/item_pocket_test.cpp`

- [ ] Add `item_restriction` (a set of `itype_id`) to `pocket_data`.
- [ ] Synthesis fills it from `itype::magazines` — the exact set of magazines the
      gun accepts.
- [ ] Tests: a Glock accepts `glockmag`, rejects an AK magazine, and still accepts
      every magazine listed in its own `magazines` map (extend the existing sweep).

### Task 3: Mod restrictions on MOD pockets

**Files:** `src/item_pocket.{h,cpp}`, `src/item_factory.cpp`, `tests/item_pocket_test.cpp`

- [ ] MOD pockets accept only gunmods whose `location` appears in the gun's
      `valid_mod_locations`, respecting the per-location count.
- [ ] Decide deliberately whether this lives in `pocket_data` or stays gun logic
      consulted by `can_contain()`; CDDA uses `flag_restriction`, but BN's mod
      locations are richer than flags. Record the choice in the commit message.
- [ ] Tests: a valid mod is accepted, a mod for a location the gun lacks is
      rejected, and the location count is not exceeded.

### Task 4: `best_pocket()`

**Files:** `src/item_contents.{h,cpp}`, `tests/item_pocket_test.cpp`

- [ ] Replace first-fit in `insert_item` with `best_pocket()`: prefer the pocket
      whose restrictions actually name the item over a merely-permissive one.
- [ ] Tests: a magazine goes to the MAGAZINE_WELL and not to a roomy CONTAINER
      pocket on the same item; ammo goes to the MAGAZINE.

### Task 5: Turn enforcement on — DEFERRED, see below

**This task was stopped before implementation.** Tasks 1-4 shipped as
`phase-3-pocket-restrictions` (`98e210d6d7`, 953 tests green).

`item::put_in` returns `void` and ignores `insert_item`'s result
(`src/item.cpp:1297`). If `insert_item` starts returning failure without
consuming the `detached_ptr`, the payload falls out of scope and **the item is
silently destroyed**. There are 48 `put_in` call sites and not one of them uses a
return value.

So enforcement cannot be switched on at `insert_item` alone. It needs `put_in` to
hand the item back — `detached_ptr<item> put_in( detached_ptr<item> && )`, empty
on success — and each of the 48 sites to decide what happens to a rejected item
(drop to ground, keep in inventory, debugmsg). That is a larger job than Tasks
1-4 combined, in code paths the test suite does not fully reach, so it belongs in
its own phase with its own playtest rather than as a tail-end step here.

**What is already delivered without it:** `best_pocket()` routes a magazine to
the well, ammo to the magazine, mods to the mod pocket and everything else to
storage. Only *rejection* of items that fit nowhere is missing, and the audit
reports nothing currently needs rejecting.

Original task, for whoever picks this up:

**Files:** `src/item_contents.cpp`, `tests/item_pocket_test.cpp`

- [ ] `insert_item` returns failure when no pocket accepts the item, instead of
      recording an audit miss and inserting anyway.
- [ ] Keep `record_pocket_audit_miss()` on the rejection path so the report still
      names anything that starts failing.
- [ ] Full suite green. This is the commit to revert if play goes wrong.

### Task 6: Human verification

- [ ] Reload every weapon class: detachable magazine, internal clip, revolver,
      bow, energy weapon.
- [ ] Attach and remove gunmods. Fill and empty containers. Butcher a corpse.
- [ ] Save, reload, confirm inventory intact, no debug messages.
- [ ] Run the coverage report and the insertion audit; both should be clean.
- [ ] Tag `phase-3-pocket-enforcement`.

## Out of scope

- Classic mode world option (Phase 4).
- Curated multi-pocket base-game items — balance work needing a human.
- Pocket templates (Phase 5); the coverage report suggests ~40-60 wearable
  storage items are the whole realistic curation target, so template machinery
  may never be needed.
