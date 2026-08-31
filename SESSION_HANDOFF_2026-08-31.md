# Session Handoff — 2026-08-31

## Current State

**HEAD:** `9323d082b9` — feat: freeze the pocket system to the world it was made in

All six pocket plans have landed on `main`. What remains is verification, not
implementation.

## Pocket Port — What Shipped

| Plan | Status |
|---|---|
| `2026-08-29-pocket-core.md` (Phase 1) | landed |
| `2026-08-29-pocket-phase-2.md` (JSON schema) | landed |
| `2026-08-30-pocket-phase-3.md` (restrictions, enforcement) | landed |
| `2026-08-30-put-in-refactor.md` | landed |
| `2026-08-30-pocket-classic-mode.md` | landed |
| `2026-08-30-pocket-favorites.md` (organization UI, presets) | landed |

**The plans' checkboxes lie.** Five of the six read as entirely unticked despite
their work being committed; only the favorites plan was kept current, and even its
one open "presets" item shipped in `b67e8169ab`. Read `git log`, not the boxes.

## Uncommitted Work In Flight

Per-pocket sealing and preserving — sealing is a property of the compartment, not of
the whole item, so a coat with a sealed inner pocket no longer preserves what is in
its sleeve.

- `src/item.cpp` — `pocket_holding()`; `is_in_preserving_container()` and
  `is_in_sealing_container()` now walk parent *and* child to find the actual pocket,
  consulting per-pocket `spoil_multiplier` and `sealed`.
- `src/item_contents.{h,cpp}` — new `pocket_containing()`, identity-compared.
- `src/item.h` — those two predicates moved from `private` to `public` so the tests
  can reach them. CBN keeps them private, so this is 2 lines of upstream drift. If
  that is unwanted, the tests need a public seam to go through instead.
- `tests/item_pocket_test.cpp`, `data/mods/TEST_DATA/items.json` — 3 new tests and a
  `test_sealed_pocket_box` fixture.

This work is on no plan. It postdates all six.

## Last Verified Run — 2026-08-31

From-scratch configure and build, ~25 min. Suite took 548 s.

- **Full suite: 1,006 cases, 1,002 passed, 4 failed.**
- **Pocket tests: 93 cases, 276 assertions, all pass.**
- The 4 failures are `vision_*` at `tests/vision_test.cpp:256` and are environmental —
  see the compute-backend note in `CLAUDE.md`. They are not pocket-related, though this
  has not been confirmed against a clean tree.

## Next: Playtest

Build and run from `F:\Projects\CSE`. `D:` is dead.

1. **Priority routing:** open container, `P` or `o`, set priority on one pocket, pick up
   items, confirm they route to the high-priority pocket.
2. **Item rules:** bar an item type from a pocket, pick up that type, confirm it goes
   elsewhere.
3. **Persistence:** save, reload, reopen the pocket menu, confirm rules still apply.
4. **Presets:** save a preset, apply it to a second pocket.
5. **Classic mode:** set `POCKET_SYSTEM` to classic, confirm `P` says there is nothing
   to organize and rules do not apply in play.

## After Playtest

- Delete the now-unused `put_in_unchecked` definition (held pending playtest clearance).
- Decide on the `item.h` visibility change above.
- Tick the plans' checkboxes, or delete them as spent.

## Constraints (Always)

- All 181 existing `.contents.` call sites must compile unchanged. Fix `item_contents`,
  never the call site.
- Migration never destroys an item.
- The pocket system must toggle on and off without breaking saves or gameplay.

## Key References

- Design spec: `docs/superpowers/specs/2026-08-29-pocket-system-design.md`
- CDDA reference pinned at `5b915aea09`. Do not track their `master`.
