# Session Handoff — 2026-09-02

Supersedes `SESSION_HANDOFF_2026-09-01.md`, which stays for the reasoning behind the
pocket destination work. Covers the evening of 09-01 into the small hours of 09-02.

**HEAD:** `93b678746f` on **`fix/pocket-consumers`**, three commits ahead of `main`.
`main` is `d27e883511`, a merge of upstream BN. Everything is on GitHub — see
**The GitHub fork** at the bottom, which is no longer a plan.
Full suite: **1,088 cases, 1,084 passed**, the four `vision_*` failures environmental
(see CLAUDE.md).

This file covers two sittings: the overnight pocket work, and a second session on the
afternoon of 09-02 that put the repo on GitHub, merged upstream, and closed the three
open pocket-consumer gaps. **Read "What landed on the afternoon of 09-02" first** — the
sections below it are kept for their reasoning, not their status.

`CLAUDE.md` was rewritten this session. Its build section was materially wrong — it
described a BuildTools 18.8 workaround with `DevEnvDir` and `VCPKG_MANIFEST_INSTALL=OFF`
that no longer exists. The preset now pins Community `18.9.12120.119` and needs none of
it. If you read the old version, discard what you remember.

## What landed on the afternoon of 09-02

### CSE is on GitHub

`https://github.com/GoatBoy-11/Cataclysm-SE` — a **public fork** of Cataclysm-BN.
`origin` is the fork, `upstream` is BN. `origin/main` holds CSE. BN's `main` as it
stood at the fork is preserved on `origin/bn-main`, and `origin/cse-main` is a leftover
duplicate of `main` that is safe to delete.

The fork route is not a preference, it is the only cheap one: a fork shares object
storage with its parent, so only CSE's own 35.7 MB of commits upload rather than the
7.1 GB history, which is past GitHub's ~2 GB single-push limit. A fork of a public repo
**cannot be made private** — that is the whole reason this is public, and it was
Oliver's stated second choice.

Sync upstream with `git fetch upstream && git merge upstream/main`. **Never use
GitHub's "Sync fork" button**: it would overwrite CSE's `main` with BN's.

### Upstream merged: 14 BN commits

`d27e883511` merges `upstream/main` (`11bf4a4821`). Textually clean — 75 upstream files
against CSE's 287, 15 overlapping, zero conflicts. Central lab overhaul, rotatable
nesteds, fake enchantment items, multiple `use_action`s per item, batched item process
fixes, translocators on mapbuffers.

Three things to know:

- `inventory_selector::add_bionics_items` is now `add_fake_items`. CSE had added no
  call sites, so it merged clean.
- `src/translocator_utils.{cpp,h}` and `tests/translocator_utils_test.cpp` are **gone**.
- `data/json/mapgen/lab/lab_central.json` moved to `obsoletion/lab_central_old.json`.

`map::is_map_cache_valid` has a **missing return** on the out-of-bounds-z path
(`src/map.cpp`, from upstream `17a0df280e`). It predates this merge and is not a
regression, but it is real UB and a one-line fix if anyone wants it.

### The three open pocket-consumer gaps are closed

All on `fix/pocket-consumers`, **pushed but not merged** — Oliver reviews before it
lands. Each fix was falsified before being believed: broken deliberately, watched to
fail, restored.

| Commit | What |
|---|---|
| `16caf1d6ac` | `i_add_or_drop` routes into worn pockets again |
| `ecf05eb9e6` | AIM shows and addresses pocketed items |
| `93b678746f` | NPCs see food and item value in their own pockets |

`16caf1d6ac` — the fence really was the missing `on_pickup()`, as the previous handoff
guessed. Routing restored, and the full **ordered** suite leaves
`check_submap_active_item_consistency` clean. `wish.cpp`'s hand-rolled
route-then-fall-back is gone with it.

`ecf05eb9e6` — the pane now lists worn `CONTAINER` pockets' contents; `MOD`, `MAGAZINE`
and `CASINGS` stay out, being part of their item. Move and examine no longer resolve
`sitem->idx` through `Character::i_at()`. "Move all" walks the pane rather than
`0..inv_size()`, which is also what makes the destination free-volume check count what
is about to move. Volume and weight of pocketed entries are deliberately added to the
`AIM_INVENTORY` square: the capacity check needs them, at the cost of double-counting
against `AIM_WORN`, whose garments already include their contents' weight.

