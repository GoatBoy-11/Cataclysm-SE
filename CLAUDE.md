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

## Building in the Linux web container (Claude Code on the web)

A web session runs on Ubuntu 24.04 in an ephemeral container at
`/home/user/Cataclysm-SE`, cloned fresh from `origin`. **None of the Windows
guidance above applies there** — no MSVC, no `F:` drive, no repo-root exe, no
playtest. What it can do is configure, build and run the whole test suite, and
that was verified end to end on 2026-09-08.

The curses preset is the target; tiles needs a display the container has not got.
Everything below is needed, and each line is a thing that failed without it.

```sh
apt-get install -y --no-install-recommends \
    libncursesw5-dev libsqlite3-dev zlib1g-dev gettext mold ccache astyle \
    libc++-18-dev libc++abi-18-dev
git clone --depth 1 --branch release-3.4.8 https://github.com/libsdl-org/SDL.git /tmp/SDL3

cmake --preset ci-curses \
    -DLIBBACKTRACE=OFF -DBACKTRACE=OFF \
    -DFETCHCONTENT_SOURCE_DIR_SDL3=/tmp/SDL3 \
    -DSDL_X11=OFF -DSDL_WAYLAND=OFF -DSDL_VULKAN=OFF \
    -DSDL_OPENGL=OFF -DSDL_OPENGLES=OFF -DSDL_UNIX_CONSOLE_BUILD=ON \
    -DCMAKE_CXX_FLAGS=-stdlib=libc++ -DCMAKE_EXE_LINKER_FLAGS=-stdlib=libc++
cmake --build build --target cata_test --parallel 4
```

Four traps, in the order they bite:

1. **SDL3 is fetched even for the curses build** (`CMakeLists.txt:417`, for the
   compute backend), and the container's egress proxy answers GitHub's release
   tarball URL with **403**. `git clone` of the same repo is allowed, so clone the
   tag and point `FETCHCONTENT_SOURCE_DIR_SDL3` at it.
2. **SDL3 then refuses to configure without a window system.** Turning X11 and
   Wayland off is not enough — it fails the "you probably didn't mean this" check
   until `SDL_UNIX_CONSOLE_BUILD=ON` says you did.
3. **The stock toolchain cannot compile this codebase.** It is C++23
   (`CMAKE_CXX_STANDARD 23`) and CI uses **clang 22**, which the container cannot
   install: `apt.llvm.org` is blocked by the egress policy. Of what apt does offer:
   - clang 18 + the default libstdc++ 13 → no `std::ranges::to` (`action.cpp:1111`).
   - clang 18 + libstdc++ 14 (installing `g++-14` is enough to retarget it) gets
     `ranges::to` but still **no `std::expected`** (`creature_functions.h:33`):
     libstdc++ gates `<expected>` on `__cpp_concepts >= 202002L` and clang 18
     defines `201907L`.
   - **g++-14** gets past both and then dies on
     `enchantment_condition.cpp:275`, "conflicting declaration `auto
     condition_functions`" — the tree has never been built with GCC.
   - **clang 18 + libc++ 18 builds clean.** That is the only combination that
     works, and it is why `-stdlib=libc++` is in the command above.
4. **`ccache` and `mold` are named by the preset**, so they must exist or configure
   fails on the first target.

A cold build is about 25 minutes on 4 cores. The suite takes about 15.

Run it from the repo root, with the CPU backend for the same reason as on Windows —
there is no GPU here either:

```sh
CATA_TEST_COMPUTE_ACCELERATION=cpu ./build/tests/cata_test "[optional-filter]"
```

**A full run here reports 1,187 cases, 1,181 passed, 6 failed.** Two more than the
Windows count, and neither extra one is a CSE defect:

- The **four documented vision tests** fail exactly as they do under the Windows
  CPU backend.
- **`lcmatch_uses_pinyin_search_when_enabled`** (`pinyin_test.cpp:19`) fails for
  want of locale data the container does not carry.
- **`map spawn_items nests pocket loot before placing on the tile`**
  (`loot_pocket_nesting_test.cpp:60`) fails because it is **not portable**, and
  this is worth fixing. It pins `rng_set_engine_seed( 4 )` and then relies on
  `one_in()` — through `std::uniform_int_distribution`, whose sequence is
  **implementation-defined**. Seed 4 happens to roll a nesting on MSVC and not
  under libc++. The two sibling tests in that file pass, because they call
  `nest_spawned_loot_in_containers()` with an explicit `1/1` chance instead of
  rolling. The fix is to stop depending on the roll, not to hunt for a seed that
  works in two standard libraries.

**The fork has no CI.** GitHub Actions has never run on
`GoatBoy-11/Cataclysm-SE` — `list_workflow_runs` returns zero for every workflow.
Nothing but a local run has ever checked CSE, on any platform, which is why a
non-portable test could sit in `main` unnoticed. A web session is currently the
only thing that builds CSE with a compiler that is not MSVC.

### What a web session can and cannot do

Can: read and change C++, JSON and Lua; build; run the suite and read real
failures; merge upstream BN; write and review plans and handoffs; commit and push.

Cannot: **playtest**. No tiles build, no display, no repo-root exe, no
`rotate-game-exe.sh`, and the container is discarded when the session ends. Given
how much of this project's real defect-finding has come from the owner's playtests
and not from the suite, treat anything a web session ships as needing a playtest on
the Windows machine before it is believed.

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
