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
| `D:\Projects\CSE` | This fork. All work happens here. | **Yes** |
| `D:\Projects\CBN` | Upstream Cataclysm-BN, for reference and diffing | **No — read only** |
| `D:\Projects\CDDA` | Cataclysm-DDA, the source of features being ported | **No — read only** |

CBN and CDDA are reference material. Read them freely to compare implementations;
never write to them. If a task seems to require editing them, you have misread the task.

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

Adding a file to `tests/` requires re-running `cmake --preset cse-msvc`: the `tests/`
glob has no `CONFIGURE_DEPENDS`, unlike `src/`.

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
- Phase 1 plan: `docs/superpowers/plans/2026-08-29-pocket-core.md`
- CDDA reference is pinned at `5b915aea09`. Do not track their `master`.

Phase 1 Tasks 1–4 are committed. **Task 5 (whole-game verification) has not run**, the
plan's checkboxes are unticked, and `tests/item_pocket_test.cpp` has uncommitted
changes. Verify that state before starting anything new.

Two constraints from the design carry into all pocket work:

- **All 181 existing `.contents.` call sites must compile unchanged.** If one breaks,
  fix `item_contents`, never the call site.
- **Migration never destroys an item.**

## Conventions

Follow `AGENTS.md` for C++ style, formatting, JSON linting and i18n. Two exceptions:

- **Build commands** — use the Windows ones above.
- **Commit messages** — Oliver wants a short conventional-commit title plus short
  bullet points, always, without being asked. This overrides the "MUST NOT add body"
  rule in `AGENTS.md`.