`93b678746f` — `decide_needs()` and `update_worst_item_value()` each gained a pocket
scan beside their flat-inventory scan. Mostly inert in practice, since NPC needs are
usually disabled by mod; done because it was thirty lines and testable, not because it
was urgent.

### Not covered, and worth knowing

- **Nested containers stay one level deep.** A backpack inside a worn vest is listed;
  the backpack's own contents are not. That matches what AIM already did for a backpack
  in the flat inventory, so it is consistent rather than a new gap.
- **`move_all_items` trusts `spane.items` being fresh.** The AIM loop redraws before
  reading input, and the `AIM_ALL` branch above it already relied on the same thing, so
  this is existing practice rather than a new assumption.
- **The playtest exe is a Ninja build**, copied into the MSVC tree's output path and
  rotated normally. Its DLL imports are byte-identical to the previous MSVC exe's, so
  nothing new is needed at runtime. The next `cmake --build` of `cse-vcpkg` overwrites
  that staged copy, which is fine.

### The merge worktree

`F:\Projects\CSE-merge` is a `git worktree` on `fix/pocket-consumers` with its own warm
`out/build/cse-ninja` tree. It exists because the main tree carries another model's
uncommitted mouse work, which must not be compiled into a playtest build. **Remove it
with `git worktree remove F:/Projects/CSE-merge` once the branch is merged**, or it will
quietly diverge. Its `_*.log` and `_cfg.bat` scratch files are untracked and disposable.

## What landed overnight

Five commits, oldest first.

| Commit | What |
|---|---|
| `e6377729e6` | Guards on two unchecked pocket entry points |
| `a048ebc95e` | **Pockets rebuilt on load** — the data-loss fix |
| `e10e2c45e9` | Routing into worn pockets, and the three consumers it broke |
| `8a61160c25` | Collapse a worn container's contents with `c` |
| `4720960fb3` | `+`/`-` stack counts at pickup and in trade |

Note `8a61160c25` also carries the `PICKUP` and `NPC_TRADE` keybindings for `+`/`-`,
because all three live in one JSON file. Its message does not mention them.

### The one that matters: `a048ebc95e`

**Every item loaded from a save had the wrong pockets, since the pocket port landed.**

`item_contents` takes its pockets from the type it is *constructed* with, falling back
to a single 0 ml CONTAINER pocket when there is no type yet. `item::spawn( JsonIn& )`
builds a **null** item and then deserializes into it, and `item::convert()` swaps the
type without rebuilding pockets. So a loaded backpack ended up with one 0 ml pocket
instead of its four.

Symptoms: `get_encumber_when_containing` reported no storage capacity, the debugmsg
fired on load, and the container dumped its contents on the ground. Worse and quieter,
loaded containers bypassed **every** pocket restriction, because a 0 ml fallback pocket
has no length, volume or item rules to enforce.

Found from Oliver's actual save file after the item-level and contents-level round-trip
tests both passed — the bug only appears through `item::spawn( JsonIn& )`, which is the
path a real save takes and neither test used.

## Things believed true that turned out false

- **Crafting is fine.** Reading `crafting_inventory()` suggests otherwise: it adds `inv`,
  the weapon and the worn *garments*, and `inventory::add_item_internal` does not recurse
  into contents. It nonetheless finds pocketed components — verified by test, with
  preconditions asserting the flat inventory is empty. Do not "fix" this from a code read.
- **`set_collapse()` does set `player_edited`.** An earlier session claimed it did not and
  that collapse could never persist. The claim came from a `grep` that printed only lines
  matching "collapse", so the `player_edited = true` line directly beneath was invisible.
  Collapse has always been serialized; it failed to persist only because loaded items had
  the broken fallback pocket for settings to map back to.
- **NPC merchants do not specialise.** `npc::wants_to_buy` is `at_price >= 80` unless the
  NPC is an ally, with a literal `// TODO: Base on inventory`. Cheap food not appearing in
  trade is that threshold, not a doctor/chef/gun-dealer distinction. That is CDDA.

## The routing seam and its consumers

`i_add_routed()` gives worn pockets first refusal and falls back to the flat inventory.
Used by foraging, NPC gifts, asked-for items, trade and debug spawns.

**Six consumers of the flat inventory were audited. Three were broken, one is still
broken, one is degraded, two were clean.**

| Consumer | State |
|---|---|
| Trade listing | Fixed — `init_buying` now walks worn pockets |
| Dropping | Fixed — no longer assumes a non-worn item is in `inv` |
| Consume menu | Fixed — tested the pocket kind, not the parent's itype slot |
| **AIM inventory pane** | **Broken. See below.** |
| **NPC AI** | **Degraded. See below.** |
| Crafting, `items_with` | Clean, with regression tests |

