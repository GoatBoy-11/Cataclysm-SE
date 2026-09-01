# Mouse Enable Toggle & Side-Button Port (Layer 1)

**Goal:** Bring CDDA's runtime mouse toggle and X1/X2 side-button support into CSE
without adopting ImGui/IMTUI or CDDA's split input architecture. This is the
portable "Layer 1" work identified in the mouse-system study; the ImGui-bound
mouse UX is explicitly out of scope.

**Blocked by:** nothing. Independent of the pocket-system work.

**Spec:** none (investigation, not a design doc). Reference material below.

## Reference commits (CDDA, pinned @ `5b915aea09`)

- `e3fb5bbdf2` "Add option to disable mouse (#80093)" — the `ENABLE_MOUSE` option,
  the cached `enabled`/`hidekb` flags, and the SDL event gate. Source of truth for
  Tasks 1–2. Its options-API and `cata_imgui.cpp`/`uilist.cpp` hunks do **not**
  apply to CSE and are dropped.
- `8cf6d18cb2` "split input.*" — introduces the scoped `MouseInput` enum (press/release,
  X1/X2, Move) as part of a 1,871-line input refactor. **Do not cherry-pick.** Only the
  X1/X2 enum subset is ported (Task 3), hand-added to CSE's existing unscoped enum.

None of these cherry-pick cleanly: CSE keeps BN's flat cached-options style (no
`namespace cata::options` block) and BN's older `options_manager::add(...)` API.

## Why this matters now

CSE already has working mouse input (`handle_mouseview`, edge scrolling,
click-to-move, right-click fire/close), but it lacks a runtime off-switch and side
buttons. `ENABLE_MOUSE` is the low-merge-cost backbone that later work can hang off;
X1/X2 parity is a small, additive quality win. Both are new option + new enum values,
so they conflict with upstream only if upstream adds the same names.

## Global Constraints

- **No ImGui, no IMTUI, no `input_context.*` split.** Any hunk that touches those is
  out of scope for this plan.
- **Keep BN's flat option-cache style.** Use two globals (`mouse_enabled`,
  `mouse_hide_kb`), not the CDDA `cata::options::mouse` struct.
- **Clicks keep firing on button-up.** CSE has no press/release consumers; adding the
  full pressed/released distinction would change click timing for no benefit. Only the
  X1/X2 button codes are added.
- **Additive only.** New option, new enum values, new `if` guards — no rewrites of
  existing function bodies beyond the gate.
- Configure: `cmake --preset cse-msvc`
- Build: `cmake --build out/build/cse-vcpkg --config RelWithDebInfo --parallel 6`
- Tests: `CATA_TEST_COMPUTE_ACCELERATION=cpu`, then
  `out/build/cse-vcpkg/tests/RelWithDebInfo/cata_test-tiles.exe`
- `tests/` glob has no `CONFIGURE_DEPENDS`; re-run the preset if a test file is added.
- Judge the suite by pass/fail and case count, never assertion count.

---

### Task 1: The `ENABLE_MOUSE` option + cached flags

**Files:** `src/options.cpp`, `src/cached_options.h`, `src/cached_options.cpp`

- [x] In `add_options_interface()` (before the existing `ENABLE_JOYSTICK` add,
      ~`src/options.cpp:1937`), register the bool option in BN's old API:
      `add( "ENABLE_MOUSE", interface, translate_marker( "Enable mouse" ),
            translate_marker( "Enable input from mouse." ), true, COPT_NO_HIDE );`
- [x] Grey out the two dependent options after they are declared:
      `get_option( "HIDE_CURSOR" ).setPrerequisite( "ENABLE_MOUSE" );` and
      `get_option( "EDGE_SCROLL" ).setPrerequisite( "ENABLE_MOUSE" );`
      (`setPrerequisite` already exists at `options.h:123`; mirror `AUTO_PICKUP`
      usages at `options.cpp:1311` and nearby.)
