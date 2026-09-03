# Cataclysm: Slop Edition (CSE)

Read this before doing anything. It overrides `AGENTS.md` where the two disagree.

## What this project is

CSE is a personal fork of **Cataclysm: Bright Nights**, which is itself a fork of
**Cataclysm: Dark Days Ahead**. It is openly AI-built — the name is deliberate and
self-aware, not a placeholder to be "fixed".

It is not affiliated with, endorsed by, or maintained by the BN or DDA teams. Never
file issues upstream about CSE behaviour.

Upstream credit and the CC BY-SA 3.0 licence are load-bearing. `LICENSE.txt` and
credits files stay intact; add notices alongside them, never over them.

The owner is the user, who playtests every change personally. Treat "it passes the
suite" as necessary and not sufficient — see **Verification** below.

## The three folders in this workspace

| Path | What it is | May you edit it? |
|---|---|---|
| `F:\Projects\CSE` | This fork. All work happens here. | **Yes** |
| `F:\Projects\CBN` | Upstream Cataclysm-BN, for reference and diffing | **No — read only** |
| `F:\Projects\CDDA` | Cataclysm-DDA, the source of features being ported | **No — read only** |

CBN and CDDA are reference material. Read them freely to compare implementations;
never write to them. If a task seems to require editing them, you have misread the task.

`D:\Projects\CSE` is dead. Nothing is built, run, or read from there. If you are in
a `D:` path, you are in the wrong tree.

All three share the root commit `69ffbb2953`, so CDDA and CBN commits can be
cherry-picked into CSE with real three-way merges. The CDDA reference is pinned at
`5b915aea09`; do not track their `master`.

## Building on this machine

**Ignore the build commands in `AGENTS.md`** — they target Linux (`--preset linux-full`)
and will not work here. This is Windows with Visual Studio 18 Community.

```sh
cmake --preset cse-msvc
cmake --build out/build/cse-vcpkg --config RelWithDebInfo --parallel 6
```

Three **gitignored** files (excluded via `.git/info/exclude`) make this work. If a build
fails, check these before touching any tracked CMake file:

1. `CMakeUserPresets.json` — forces the `Visual Studio 18 2026` generator (the repo pins
   VS 17), pins `CMAKE_GENERATOR_INSTANCE` to the Community install *with* a `,version=`
   field, and sets `VCPKG_ROOT` to VS's bundled vcpkg. **Without `VCPKG_ROOT`, CMake
   silently builds all of SDL3 from source and then fails on missing submodules, a
   Vulkan-SDK-shadowed DirectXShaderCompiler, and missing NASM.**
2. `cse-local-overrides.cmake` — sets `CMAKE_MSVC_DEBUG_INFORMATION_FORMAT` to
   `ProgramDatabase`. Without it, upstream's `Embedded` (`/Z7`) pushes
   `cataclysm-bn-tiles-common.lib` past the 4 GB archive limit and the link dies
   with `LNK1248`.
3. `.codegpt-game.json` — launch manifest for the game-development skill.

