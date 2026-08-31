# Session Handoff — 2026-08-31 (evening update)

## Committed on `main` today, oldest first

| Commit | What |
|---|---|
| `aea68c998e` | feat: sealing and preserving belong to the pocket |
| `71fe1de2d7` | chore: pocket tooling off `D:` |
| `209e68093e` | docs: CLAUDE.md + handoff corrections |
| `0ecebf16ff` | feat: show which pocket is holding what (info + organizer contents, dead collapse toggle removed, zero-capacity pockets hidden) |
| `4620566dba` | fix: CASINGS pocket so RELOAD_EJECT guns stop debugmsg-spamming |
| `ebb888863c` | feat: pickup routes into worn pockets (milestone 1) |
| `b7a0e57a15` | feat: pickup refuses what no pocket holds (milestone 3) |

All verified: full suite green apart from the four environmental `vision_*`
failures documented in CLAUDE.md.

## The strategic decision (Oliver approved)

Do NOT port CDDA's inventory model. Graft pockets onto BN's flat inventory,
incrementally, a visible milestone at a time. The flat inventory stays as a
hidden fallback/compatibility layer, which is what keeps saves safe and every
milestone shippable. Oliver accepts balance drift; wants CDDA feel.

- Milestone 1 — pickup routes into worn pockets. Plan:
  `docs/superpowers/plans/2026-08-31-pocket-pickup-routing.md`
- Milestone 3 — pickup refuses what no pocket takes. Plan:
  `docs/superpowers/plans/2026-08-31-pocket-enforcement-pickup.md`
- Milestone 2 — nested inventory screen. PARKED, no plan yet, cosmetic;
  biggest chunk, only start when Oliver asks.

## Everything below is now COMMITTED - kept for the reasoning

Final suite after both milestones: 1,024 cases, 1,020 passed, only the four
environmental `vision_*`. Working tree clean apart from Oliver's untracked
`gfx/Chibi*` files.

## What was uncommitted (now landed)

Milestones 1 and 3 are implemented together, verification in flight:

- `Character::i_add_to_worn_pockets` (character.cpp/.h) — worn pockets get
  first refusal on pickup; priority ranks across garments; refusal falls back.
- Carried-volume model B: `volume_carried()` counts worn-pocket contents in
  full mode (matches how weight works); `can_pick_volume` asks volume_carried.
  Classic arithmetic untouched. This fixed `drop_token_test.cpp:375`.
- Pickup call site + enforcement branch in pickup.cpp: in full mode an item
  every pocket refuses is not quietly stashed — the existing problematic-pickup
  flow (wield/wear) handles it; autopickup cancels.
- `test_pocket_vest` fixture; 6 routing tests green; 3 enforcement end-to-end
  tests. Enforcement tests hit a test-harness bug (stale reference: capture the
  item from `map.i_at()` AFTER `add_item_or_charges`, never before) — fix in,
  rebuild running at handoff time.

## Next steps, in order

1. **Oliver playtests** (the only open item on both plans). Rebuild first -
   the exe in the repo root is a copy and goes stale on every build. Check:
   wear a rucksack/tacvest, set a pocket to priority 5, pick items off the
   ground - they should route there and show under `o`; bar an item type from
   every pocket, walk over it, `g` - expect the wield/wear prompt, not a quiet
   stash; classic world - everything as stock BN.
2. Tick the playtest boxes in both plan docs afterwards.
3. Milestone 2 (nested inventory screen) only when Oliver asks. Still parked,
   still cosmetic, still the biggest remaining chunk.

## Test-harness gotchas found the hard way

- `pick_one_up` returns true on a silent cancel, so `do_pickup`'s return is not
  proof of a pickup. Assert on where the item ended up.
- `wear_item` costs moves; reset `u.moves = 100` AFTER wearing or `do_pickup`'s
  loop never starts.
- Capture the item reference from `map.i_at()` AFTER `add_item_or_charges`;
  the map may not keep the object it was handed.
- Item identity does not survive `i_add`'s restacking. Assert by `typeId()` and
  counts, not by pointer.

## Build & test (unchanged)

See CLAUDE.md: `cmake --preset cse-msvc`, build `out/build/cse-vcpkg`,
run suite with `CATA_TEST_COMPUTE_ACCELERATION=cpu`. Four `vision_*` failures
are environmental. Never pipe builds through a pager. `D:` is dead.

## Standing constraints

- 181 `.contents.` call sites compile unchanged; fix `item_contents`, never
  the call site.
- No item is ever destroyed; refusal falls back, never force-inserts on the
  new paths.
- Classic worlds and old saves behave exactly as stock BN.
