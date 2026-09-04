# Session handoff — 2026-09-04

## Where things stand

`main` is `cd82c8fe02`. The playtest branch `feat/native-mouse-ui` is `06f9a9558d`
(the same work, cherry-picked). PR #3 merged the mouse branch into `main` during the
session, so `main` now carries everything: the mouse feature, its fixes, the
civilian_variety items, the traits, and the tileset graft.

The repo-root playtest exe is the 16:04 build of `06f9a9558d`, verified byte-identical
to its build output. `cataclysm-bn-tiles_old.exe` is the 14:35 build.

## Shipped this session

- **civilian_variety items imported** into base data — 20 items (notes, photograph,
  three work uniforms, seven books, three figures, two body pillows, talking doll),
  13 uncraft recipes, 4 snippet categories, 16 vanilla item-group injections, and two
  Lua iuse handlers in `data/json/lua/iuse/comfort_items.lua`. Monsters and the six
  corpse items were deliberately **not** imported.
- **Tileset graft** — the four `civilian_variety_*` sheets wired into
  `gfx/MSX++UnDeadPeopleEdition/tile_config.json` at absolute range 30648–30921.
- **Three blocking mouse defects fixed** (character-creation softlock, mods dialog
  closing on a row click, crafting detail-pane click starting the wrong recipe).
- **Anime Protagonist**: per-tick nil-global error and the NPC-dialogue crash.
- **Atelophobia removed** entirely (data only).
- **Worldgen tabs** clickable from all three tabs, not just the first.

## Open — the two requested additions

Both confirmed wanted by the user; neither started.

1. **Mouse on Yes/No/Ignore prompts.** Do this one first. Every `query_yn` and the
   "ignore further distractions" prompts route through the single `query_popup` class,
   so one change lights up every prompt in the game. Contained, high payoff.
2. **Mouse wheel to change numbers.** Not one seam — the pickup-quantity prompt, the
   skill picker in character creation, and the options number fields are separate
   widgets with their own loops. The wheel already arrives as `SCROLL_UP`/`SCROLL_DOWN`,
   so each site is small, but each needs its own decision about what "highlighted" means.

## Open — review findings not yet fixed

A `/code-review high` pass over the mouse branch produced 15 findings. Three blocking
ones are fixed; these remain, roughly by severity:

- `inventory_ui.cpp:2178` — `rect_entry_map` caches raw `inventory_entry*` into a
  `std::vector` that `prepare_paging()`/`set_filter()` reallocate, and is rebuilt only
  in `draw_columns`. Any mutation reaching `get_input()` before a redraw dereferences
  freed memory. **Look at this one first — it is the only memory-safety finding.**
- `worldfactory.cpp` `pick_world` (~487) — category tabs switch on plain `MOUSE_MOVE`,
  so sweeping the mouse across the top silently changes category and resets scroll.
  The equivalent bug at ~1266 is now gone, but `pick_world` still has it.
- `ui.cpp:1093` — uilist hover/click sets `fselected`/`selected` directly, bypassing the
  disabled-entry skipping `scrollby()` enforces.
- `crafting_gui.cpp:1499` — tab hit test passes `getmaxx(w_head)` while drawing passes
  `getmaxx(w_head) - lost_width`; clicks map to a neighbouring category once captions
  overflow.
- `ui_mouse.cpp:85` — `tab_rectangles` clips at `max_tab_width + origin.x` while
  `draw_tabs` clips at `max_tab_width`, so rectangles exist for tabs never drawn.
- `main_menu.cpp:719` — submenu rows above `w_open`'s top are unclickable, since input
  is gated on `get_mouse_cell( w_open )`.

Plus four cleanup items (a dead `( void )current_sub;` cast, a redundant `mouse_mode`
guard, a duplicated `calcStartPos`, a point round-trip that cancels itself).

## Traps hit this session — worth not repeating

- **A correct build can still be the wrong build.** Much of a session was spent
  diagnosing "bugs" that were an exe from 2026-09-02 running data from 2026-09-04. The
  binary was built fine; it simply predated the commits under test. When a failure
  implicates recently-landed C++, check the exe timestamp against the newest commit
  touching `src/` *first*. `Invalid hook name: on_craft_failure` and a failing
  `overmap_test` were both this, not real defects.
- **Do not pipe test output through `tail`.** The failure detail is discarded and the
  run has to be repeated. Redirect to a file and grep it.
- **`main` moved twice mid-session** from outside this workspace. Fetch before assuming
  a push will fast-forward.
- The other agent works in `F:\Projects\CSE`; `F:\Projects\CSE-merge` was used as an
  isolated worktree for builds and merges so the playtest tree was never disturbed.

## Notes

- The engine-side `on_craft_failure` hook is still in `catalua_hooks.cpp` and fires in
  `crafting.cpp`, now with no Lua consumer. Left deliberately: it works, costs nothing,
  and removing it is C++ churn plus a rebuild.
- A character who already took Atelophobia will load with an unknown mutation.
- Untracked scratch in the repo root (`build_log.txt`, `cse.txt`, `diff*.txt`,
  `test_*.log`, `.fix_inputcells_diag.ps1`, `gfx/ChibiUltica/`, the `.psd` files) was
  left alone at the user's request. `gfx/ChibiUltica/` looks like real content, not junk.
- Pre-existing JSON errors, unrelated and harmless-looking:
  `nested_lab_central_core.json:483` (`rotation`) and `mutations.json` (`fake_items`).