**A Visual Studio update can break the build outright.** On 2026-08-31 Community
updated to 18.9 and temporarily lost `VsDevCmd.bat` and its bundled `VC/vcpkg`;
CMake then refused to configure at all. That was resolved by repairing the install,
and the preset now pins Community `18.9.12120.119` directly. If configure fails with
"could not find specified instance of Visual Studio", the version in
`CMakeUserPresets.json` no longer matches what is installed — check with `vswhere`
and update the pin, or repair VS (installer → Repair, or re-add "Desktop development
with C++").

**Check timestamps, not just exit codes.** A build can report success while
producing nothing — a stale exe then passes tests that never saw the change:
`ls -la out/build/cse-vcpkg/tests/RelWithDebInfo/cata_test-tiles.exe`.

Never pipe `cmake --build` through `tail` or `head`: the shell reports the pager's exit
status, so a failed build looks like a clean one. Build in the foreground with an
explicit 600000 ms timeout and redirect to a file.

### The Ninja preset (faster, second build tree)

`cse-ninja` builds the same targets in roughly half the time (8m46s against
20-25m for a cold full build). It lives in its own tree, `out/build/cse-ninja`, so both
presets can coexist; `ninja.exe` is at `F:/Projects/ninja/ninja.exe`.

**It needs a developer environment**, because Ninja calls `cl.exe` from PATH
while the VS generator finds the compiler itself. Wrap both configure and build:

```bat
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cmake --preset cse-ninja
cmake --build out/build/cse-ninja
```

Two things the preset must set, both learned the hard way:

- `CMAKE_CXX_FLAGS_RELWITHDEBINFO` / `CMAKE_C_FLAGS_RELWITHDEBINFO` to
  `/O2 /Oi /Ob2 /DNDEBUG`. The VS generator injects its own per-config
  optimisation; Ninja does not, and the repo leaves the variable at `/Oi`. Without
  this the build succeeds but is unoptimised: tests ran 26s instead of 4s, and
  startup took 90 seconds.
- `CMAKE_BUILD_TYPE`, since Ninja is single-config.

**`json_formatter` does not link under Ninja** (unresolved `replace_all`), so JSON
linting uses the VS tree's
`out/build/cse-vcpkg/tools/format/RelWithDebInfo/json_formatter.exe`.

`sccache` is installed but useless here: it only caches MSVC compilations that use
`/Z7`, and `/Z7` is exactly what `cse-local-overrides.cmake` exists to avoid.

Adding a file to `tests/` requires re-running `cmake --preset cse-msvc`: the `tests/`
glob has no `CONFIGURE_DEPENDS`, unlike `src/`.

## Testing

**This machine has no working SDL_GPU device.** The test binary defaults to the
`gpu_software` compute backend and aborts before the first test with *"SDL_GPU: device
creation failed"*. Run the suite with the CPU backend instead:

```sh
export CATA_TEST_COMPUTE_ACCELERATION=cpu
out/build/cse-vcpkg/tests/RelWithDebInfo/cata_test-tiles.exe "[optional-filter]"
```

Tag-filtered runs are the working loop: `[pocket]` finishes in hundredths of a
second, `[pocket],[routing]` in about ten. The full suite takes ~10 minutes; run it
before committing, not between edits.

The CPU path is upstream BN's fallback (`7478f040a5`, `b43ea3daff`), and its lighting
does not match the GPU path exactly: **four vision tests fail under it** —
`vision_wall_obstructs_light`, `vision_single_tile_skylight`, `vision_see_out_of_vehicle`,
`vision_see_into_vehicle`, all at `tests/vision_test.cpp:256`. Treat those four as
environmental. **Any other failure is real.**

A full run reports roughly *1,082 cases, 1,078 passed, 4 failed*. The four are the
vision tests above. Read the case counts, never an exit code: a backgrounded run
reports the harness's status, not the binary's, and Catch2's own exit code is a
failed-assertion count.

## Verification — the part that keeps going wrong

**A green suite is not evidence a change works.** This project has repeatedly shipped
tests that passed for the wrong reason. Before trusting a new test, break the code it
covers and watch it fail. That step is not optional; it has caught real defects here
more than once.

Two specific traps, both of which have bitten:

- **The test avatar is under-initialised in ways that silently disable the behaviour
  under test.** It carries `DEBUG_STORAGE`, so its carrying capacity is effectively
  infinite and no test using `g->u` can exercise an over-capacity path — use
  `standard_npc` for those. It also has **no valid character id**, and `on_pickup()`
  skips ownership assignment entirely without one, so any test touching item ownership
  must call `setID( character_id( 1 ), true )` first (see `tests/iuse_test.cpp` for the
  restore-on-scope-exit pattern).
- **A test that manufactures its own preconditions can mask the bug.** A trade test that
  called `set_owner()` itself passed while the real code path left items unowned. Assert
  the precondition, do not create it.

**Playtesting finds what the suite cannot.** Human playtests have found a
save-corruption bug, four separate item-routing gaps and a UI duplication bug that the
suite passed clean through. Expect to hand the user a build and be told what broke.

### The exe rotation rule

**After every build, rotate the repo-root exe**, or a playtest will use a stale binary
and ghost-report regressions:

```sh
bash .claude/rotate-game-exe.sh
```

It compares the built exe against the repo-root copy **by content** and no-ops when they
match, so running it redundantly is safe and preserves `cataclysm-bn-tiles_old.exe` as a
genuine previous build for A/B testing. Timestamps are not used and must not be
reintroduced: copying the exe by hand makes the live copy newer than the build that
produced it, which would suppress the check permanently.

The script and its `PostToolUse` hook in `.claude/settings.local.json` are excluded via
`.git/info/exclude`. **The hook only fires once the settings watcher has seen `.claude/`**
— restart the harness if it never runs, and rotate by hand meanwhile.

**The version string lags a commit** whenever the build precedes the commit. A log
reporting `e83a742dff` was running `18c1bfbdec`. Trust the exe timestamp. Remember JSON
is read at runtime: a data-only change needs no rebuild, and an old binary will still
pick it up.

the user's playtest logs are at `config/debug.log`, which accumulates every session —
split on `Starting log.` and read the newest block, or you will diagnose a stale error.

## Fork discipline

Every C++ edit is a future merge conflict with upstream BN. Cheapest to most expensive:

1. JSON and Lua content — near-zero merge cost
2. New files — conflict only if upstream adds the same path
3. Additive hooks in existing files
4. Edits inside existing function bodies — most expensive, use sparingly

When a change can live behind an existing seam rather than spread across call sites,
put it behind the seam. That principle is why the pocket port touches `item_contents`
instead of its 181 callers.

**Changing a seam obliges you to audit its consumers.** Routing acquired items into
worn pockets silently broke three systems that read the flat inventory directly —
trade, dropping and the consume menu — each found by playtest days apart. When you
change where data lives, grep for everything that reads the old location *before*
shipping.

Machine-local config belongs in `.git/info/exclude`, never in the tracked `.gitignore`.

## Conventions

Follow `AGENTS.md` for C++ style, formatting, JSON linting and i18n. Two exceptions:

- **Build commands** — use the Windows ones above.
- **Commit messages** — the user wants a short conventional-commit title plus short
  bullet points, always, without being asked. This overrides the "MUST NOT add body"
  rule in `AGENTS.md`.

Commit only when asked. Branch before committing to `main` for feature work; small
fixes committed straight to `main` are normal here.

When porting JSON from CDDA, **check that values are legal in CSE, not merely valid
JSON**. A ported `"longest_side": "1 meter"` parsed fine and crashed the game:
`units::length_units` knows only `mm`, `cm`, `m`.

## Current state

Branding is done: menu, window titles, memorial header, README, and userdata paths
(`cataclysm-cse`, separate from BN's). Build targets, translations and code comments
deliberately keep upstream names to hold merge cost down.

CSE is on GitHub as of 2026-09-02: `origin` is
`https://github.com/GoatBoy-11/Cataclysm-SE.git`, `upstream` is
`https://github.com/cataclysmbn/Cataclysm-BN.git`. The repo is a **public fork** of
Cataclysm-BN, because a fork shares object storage with its parent — only CSE's own
commits upload, and the 7.1 GB history does not (a standalone repo would need chunked
pushes past GitHub's ~2 GB single-push limit). A fork of a public repo cannot be made
private; that is why it is public, and it was the user's second choice.

`origin/main` holds CSE. BN's `main` as it stood at the fork is preserved on
`origin/bn-main`, and `origin/cse-main` is a leftover duplicate of `main`, safe to
delete. Sync upstream with `git fetch upstream && git merge upstream/main` — **never**
GitHub's "Sync fork" button, which would overwrite CSE's `main` with BN's.

**Do not force-push unless asked in so many words**, and never upload anything from
outside `F:\Projects\CSE`.

Work in progress and its open threads live in the dated handoff files at the repo root,
`SESSION_HANDOFF_YYYY-MM-DD.md`. **Read the newest one before starting.** It, not this
file, is the record of what is half-finished.

**Do not trust plan checkboxes** in `docs/superpowers/plans/`. Several read as unticked
despite their work being committed. Read `git log` for what actually shipped.

### Working alongside another AI

A second model has worked in this repo concurrently (a mouse-support port). Expect this
to recur. The split that worked: one model in `src/`, the other in `data/json/`. Neither
model sees the other's uncommitted tree, so commit or stash before handing over, and
write down anything you fixed in files you do not own.
