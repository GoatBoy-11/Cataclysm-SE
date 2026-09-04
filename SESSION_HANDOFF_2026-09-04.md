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

---

# Later the same day — mouse work continued

## Shipped (uncommitted, working tree only)

- **Mouse on every Yes/No/Ignore prompt.** One change in `query_popup`: hover
  highlights, left click confirms, right click cancels where `allow_cancel` is set.
  Clicking routes through the existing `CONFIRM`/`QUIT` branches by rewriting
  `res.action`, so the decision chain below is untouched.
- **ESC no longer lost while the mouse moves.** Root cause was upstream, not the
  mouse port: `CheckMessages()` drains the whole SDL queue into a single
  `last_input`, so a motion event arriving after a keypress destroyed it. Motion is
  now written only when the slot is still empty. `game.cpp` was byte-identical to
  CBN throughout — nothing there was ever wrong.
- **Click modifiers.** `input_event` gained `mouse_ctrl` / `mouse_shift`, filled
  from `SDL_GetModState()` beside `mouse_pos`. Deliberately **not** in
  `operator==`: putting them in `modifiers` would make every click match no
  binding while ctrl or shift was held, killing clicking game-wide.
- **Pickup and trade got mouse support from scratch** (neither had any). Plain left
  click marks/toggles the whole stack, ctrl+click ±1, shift+click ±5, both clamped
  to the stack. Right click is the "decrease" side. Wheel scrolls the list.
- **Four menus**: options (rows, page tabs, wheel), colour manager (row + column,
  wheel), distraction manager (click toggles, wheel), keybindings (wheel, and a
  click stands in for the row's hotkey while adding/removing).

## Things worth knowing

- **The wheel never changed quantity anywhere.** The request was to replace that
  behaviour; it did not exist. In `PICKUP`, `SCROLL_UP`/`SCROLL_DOWN` were bound to
  PPAGE/NPAGE only and scrolled the *item info pane*. They now do that on a key and
  scroll the list on the wheel, distinguished by `get_raw_input().type`.
- **Two row mappings are recorded during drawing, not recomputed**: trade
  (`them_row_entries` / `you_row_entries`) and options (`row_items`). Both lists
  interleave non-item rows — category headings in trade, group headers and
  collapsed groups in options — so `offset + row` is wrong and would silently act
  on a different item than the one clicked.
- `options.cpp`'s `on_select_option` captures `curr_item` **by reference**, so a
  click must set `iCurrentLine` before `curr_item` is bound. The click pre-pass sits
  above it for that reason.

## Fixed in a file I do not own

`src/image_viewer.cpp` (untracked, another agent's in-flight work) did not compile:
`std::max( 1.0, std::lround( … ) )` has no overload, since `lround` returns `long`.
Changed the literals to `1L` on both lines so the tree would build at all. Nothing
else in that file was touched.

Its test `resolve_image_path_in_roots_prefers_earlier_root`
(`tests/image_viewer_test.cpp:97`) fails on `REQUIRE( assure_dir_exist( first ) )`.
That failure is theirs, unrelated to the mouse work, and was left alone.

## Not done

- Not committed, at the user's request.
- `astyle` is not installed on this machine, so none of the new code has been run
  through the project formatter. It was written to match surrounding style by hand.
- The five review findings from the earlier `/code-review high` pass are still open,
  including the only memory-safety one (`inventory_ui.cpp:2178`, `rect_entry_map`
  caching raw `inventory_entry*` across reallocation).

---

## Resolution — the work is committed and pushed

`feat/mouse-ui-menus` = `0bf017adde`, pushed to `origin`. 15 files, 705 insertions.
Built from `cd82c8fe02`, which is the tree the work was developed and tested against.

It was committed with plumbing (`write-tree` / `commit-tree` / `branch`) rather than a
checkout, deliberately: the shared working tree belongs to another agent mid-task, and
switching branches is what destroyed this work three times already. Nothing was checked
out, stashed, or switched.

**Why a new branch and not `feat/native-mouse-ui`:** that branch is at `ad50fcc181`,
whose only extra content over `cd82c8fe02` is `SESSION_HANDOFF_2026-09-04.md`. The work
was written and tested against `cd82c8fe02`, so that is its honest parent. Merging or
rebasing onto `feat/native-mouse-ui` is a one-liner if wanted.

### State of the shared tree

Left as found, plus this work unstaged. Nothing is staged, so the other agent's next
`git commit` cannot sweep these files in — but `git commit -a` still would. That is
harmless now: the canonical copy is the pushed branch.

### The collision, for next time

The other agent stashed the tree, switched it to `feat/show-image`, and later dropped
the stash holding this work. Two builds completed against a tree that had been reverted
mid-build, producing binaries that silently lacked the changes. The lesson is narrower
than "commit early": **verify the source is still present immediately after a build**,
not just that the build exited 0. `grep` for a marker from your own change.

The durable fix is one worktree per agent, which `CLAUDE.md` already recommends and
which was not in force here.

### Verification of the delivered build

Playtest exe is the 19:18 build, byte-identical to its build output, and confirmed to
contain this work by running `[ui_mouse]` from the shipped test binary (5 cases,
36 assertions) rather than trusting the source tree.

Full suite: 1,112 cases, 1,108 passed, 4 failed — the four documented environmental
CPU-backend vision failures, nothing else.