- [x] Add flat globals to `cached_options.h` (matching the file's existing flat list):
      `extern bool mouse_enabled;` / `extern bool mouse_hide_kb;`
- [x] Define them in `cached_options.cpp`:
      `bool mouse_enabled = true;` / `bool mouse_hide_kb = false;`
- [x] Populate them in `cache_to_globals()` (~`options.cpp:4438`):
      `mouse_enabled = ::get_option<bool>( "ENABLE_MOUSE" );` and
      `mouse_hide_kb = ::get_option<std::string>( "HIDE_CURSOR" ) == "hidekb";`

### Task 2: Gate SDL mouse events on the cached flag

**Files:** `src/sdltiles.cpp`

- [x] In the three existing mouse cases, add `if( !mouse_enabled ) { break; }` as the
      first statement:
      - `case SDL_EVENT_MOUSE_MOTION:` (~line 3398)
      - `case SDL_EVENT_MOUSE_BUTTON_UP:` (~line 3410)
      - `case SDL_EVENT_MOUSE_WHEEL:` (~line 3421)
- [x] Add `#include "cached_options.h"` (it was not present in `sdltiles.cpp`).
- [x] Do **not** touch `is_mouse_enabled()` — it reports platform capability
      (Win32 curses has no mouse) and is a different concern from the user toggle.

### Task 3: X1/X2 side buttons

**Files:** `src/input.h`, `src/input.cpp`, `src/sdltiles.cpp`

- [x] Extend the unscoped `enum mouse_buttons` (~`input.h:94`) with
      `MOUSE_BUTTON_X1` and `MOUSE_BUTTON_X2` after `MOUSE_MOVE`.
- [x] In `input.cpp`, add `keyname_to_keycode["MOUSE_X1"] = MOUSE_BUTTON_X1;` and the
      `MOUSE_X2` twin, plus the reverse key-name/description pairs, mirroring the
      existing `MOUSE_LEFT`/`MOUSE_RIGHT` handling (~lines 436–493).
- [x] In `sdltiles.cpp`, in the `SDL_EVENT_MOUSE_BUTTON_UP` switch, add
      `case SDL_BUTTON_X1:` and `case SDL_BUTTON_X2:` mapping to the new enum values.
- [x] Leave `ncurses_def.cpp` untouched — curses exposes no X1/X2 event.

### Task 4: Verification

- [x] Build passes with `cataclysm-bn-tiles` and `cata_test-tiles` (Windows commands above).
      Both binaries relinked from the changed TUs; zero compile/link errors.
- [ ] Game launches; `ENABLE_MOUSE` appears in Options → Interface and, when disabled,
      both `HIDE_CURSOR` and `EDGE_SCROLL` grey out. *(Not verified: see data-load note.)*
- [ ] Manual check: with `ENABLE_MOUSE` off, mouse motion/clicks/wheel produce no
      in-game response; with it on, current behaviour is unchanged. *(Not verified.)*
- [ ] Manual check: X1/X2 buttons produce the new actions where bindable, and do not
      crash when unbound. *(Not verified.)*
- [x] Targeted test run — `[pocket]` passes (510 assertions / 159 cases, "All tests
      passed"). Note: a pre-existing WIP JSON typo (`"longest_side": "1 meter"`,
      an unsupported unit) blocked all data load; fixed those two values to `"1 m"`
      in `data/json/items/generic.json` to unblock verification.

## Out of scope

- ImGui/IMTUI adoption and the `input_context.*`/`input_enums.h` split.
- The `MouseInput` press/release distinction (no CSE consumer).
- `refresh_mouse_config()` / `want_capture_mouse()` — ImGui-bound; CSE's cursor
  visibility already works without them.
- Overmap-editor mouse nav (`0aa3dfb2a7`) and tile map-editor mouse (`15eb8b5208`) —
  both optional, decision deferred.
- Any change to how clicks are timed (up vs down).
