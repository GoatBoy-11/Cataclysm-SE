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
cherry-picked into CSE with real three-way merges.

## Building on this machine

**Ignore the build commands in `AGENTS.md`** — they target Linux (`--preset linux-full`)
and will not work here. This is Windows with Visual Studio 18.

```sh
cmake --preset cse-msvc
cmake --build out/build/cse-vcpkg --config RelWithDebInfo --parallel 6
out/build/cse-vcpkg/tests/RelWithDebInfo/cata_test-tiles.exe "[optional-filter]"
```

Three **gitignored** files (excluded via `.git/info/exclude`) make this work. If a build
fails, check these before touching any tracked CMake file:

1. `CMakeUserPresets.json` — forces the `Visual Studio 18 2026` generator (the repo pins
   VS 17) and sets `VCPKG_ROOT` to VS's bundled vcpkg. **Without `VCPKG_ROOT`, CMake
   silently builds all of SDL3 from source and then fails on missing submodules, a
   Vulkan-SDK-shadowed DirectXShaderCompiler, and missing NASM.**
2. `cse-local-overrides.cmake` — sets `CMAKE_MSVC_DEBUG_INFORMATION_FORMAT` to
   `ProgramDatabase`. Without it, upstream's `Embedded` (`/Z7`) pushes
   `cataclysm-bn-tiles-common.lib` past the 4 GB archive limit and the link dies
   with `LNK1248`.
3. `.codegpt-game.json` — launch manifest for the game-development skill.

**A Visual Studio update can break the build outright.** On 2026-08-31 Community
updated itself to 18.9 and lost both `Common7/Tools/VsDevCmd.bat` and its bundled
`VC/vcpkg`; CMake then refused to configure at all ("could not find specified
instance of Visual Studio", or "VsDevCmd.bat not found"). The build currently runs
against **BuildTools 18.8** instead, via three things:

- `CMakeUserPresets.json` pins `CMAKE_GENERATOR_INSTANCE` to the BuildTools path
  *with* a `,version=` field, and points `VCPKG_ROOT` at its `VC/vcpkg`.
- `DevEnvDir` must be exported to `<BuildTools>/Common7/IDE`, or
  `build-scripts/VsDevCmd.cmake` asks `vswhere -latest` and gets the broken
  Community install back.
- `-DVCPKG_MANIFEST_INSTALL=OFF` at configure time: BuildTools' vcpkg has no
  `bootstrap-vcpkg.bat`, so the manifest install fails. This only works because
  `out/build/cse-vcpkg/vcpkg_installed` is already populated — a clean build needs
  a working vcpkg first.

The real fix is repairing Visual Studio (installer → Repair, or re-add "Desktop
development with C++"). Once repaired, revert these overrides.

**Check timestamps, not just exit codes.** A build can report success while
producing nothing — a stale exe then passes tests that never saw the change:
`ls -la out/build/cse-vcpkg/tests/RelWithDebInfo/cata_test-tiles.exe`.

Adding a file to `tests/` requires re-running `cmake --preset cse-msvc`: the `tests/`
glob has no `CONFIGURE_DEPENDS`, unlike `src/`.

**This machine has no working SDL_GPU device.** The test binary defaults to the
`gpu_software` compute backend and aborts before the first test with *"SDL_GPU: device
creation failed"*. Run the suite with the CPU backend instead:

```sh
export CATA_TEST_COMPUTE_ACCELERATION=cpu
out/build/cse-vcpkg/tests/RelWithDebInfo/cata_test-tiles.exe "[optional-filter]"
```

That path is upstream BN's fallback (`7478f040a5`, `b43ea3daff`), and its lighting does
not match the GPU path exactly: four vision tests fail under it — `vision_wall_obstructs_light`,
`vision_single_tile_skylight`, `vision_see_out_of_vehicle`, `vision_see_into_vehicle`, all
at `tests/vision_test.cpp:256`. Treat those four as environmental. **Any other failure is real.**

Never pipe `cmake --build` through `tail` or `head`: the shell reports the pager's exit
status, so a failed build looks like a clean one.

## Fork discipline

Every C++ edit is a future merge conflict with upstream BN. Cheapest to most expensive:

1. JSON and Lua content — near-zero merge cost
2. New files — conflict only if upstream adds the same path
3. Additive hooks in existing files
4. Edits inside existing function bodies — most expensive, use sparingly

When a change can live behind an existing seam rather than spread across call sites,
put it behind the seam. That principle is why the pocket port touches `item_contents`
instead of its 181 callers.

Machine-local config belongs in `.git/info/exclude`, never in the tracked `.gitignore`.

## Current state

Branding is done: menu, window titles, memorial header, README, and userdata paths
(`cataclysm-cse`, separate from BN's). Build targets, translations and code comments
deliberately keep upstream names to hold merge cost down.

`origin` still points at `https://github.com/cataclysmbn/Cataclysm-BN.git`. When CSE
gets its own repo, rename that remote to `upstream` and add the new one as `origin`.
Nothing has been pushed.

### Work in flight — the pocket system port

Porting CDDA's pocket system (multi-compartment containers with per-pocket limits,
priorities and whitelists) into CSE.

- Design: `docs/superpowers/specs/2026-08-29-pocket-system-design.md`
- Plans live in `docs/superpowers/plans/`. In dependency order: `2026-08-29-pocket-core.md`
  (Phase 1), `2026-08-29-pocket-phase-2.md` (JSON), `2026-08-30-pocket-phase-3.md`
  (restrictions and enforcement), `2026-08-30-put-in-refactor.md`,
  `2026-08-30-pocket-classic-mode.md`, `2026-08-30-pocket-favorites.md`.
- CDDA reference is pinned at `5b915aea09`. Do not track their `master`.

**All six plans have landed on `main`** through `9323d082b9`. What is left is verification,
not implementation: full suite green, and a human playtest of priority routing, item rules,
persistence across save/reload, and the classic-mode toggle.

**Do not trust the plans' checkboxes.** Five of the six still read as entirely unticked
despite their work being committed; only `2026-08-30-pocket-favorites.md` was kept current.
Read `git log` for what actually shipped.

Two constraints from the design carry into all pocket work:

- **All 181 existing `.contents.` call sites must compile unchanged.** If one breaks,
  fix `item_contents`, never the call site.
- **Migration never destroys an item.**

**The test avatar carries `DEBUG_STORAGE`**, so its carrying capacity is
effectively infinite and no test using `g->u` can exercise any
over-capacity path. Use `standard_npc` for those. This is why the suite missed
a worldgen crash that a few seconds of real play found.

## Conventions

Follow `AGENTS.md` for C++ style, formatting, JSON linting and i18n. Two exceptions:

- **Build commands** — use the Windows ones above.
- **Commit messages** — Oliver wants a short conventional-commit title plus short
  bullet points, always, without being asked. This overrides the "MUST NOT add body"
  rule in `AGENTS.md`.