The ownership defect underneath all of it: `Character::i_add()` ends with `on_pickup()`,
which is what assigns ownership. Routing skipped it, so routed items were unowned and
invisible to anything asking `is_owned_by()` — which is exactly how trade filters. It
also skipped bucket spilling, encumbrance flagging and item pickup callbacks.

## Open, in the order worth doing

Items 1 to 3 of the previous list are **done** — see "The three open pocket-consumer
gaps are closed" above. What is left, renumbered:

0. **Playtest `fix/pocket-consumers`.** The suite is green and each fix was falsified,
   but nothing here has been played. Specifically worth trying: open AIM with a loaded
   backpack worn, and move, examine and "move all" a pocketed item in both directions.
   The repo-root exe is that build; `cataclysm-bn-tiles_old.exe` is the one before it.
1. **Import CDDA items, wallet first.** Agreed as the next job after the pocket system.
   Full survey in the memory note `cse-cdda-item-import-job`. Short version: CSE's
   `item_pocket` already reads and enforces `flag_restriction` but no CSE item uses it;
   the wallet's three flag-restricted pockets exercise it with no C++. Needs the flags
   `BANK_NOTE_SHAPED`, `CREDIT_CARD_SHAPED`, `COIN_SHAPED` adding and tagging onto
   `cash_card`, `money_bundle` and the coins. Nineteen further items have zero missing
   dependencies.
2. **`longest_side` on more items**, if wanted. 825 values were ported from CDDA for ids
   present in both trees; anything CSE-only still derives its length from volume.
3. **`map::is_map_cache_valid` missing return**, described above. Upstream's bug, one
   line, real UB.

Still open from the previous handoff and untouched: gunmod rejections (`m7` refuses four
scopes; inert, the audit is a dry run) and hauling/crafting-return paths that call
`i_add` directly.

## Process lessons that cost real time

- **A test that manufactures its own preconditions proves nothing.** The trade test set
  `set_owner()` by hand and passed while the real path left items unowned. Removing that
  exposed a *second* wrong reason: the test avatar has no valid character id, and
  `on_pickup()` skips ownership entirely without one, so nothing could ever be owned in
  tests. Both had to be fixed before the test could see the bug. **Three tests this
  session passed for the wrong reason.**
- **Break the code and watch the test fail.** Every fix here was falsified before being
  believed. That is what caught the two above.
- **A script reporting zero work *and* zero skips is reporting a bug.** A JSON pass added
  `[`/`]` to its depth counter, so in a top-level-array file objects sat at depth 2 and
  nothing matched. It looked like a clean no-op.
- **Check value legality, not just JSON validity.** A ported `"longest_side": "1 meter"`
  parsed fine and crashed the game.
- **Read the newest block of `config/debug.log`.** It accumulates every session; an old
  error near the end of the file will send you after a bug that is already fixed.
- **The exe rotation is content-based now.** Manual `cp` on top of it was destroying the
  A/B reference by overwriting `_old.exe` with a copy of the current build. Timestamps
  are not usable here: copying by hand makes the live exe newer than its own build.

## Build and test

Unchanged, see CLAUDE.md, which is now accurate. `cmake --preset cse-msvc`, build
`out/build/cse-vcpkg`, run with `CATA_TEST_COMPUTE_ACCELERATION=cpu`, rotate the exe with
`bash .claude/rotate-game-exe.sh`.

## The GitHub fork — done

Created on the afternoon of 09-02 with Oliver's explicit go-ahead, and described at the
top of this file. `gh` is authenticated as **GoatBoy-11** with `repo` scope.

Two standing limits, both still in force: **nothing is force-pushed without being asked
in so many words**, and nothing outside `F:\Projects\CSE` is ever uploaded — `CBN` and
`CDDA` are read-only reference trees.

Still untracked and not part of any commit: `gfx/ChibiUltica/` — which is a **complete, working
tileset** (24 PNGs plus `tile_config.json` and `tileset.txt`, `NAME: Chibi_Ultica`) and
should already be selectable in game — and seven loose `Chibi*.png` under
`gfx/MSX++UnDeadPeopleEdition/`, which are **inert**: that tileset's config references
none of them. Ask before adding either.

Also untracked and belonging to the other model: `test.full.log`, `test.full.err.log`,
and two plan docs under `docs/superpowers/plans/` for the mouse and construction-preview
work.
