# Construction-Menu Mouse Preview Port

**Goal:** When the player is asked "Construct where?", let the mouse move over
the map and preview the highlighted tile + ghost ter/furniture, and right/left
click to pick the tile — matching CDDA's `*` construction flow.

**Blocked by:** nothing directly, but see "Hard dependency" below — this is a
bigger port than the Layer 1 work, because it needs a CDDA-era `choose_direction`
refactor that CSE never took.

**Spec / reference:** CDDA pinned @ `5b915aea09`, file `src/construction.cpp`
and `src/action.cpp`. The target feature lives in two CDDA functions:
`construction_preview_callback` and the second `place_construction` overload.

**Status:** scoped, NOT implemented. This doc is the plan; get sign-off before
writing code, because the blast radius is wider than the mouse-enable work.

## What was verified (do not rediscover)

CSE **already has**, so no new plumbing is needed for these:

- `input_context::get_coordinates( const catacurses::window & )` —
  `src/input.h:651`, impl `src/input.cpp:1492`. Returns
  `std::optional<tripoint_bub_ms>`; resolves the last mouse coordinate against
  the capture window and `g->ter_view_p`.
- `game::add_draw_callback( shared_ptr_fast<draw_callback_t> )` — used already in
  CSE's `construction.cpp:1630` (the *existing* `draw_valid` highlight callback).
- `game::draw_highlight`, `draw_terrain_override`, `draw_furniture_override` —
  `src/game.h:838-841`.
- `MOUSE_MOVE` / `SELECT` actions and `input_manager` mouse plumbing (now with
  the X1/X2 additions landed in `e1b3ada5b0`).

## What CSE does NOT have (the actual work)

This is the honest headline: **the CDDA construction mouse preview cannot be
pasted into CSE.** The reason is in the direction-picker, not the construction
file.

CDDA's `choose_direction` (and the callback-taking `choose_adjacent` that the
construction code calls) was rewritten to:

1. Accept `allow_mouse` and an `action_cb` callback;
2. Register `COORDINATE`, `MOUSE_MOVE`, and `SELECT` when `allow_mouse` is set;
3. Loop through `ctxt.handle_input()` and consult the callback for
   mouse/click handling.

CDDA `action.h` / `action.cpp`:
```cpp
std::optional<tripoint_rel_ms> choose_direction(
    const std::string &message, bool allow_vertical, bool allow_mouse, int timeout,
    const std::function<std::pair<bool, std::optional<tripoint_rel_ms>>(
        const input_context &, const std::string &)> &action_cb );

std::optional<tripoint_bub_ms> choose_adjacent(
    const std::string &message, bool allow_vertical, bool allow_mouse,
    const std::function<std::pair<bool, std::optional<tripoint_rel_ms>>(
        const input_context &, const std::string &)> &action_cb );
```

CSE's versions are the old, simple, no-callback ones:
```cpp
std::optional<tripoint_rel_ms> choose_direction( const std::string &, bool );
std::optional<tripoint_bub_ms> choose_adjacent( const std::string &, bool );
```

So the port is a **two-part job**: (A) back-port the callback/mouse-capable
direction picker, then (B) wire the construction preview on top of it.

---

## Hard dependency (decide before starting)

`choose_direction` is a shared helper with many callers. Changing it means either:

- **Additive overload (recommended):** keep the existing 2-arg
  `choose_direction`/`choose_adjacent` as thin wrappers and add the full-featured
  overloads with defaulted `allow_mouse = false`, `timeout = -1`, `action_cb =
  nullptr`. This is the fork-discipline-friendly path (existing callers compile
  unchanged, matching the pocket-work rule).
- **Rewrite in place:** higher risk, more churn — avoid.

## Global constraints

- **All existing `choose_direction` / `choose_adjacent` call sites must compile
  unchanged.** Match CDDA's signature with default arguments so old calls behave
  exactly as before.
- **No ImGui.** This stays on the SDL `MOUSE_MOVE`/`SELECT`/`get_coordinates`
  path.
- **Preserve BN behaviour when the mouse is idle.** With `allow_mouse == false`
  the new overload must reduce to today's keyboard-only flow.
- Configure: `cmake --preset cse-msvc`
- Build: `cmake --build out/build/cse-vcpkg --config RelWithDebInfo --parallel 6`
- Tests: `CATA_TEST_COMPUTE_ACCELERATION=cpu`, then
  `out/build/cse-vcpkg/tests/RelWithDebInfo/cata_test-tiles.exe`
- Full suite: expect only the 4 known environmental vision failures; any other
  failure is real.

---

### Task 1: Mouse-capable `choose_direction` (additive)

**Files:** `src/action.h`, `src/action.cpp`

- [x] Add the 5-param `choose_direction` overload (CDDA signature above).
- [x] In it, when `allow_mouse`, register `COORDINATE`, `MOUSE_MOVE`, `SELECT`.
- [x] Call `action_cb( ctxt, action )` each loop; honour its
      `done` / returned `tripoint_rel_ms` exactly as CDDA does.
- [x] Keep the existing `choose_direction` signature as a default-arg overload,

### Task 2: Mouse-capable `choose_adjacent` (additive)

**Files:** `src/action.h`, `src/action.cpp`

- [x] Add the callback-taking `choose_adjacent` overload (pos-anchored).
- [x] Keep `choose_adjacent( message, allow_vertical )` as a wrapper delegating
      to the new overload with `pos = g->u.bub_pos()`.
- [x] The new overload wraps the user callback to also translate `SELECT` via
      `ctxt.get_coordinates( g->w_terrain )`.

### Task 3: Construction preview wiring

**Files:** `src/construction.cpp`

- [x] Port `construction_preview_callback` (adapted: CSE uses separate
      `post_terrain`/`post_furniture` fields, not CDDA's `post_is_furniture`).
- [x] Port the mouse-enabled `place_construction` flow: build `valid`, register
      the preview callback, call `choose_adjacent( g->u.bub_pos(), ... )`.
- [x] The `action_cb` updates `mouse_pos` on `MOUSE_MOVE` via
      `ctxt.get_coordinates( g->w_terrain )`; `SELECT` is handled by the
      `choose_adjacent` wrapper.
- [x] Replaced the old `draw_valid` highlight with the preview callback
      (no duplicate highlight, no regression).
- [~] Blink animation: kept `blink` always-true (no timer). Cosmetic deferral,
      noted so it is not mistaken for a bug.

### Task 4: Verification

- [ ] Build clean (both binaries).
- [ ] Existing `*` construction still works keyboard-only (no mouse), identical
      highlight behaviour.
- [ ] With mouse enabled: hovering previews the tile; left/right click selects;
      Escape cancels ("Never mind.").
- [ ] Full suite green except the 4 known vision failures.

## Out of scope

- ImGui/IMTUI, and the remaining CDDA `COORDINATE`-action refactors beyond what
  Task 1 needs.
- Overmap-editor mouse nav (`0aa3dfb2a7`) and tile map-editor mouse
  (`15eb8b5208`) — separate, deferred.
- Any change to how clicks are timed (still button-up).
