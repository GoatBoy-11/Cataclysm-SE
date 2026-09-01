# Session Handoff — 2026-09-01 (overnight session)

Supersedes `SESSION_HANDOFF_2026-08-31.md`.

**HEAD:** `a239cbd5e3` on branch **`pocket-routing-unload-and-newchar`**.
`main` is still at `aa7393757a` — the branch has not been merged.
Full suite: **1,048 cases, 1,044 passed**, the four `vision_*` failures
environmental (see CLAUDE.md).

## Merge this first

```sh
git checkout main && git merge pocket-routing-unload-and-newchar
```

Clean fast-forward. Three commits:

| Commit | What |
|---|---|
| `332b1c6007` | route unloaded and starting items into pockets |
| `30cb57206c` | show pocket contents nested in the inventory |
| `a239cbd5e3` | tick the nested inventory plan |

## What landed

**Routing now covers three paths, not one.** Pickup already routed; unload and
character creation now do too.

- Unload offers each freed item to worn pockets before the flat inventory,
  passing the container being emptied as an exclusion. Without that exclusion
  `U` on a worn garment routes its contents straight home and reads as a no-op,
  and the reinsertion happens inside the iteration doing the removal.
- Character creation stows the starting kit **after** the profession loop, not
  during it: armour is worn inside that same loop, so nothing has pockets to go
  into until the whole kit is handed out. Lives in
  `Character::stow_loose_inventory_into_pockets()` so it is testable —
  `avatar::create()` drives the creation UI and cannot be unit-tested.
- Unload **never drops**. Everything unloaded is already carried and already
  counted against capacity, so the flat inventory costs nothing. An earlier
  draft dropped on refusal and tipped the contents of the only worn garment
  onto the floor; `unloading_a_garment_empties_its_pockets` caught it.
- Pickup still *does* drop on refusal, and that asymmetry is deliberate: a
  picked-up item is new to the player, so refusing leaves it visible on the
  ground.

**Nested inventory display (milestone 2 of the plan) is done.** Worn pockets'
contents appear indented under their garment, carry a parent pointer and indent
level, and can be acted on directly. `visit_items` already recursed into
pockets — nested items were being seen and filtered out, so the gap was
narrower than the plan assumed. Ordering needed a post-sort pass because
`prepare_paging` sorts by category then name and scatters children away from
their container; unplaced children are appended rather than dropped.

Classic mode routes nothing and nests nothing, in all three paths.

## Player-visible change worth watching

**Unloading a gun while wearing a backpack now puts the magazine in a pocket,
not the loose inventory list.** That is the fix working, but it is the biggest
behavioural shift of the session. `reloading_test.cpp` asserted the old
placement and its expectation was updated — an existing test changed, not a new
one added, which is worth knowing in review.

## Not verified — the user's playtest is outstanding

Nothing below has been seen by a human. The suite cannot judge any of it.

- Pick items up, open inventory: nested under the garment, actionable directly.
- Two garments both holding things: each child under its own parent.
- A classic world: identical to stock BN.
- Roll a fresh character: does a real profession's kit land sensibly across
  real clothing? Tests only prove a vest and a rock.
- Indentation renders as expected. Tests assert `entry.indent == 1`, **not**
  that `get_entry_indent` draws two extra columns. Only eyes can confirm that.

## The GitHub fork — DO NOT DO THIS UNLESS THE USER ASKS

The user has chosen the fork route. **They have explicitly said not to create
it yet.** Do not create the repository, do not add remotes, do not push, and do
not run any part of the setup below unless they ask for it in so many words.
Publishing is outward-facing and irreversible; their decision to go this route
is not permission to execute it.

When they do ask:

- `gh` is authenticated as **GoatBoy-11** with `repo` scope. Capability is there.
- **Use GitHub's fork mechanism**, not a fresh standalone repo. A fork shares
  object storage with its parent, so only CSE's own commits upload — seconds,
  not hours.
