# Session Handoff — 2026-09-01

Supersedes `SESSION_HANDOFF_2026-08-31.md` and the overnight version of this
file. Covers two sessions: the overnight one ending at `a239cbd5e3`, and the day
session ending at `de9b618740`.

**HEAD:** `de9b618740` on branch **`pocket-routing-unload-and-newchar`**.
`main` is still at `aa7393757a` — 21 commits behind, never merged, nothing
pushed anywhere.
Full suite: **1,070 cases, 1,066 passed**, the four `vision_*` failures
environmental (see CLAUDE.md).

## Merge this first

```sh
git checkout main && git merge pocket-routing-unload-and-newchar
```

Clean fast-forward.

## Read this before touching the open list

**The "pockets that hold nothing" item was a false alarm, and acting on it would
have done real harm.** The previous handoff said quivers and bandoliers now
*refuse* the ammo they exist to carry, because `pocket_coverage.txt` prints them
at `0 ml`. That inference was never tested, and it is wrong.

`item_pocket::can_contain()` returns on the round count **before volume is ever
consulted**:

```cpp
// Ammo capacity is counted in charges, not volume, so stop here.
return ret_val<contain_code>::make_success( contain_code::SUCCESS );
```

Every item on that list — `quiver`, `quiver_large`, the birchbark variants,
`ammo_pouch`, `stone_pouch`, `grenade_pouch`, `flintlock_pouch`, every
`bandolier_*`, and `tacvest`'s seventh pocket — carries an `ammo_restriction`, so
`0 ml` means "not measured in millilitres", not "holds nothing". Giving these
pockets a volume would impose a second and wrong constraint on pockets that
already know their own capacity.

`de9b618740` pins this with a test, because the number will look broken to the
next person who reads that report. The report is what should be fixed, if
anything: it prints `max_contains_volume` for pockets whose capacity is a round
count.

## What landed today

**Three features, each playtested and confirmed by the user.**

| Commit | What |
|---|---|
| `44cbb706fd` | Legacy pockets derive a `max_item_length`; item info stops advertising "up to 0 cm" |
| `2ffc8f40df` | Pocket contents also listed under their own category, named with their container |
| `4f88fef642` | Companion highlight no longer erases itself |

**Choosing an item's pocket**, plan
`docs/superpowers/plans/2026-09-01-pocket-destination-choice.md`, all five tasks
ticked and playtested:

- `item_contents::insert_into( size_t, detached_ptr<item>&& )` — insert by index
- `Character::pocket_destinations( const item&, const item* exclude )` — every
  worn pocket that would accept an item, priority then ascending remaining volume
- `ask_pocket_destination()` in `src/pocket_destination_menu.{h,cpp}` — a
  query-only picker that moves no ownership until the player commits
- `MOVE_TO_POCKET`, bound to **`M`** (`m` was already `MEND`)
- `POCKET_PICKUP` world option, **auto** (unchanged behaviour) or **choose**

## The length work is inert, and that is expected

`44cbb706fd` gives legacy pockets real length limits, but **no item in the tree
declares `longest_side`** — all 3,216 derive it from volume at
`item_factory.cpp:478`. A volume-derived pocket length can never refuse what the
volume check already passed, so the limit bites nothing today. It is groundwork.
The half that would make it real is authoring `longest_side` on items that are
long for their bulk: axes, shovels, spears, rifles, crowbars.

## Things believed true that turned out false

Worth reading before trusting any inherited claim.

- **The pocket organize menu already ships.** Inventory, select a container,
  press `o` — "Organize pockets", at `item_contents.cpp:887`, reached from
  `examine_item_menu.cpp`. Priority, whitelists, blacklists, collapse, disable,
  unload, and named presets saved to the config directory. It is complete.
- **Partial pickup already works.** Type the digits, then select the item;
  `itemcount` accumulates at `pickup.cpp:961`. What is missing is CDDA's
  incremental `+`/`-` keys — the `PICKUP` keybinding category has no
  increase/decrease. The user wants those eventually; the counting machinery
  already exists, so it is a small plan.
- **`quiet` does not mark the non-interactive caller.** It suppresses only the
  "you put it away" message (`character.h:1381`). A separate `allow_prompt`
  parameter now gates the pickup prompt, passed `!opts.autopickup` at
  `pickup.cpp:333` and `:473`.

## Open, in the order worth doing

1. **Merge to `main`.** Clean fast-forward, 21 commits.
2. **`longest_side` on long items.** JSON, near-zero merge cost, and the only
   thing that makes the length limits mean anything.
3. **`+`/`-` at pickup.** Small; the count machinery exists. Note the
   interaction: with `POCKET_PICKUP=choose`, picking 2 of 6 *and* choosing a
   pocket means two prompts for one action — design them together.
4. **Unload in choose mode fires a menu per item.** Emptying a 20-item backpack
   asks 20 times. `unload_all` is exempted; single unloads are not. The fix is a
   "put the rest here too" entry, deliberately left undesigned until someone has
   felt it.
5. **Gunmod rejections.** `m7` refuses `holo_sight`, `acog_scope`,
   `rifle_scope`, `muzzle_brake` — 24 insertions in the latest audit. Inert: the
   audit is a dry run and mod installation does not take the enforced path.
