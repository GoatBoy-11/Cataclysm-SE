# Session Handoff — 2026-08-31 (end of day)

**HEAD:** `d70026c74d`. Working tree clean. Full suite: **1,034 cases, 1,030
passed**, the four `vision_*` failures environmental (see CLAUDE.md).

## Where the pocket system actually stands

It works and is no longer harmful, but it is not yet the CDDA experience.

**Working in play:**
- Pickup routes items into worn pockets by the player's priorities and rules,
  the whole pickup rather than only its first item.
- A garment's capacity is what its pockets hold (jeans 4.66 L, not 500 ml).
- Clothing keeps its own name when it holds something.
- Guns keep their spent casings; the debugmsg storm is gone.
- `U` on a pocketed garment empties it.
- Classic mode behaves exactly like stock BN.

**Not working yet, and the next job:** you cannot see what a pocket holds in
the inventory, and cannot take out a single item without emptying the garment.
That is the nested inventory display —
`docs/superpowers/plans/2026-08-31-pocket-nested-inventory.md`. Oliver's
hierarchical mockup is the target. **This was mis-called "cosmetic" earlier in
the day; it is not.** Routing items into pockets no screen can open takes them
away from the player.

## Today's commits, oldest first

| Commit | What |
|---|---|
| `aea68c998e` | sealing and preserving belong to the pocket |
| `71fe1de2d7` | pocket tooling off `D:` |
| `209e68093e` | CLAUDE.md and handoff corrected |
| `0ecebf16ff` | item info and organizer show pocket contents |
| `4620566dba` | CASINGS pocket - fixes RELOAD_EJECT debugmsg spam |
| `ebb888863c` | pickup routes into worn pockets (milestone 1) |
| `b7a0e57a15` | pickup refuses what no pocket holds (milestone 3) |
| `1091f4cff2` | overflow drop cannot walk off an empty inventory |
| `8f1eea572e` | Ninja build tree documented |
| `6fb9a17d8d` | a garment holds what its pockets hold (capacity) |
| `20536f5f84` | clothing keeps its own name |
| `3972bdf9f4` | a garment can be emptied of what was routed in |
| `87bbeb3541` | pride_flags out of the default mod loadout |
| `d70026c74d` | route the whole pickup, not just its first item |

## Open, in the order worth doing

1. **Nested inventory display** (plan written). Oliver's reports 3 and 4.
2. **Gunmod rejections.** Oliver's `pocket_audit.txt` shows `m7` refusing
   `holo_sight`, `acog_scope`, `muzzle_brake` - 18 insertions - though it has
   two MOD pockets. Harmless now (the audit is a dry run and mod installation
   does not take the enforced path), but wrong, and it would bite if
   enforcement ever reaches mods.
3. **Pockets that hold nothing.** `tacvest` has an authored `0 ml` pocket;
   `ammo_pouch`, `bandolier_*`, `quiver*`, `stone_pouch`, `grenade_pouch` and
   the power-armor pouches have synthesized `0 ml` ones. Their capacity lives
   in fields the import did not translate.
4. **Routing coverage.** Only pickup routes. Hauling, AIM transfers, crafting
   returns and unload still call `i_add` directly. Safe (nothing is lost),
   deliberately inconsistent. If Oliver reports intermittent routing again,
   these are the first suspects.
5. **Item info still prints legacy `storage`** in `armor_layers.cpp:301` and
   `game_inventory.cpp:336`, so a garment can advertise 500 ml while holding
   4.66 L. Cheap to align.

## Two things that decided today's design

- **Capacity comes from pockets** (Oliver chose this over capping to BN's old
  numbers). Volume roughly doubles on the 131 item types with imported pockets,
  so weight now binds first. If it plays too loose, tune the pocket volumes in
  JSON - content, not code.
- **Classic mode must stay indistinguishable from stock BN.** Where the
  difference is structural, gate on the mode; where the data makes it moot,
  say so and prove it.

## Test-harness gotchas that cost real time

- The test avatar has `DEBUG_STORAGE`: infinite capacity, so **no test using
  `g->u` can exercise an over-capacity path.** Use `standard_npc`.
- `wear_item` costs moves; reset `u.moves = 100` AFTER wearing or
  `do_pickup`'s loop never starts.
- Capture an item reference from `map.i_at()` AFTER `add_item_or_charges`.
- Item identity does not survive `i_add`'s restacking, and `test_rock` merges
  by charges. Assert on `typeId()` and counts.
- `pick_one_up` returns true on a silent cancel, so `do_pickup`'s return is not
  proof of a pickup.

## Build

See CLAUDE.md. Short version: `cmake --preset cse-msvc`, build
`out/build/cse-vcpkg`, run tests with `CATA_TEST_COMPUTE_ACCELERATION=cpu`.
A `cse-ninja` preset builds in about half the time but needs a vcvars64 shell.
**The exe in the repo root is a copy and goes stale on every build** - re-copy
it, and check the version stamp in `debug.log` against `git log` before
trusting a bug report.

## Standing constraints

- All 181 existing `.contents.` call sites compile unchanged; fix
  `item_contents`, never the call site.
- No item is ever destroyed. Refusal falls back; only bad data force-inserts.
- Classic worlds and old saves behave exactly as stock BN.