- **Why a standalone repo does not work:** `git rev-list --disk-usage --objects
  main` is **6.96 GB**. That is measured, not estimated. GitHub caps a single
  HTTPS push around 2 GB, and strongly recommends repos stay under 5 GB.
  Chunked pushes could technically get there but would land a repo GitHub
  actively discourages.
- **The bulk is inherited, not ours.** Working tree is 5,384 files; `.git` holds
  591,232 objects back to root commit `69ffbb2953`. The largest historical blobs
  are `android/app/deps.zip` (42.5 MB, two revisions), `gfx/Coleen32Tileset/
  Coleen.png` (25.9 MB), translation `.po` files at 11–13 MB across many
  revisions, `unifont.ttf` (11.7 MB), and several 12 MB `cataclysm` binaries
  committed upstream years ago.
- **Nothing irrelevant is tracked.** `out/` is ignored at `.gitignore:89` with 0
  tracked files. CBN and CDDA are sibling directories outside this repo, 0
  tracked files. Zero tracked `.exe`, `.pdb`, `debug.log` or save files. This was
  checked; the size is not a hygiene problem.
- **The tradeoff the user accepted:** a fork of a public repo cannot be private.
- Per CLAUDE.md, once it exists: rename `origin` (currently upstream BN) to
  `upstream`, add the new fork as `origin`.
- Untracked in the tree and **not** part of any commit so far: `gfx/ChibiUltica/`
  and seven `Chibi*.png` files under `gfx/MSX++UnDeadPeopleEdition/`. Ask before
  adding them; they predate this session.

## Open, in the order worth doing

1. **Playtest** the three landed features. Cheapest bug-finding in this project —
   the last one found a worldgen crash and the routing gaps in minutes.
2. **Pockets that hold nothing.** `tacvest` has an authored `0 ml` pocket;
   `ammo_pouch`, `bandolier_*`, `quiver*`, `stone_pouch`, `grenade_pouch` and the
   power-armour pouches have synthesized ones. Their capacity lives in fields the
   import did not translate. Live player impact — a quiver that holds nothing now
   *refuses* arrows under enforcement. JSON content, so near-zero merge cost.
3. **Item info still prints legacy `storage`** at `armor_layers.cpp:301` and
   `game_inventory.cpp:336`. A garment advertises 500 ml while holding 4.66 L.
   Two lines, and it now directly contradicts the nested display.
4. **Routing coverage.** Hauling, AIM transfers and crafting returns still call
   `i_add` directly. Unload is done. Safe, deliberately inconsistent. First
   suspects if routing looks intermittent again.
5. **Gunmod rejections.** `m7` refuses `holo_sight`, `acog_scope`, `muzzle_brake`
   despite two MOD pockets. Inert — the audit is a dry run and mod installation
   does not take the enforced path.

## Process lessons that cost real time this session

- **Never chain a test run after a build without checking the build's own exit
  code.** A broken edit failed the build, the chained test ran the *stale* binary
  and printed "All tests passed". Only an explicit `echo EXIT=$?` caught it. This
  is the trap CLAUDE.md already warns about, hit twice.
- **Never pipe `cmake --build` through `tail`** — the shell reports the pager's
  status. Did it once, had to kill and rerun.
- **A test written in the same pass as its fix can be worthless.** The ordering
  test passed with the feature disabled: `select()` does not trigger
  `prepare_paging`, so entries stayed in insertion order, which is already
  parent-then-child. Use `select_item_type()` to force paging. Two builds saved
  by skipping the red state cost four to recover.
- Beware `grep -c` as a command chain's last statement — zero matches exits 1 and
  the whole task reports failure.

## Build

Unchanged, see CLAUDE.md. `cmake --preset cse-msvc`, build `out/build/cse-vcpkg`,
run tests with `CATA_TEST_COMPUTE_ACCELERATION=cpu`. Check the binary timestamp
against the clock before trusting any result.