6. **Routing coverage.** Hauling, AIM transfers and crafting returns still call
   `i_add` directly. Deliberate; first suspects if routing looks patchy.

Two parked minors from review: `pockets_prompt_on_pickup()` lacks the
`has_option` guard its sibling `pockets_are_classic()` carries, and the new
`detached_ptr` parameter has no null guard. Neither is reachable today.

## Process lessons that cost real time

- **Five tests in one day passed for the wrong reason.** A tie-break test whose
  expected order matched insertion order; a cache test that read the accessor
  only after the write; a `quiet=true` test against a `!quiet` gate. Before
  trusting a new test, break the code it covers and watch it fail. Requiring that
  step is what caught the tie-break one.
- **A claim inherited from a handoff is not evidence.** The quiver item above
  survived two handoffs and was repeated to the user twice as the top priority.
  Read the enforcement path before acting on a symptom someone else inferred.
- **Check the build's own exit code, always.** Two subagents parked on
  backgrounded builds that were killed before their status was recoverable, and
  one test run used a stale binary. Build in the foreground with an explicit
  600000 ms timeout, redirect to a file, `echo "EXIT=$?"`, and compare the
  binary's mtime against the edited sources.
- **The version string lags a commit** whenever the build precedes the commit. A
  log reporting `e83a742dff` was running `18c1bfbdec`. Trust the exe timestamp.
- **Subagent-driven execution cost 3–5× the wall clock here**, mostly serialized
  MSVC builds and each fresh agent re-deriving the same API names. It earned its
  keep once: a reviewer caught that escaping the picker silently took a worn coat
  off, bypassing `Character::takeoff` and leaving stale encumbrance. Reserve it
  for genuinely risky diffs.

## The exe rotation rule

**After every build, rotate the exe.** The repo-root binary must reflect the
current HEAD or a human-test will use a stale binary and ghost-report regressions.

Automation: `.claude/rotate-game-exe.sh` is wired to a `PostToolUse` hook in
`.claude/settings.local.json`; both are excluded via `.git/info/exclude`. It fires
after every Bash call and no-ops unless the build tree is newer. It refuses rather
than half-rotate when `_old.exe` is locked by a running game.

**The hook only fires once the settings watcher has seen `.claude/`** — run `/hooks`
once on session start, or restart the harness. Until then, rotate by hand after
each build:

```sh
mv cataclysm-bn-tiles.exe cataclysm-bn-tiles_old.exe
cp out/build/cse-vcpkg/tests/RelWithDebInfo/cata_test-tiles.exe cataclysm-bn-tiles.exe
```

The `_old.exe` is kept for A/B testing. **Check the repo-root exe timestamp
against the build tree before trusting any playtest.**

## The GitHub fork — DO NOT DO THIS UNLESS THE USER ASKS

The user has chosen the fork route. **They have explicitly said not to create it
yet.** Do not create the repository, do not add remotes, do not push, and do not
run any part of the setup below unless they ask for it in so many words.
Publishing is outward-facing and irreversible; their decision to go this route is
not permission to execute it.

When they do ask:

- `gh` is authenticated as **GoatBoy-11** with `repo` scope. Capability is there.
- **Use GitHub's fork mechanism**, not a fresh standalone repo. A fork shares
  object storage with its parent, so only CSE's own commits upload — seconds, not
  hours.
- **Why a standalone repo does not work:** `git rev-list --disk-usage --objects
  main` is **6.96 GB**. Measured, not estimated. GitHub caps a single HTTPS push
  around 2 GB and recommends repos stay under 5 GB.
- **The bulk is inherited, not ours.** Working tree is 5,384 files; `.git` holds
  591,232 objects back to root commit `69ffbb2953`. Largest historical blobs are
  `android/app/deps.zip` (42.5 MB), `gfx/Coleen32Tileset/Coleen.png` (25.9 MB),
  translation `.po` files at 11–13 MB across many revisions, `unifont.ttf`
  (11.7 MB), and several 12 MB `cataclysm` binaries committed upstream years ago.
- **Nothing irrelevant is tracked.** `out/` is ignored at `.gitignore:89` with 0
  tracked files. CBN and CDDA are sibling directories outside this repo. Zero
  tracked `.exe`, `.pdb`, `debug.log` or save files. The size is not a hygiene
  problem.
- **The tradeoff the user accepted:** a fork of a public repo cannot be private.
- Per CLAUDE.md, once it exists: rename `origin` (currently upstream BN) to
  `upstream`, add the new fork as `origin`.
- Untracked and **not** part of any commit: `gfx/ChibiUltica/` and seven
  `Chibi*.png` files under `gfx/MSX++UnDeadPeopleEdition/`. Ask before adding
  them; they predate all of this.

## Build

Unchanged, see CLAUDE.md. `cmake --preset cse-msvc`, build `out/build/cse-vcpkg`,
run tests with `CATA_TEST_COMPUTE_ACCELERATION=cpu`. Check the binary timestamp
against the clock before trusting any result.

The SDD workspace at `.superpowers/sdd/2026-09-01-pocket-destination-choice/` was
deliberately kept rather than deleted: its ledger holds the reasoning behind
every ruling made during execution. Gitignored scratch, safe to delete once read.
