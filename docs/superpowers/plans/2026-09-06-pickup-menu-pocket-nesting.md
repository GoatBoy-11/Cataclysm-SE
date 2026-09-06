# Pickup menu: show and select what is inside pockets

Status: **not started**. Scoped 2026-09-06 against commit `01cde8c5fc`.
All design questions are **answered** - see section 3. Nothing here needs the
owner's input before you start.

Line numbers are from `01cde8c5fc` and will drift. Function names will not.
Search by name, not by line.

---

## 0. STOP. Read this section before opening any file.

Three facts decide whether this work goes well. Every one of them is something
you would get wrong by reading `pickup.cpp` from the top and starting to type.

### FACT 1 - The pickup menu ALREADY has parent/child rows. Do not build a second one.

`pickup_count` (`src/pickup.cpp:82`) already carries:

```cpp
std::optional<size_t> parent;
std::vector<size_t> children;
bool all_children_picked = false;
```

- The renderer **already** draws a child marker coloured by the parent's state
  (`src/pickup.cpp:889`).
- Selection **already** cascades from a parent to its children
  (`src/pickup.cpp:1027-1037`).
- `pick_drop_selection::children` **already** carries them into `pick_one_up()`.

What it cannot do is see a pocket. `calculate_parents()` (`src/pickup.cpp:569`)
derives every parent link from `item_drop_token` - "these items hit the ground
together out of the same container" - and nothing else.

> **Your job is to give that existing machine a second source of children.**
> If you find yourself writing a new tree structure, a new entry type, or a
> second parent array, stop and re-read this section. Two nesting systems that
> disagree with each other is a far worse outcome than shipping nothing.

### FACT 2 - `EMPTY` is a useful hook, but it does NOT already do what Step 3 needs.

`src/pickup.cpp:215` has an answer `EMPTY` labelled *"Pick up just %s, without
contents"*, hotkey `e`, and `src/pickup.cpp:476` guards the child loop with
`if( option != EMPTY )`.

That is genuinely most of the plumbing. **But read carefully what it does:** it
skips *separately picking up* the children. Today's children are drop-token
siblings that are already lying on the tile, so skipping them leaves them on the
tile, which is the desired result.

**Your new pocket children are different. They are physically inside the
container.** If you pick the container up and merely skip its children, the
contents travel with it, still inside. That is the opposite of what is wanted.

> For pocket children you must **actively move the contents out onto the tile
> first**, then pick up the container. Reusing `EMPTY` gets you the answer
> plumbing and the hotkey. It does not get you the behaviour.

### FACT 3 - Verification here is a gate, not a principle.

This project has repeatedly shipped tests that passed for the wrong reason. On
2026-09-06 a crafting test passed four runs in five: the recipe under test could
fail, and "nothing on the floor" looked identical to the bug being present.

Section 6 is a hard gate with exact commands. Do not report this work as done
until you have run it and pasted the output.

---

## 1. What the owner asked for

Verbatim, from the playtest report of 2026-09-06. This is the spec.

> In the pickup menu, if an item has things in it's pockest, I cannot see them -
> only the main "container" item. example: "=pants (2 items)". Maybe the player
> should be able to collapse and uncollapse the entry (like how you did for the
> main inventory)? The player could then highlight the items in pockets and
> decide to pick them up individually, if the select and pick up the "container"
> then they would pick up that PLUS the items. A method to pickup just the
> container and now [not] pocketed items could also be integrated, for example
> holding the ALT key when highlighting and picking up the container (so ALT +
> ENTER or ALT + Right Mouse Click - with the SHIFT or CTRL additions if they
> want 1 instance or 5 of the item, if it's in a stack) - if they do so the
> pocketed items are removed from the container before the player picks them up
> and stay in the pile.

| # | Requirement |
|---|---|
| R1 | Items inside a pocket appear as their own rows in the pickup list |
| R2 | Those rows collapse and uncollapse, like the main inventory does |
| R3 | A pocketed item can be selected and picked up on its own |
| R4 | Selecting the container picks up the container **and** its contents |
| R5 | A key picks up the container **alone**; its contents are emptied onto the tile and stay there |
| R6 | Same feature in the trade menu, **after** the pickup menu has landed |

---

## 2. Why nothing shows today

`pick_up_from_items()` (`src/pickup.cpp:702`) builds the list from a flat
`std::vector<item_stack::iterator>` of what is **on the tile**. Items in a pocket
are not in that vector, so they were never candidates for a row.

The row label (`src/pickup.cpp:933`) is
`this_item.display_name( stacked_here[true_it].size() )`, with `this_item.symbol()`
prepended when the `ITEM_SYMBOLS` option is on. That is where `=pants (2 items)`
comes from. **The menu is already displaying the contents count and then refusing
to expand it.**

---

## 3. Decisions - already made, do not re-ask

The owner answered all of these on 2026-09-06. Implement them as written.

| Question | Decision |
|---|---|
| Collapsed or expanded by default | **Situational.** Expanded when the row would add **1 or 2** descendant rows; collapsed at **3 or more**. |
| How to count for that threshold | **Recursive descendant rows**, not top-level item count. A backpack holding 3 pouches of 4 things each is 15, not 3. |
| Collapse key | **`c`** - the same key the inventory uses. |
| Container-alone key | **`e`** - the same letter the existing "without contents" answer already uses. |
| ALT chords | **Do not hardcode.** Register named actions; the owner can rebind to ALT+ENTER in-game if their input layer supports it. |
| Nesting depth | **Uncapped.** As deep as the containers allow, matching the inventory. |
| Indent depth | **Capped at 3 levels.** The pickup window is narrower than the inventory; nesting keeps going, indentation stops growing. |
| Trade menu | **Yes, but second.** Land pickup first. See section 7. |

### Keybinding facts you will need

The `PICKUP` context currently consumes: `,` `+` `=` `-` `h` `j` `k` `l` `W` `w`,
plus PPAGE/NPAGE, arrows, numpad and joystick. **`c` and `e` are both free.**

Registering a letter as an action removes it from the item-hotkey pool
(`ctxt.get_available_single_char_hotkeys( all_pickup_chars )`). `h/j/k/l/w/W`
already do this. Two more is accepted and expected.

Add both to `data/raw/keybindings/keybindings.json` with `"category": "PICKUP"`:

- reuse the existing action id **`SHOW_HIDE_CONTENTS`** for collapse, so the
  in-game keybindings menu shows it under the same name as the inventory's. Its
  INVENTORY entry is already `{"key": "c"}`; add a PICKUP entry alongside.
- add a new action id for container-alone. **`PICKUP_WITHOUT_CONTENTS`** is the
  suggested name, key `e`.

### About SHIFT and CTRL

The owner's spec mentions SHIFT/CTRL for taking 1 or 5 of a stack. Confirmed at
`src/pickup.cpp:1111-1118`, **with two details the spec does not state**:

```cpp
if( evt.mouse_ctrl || evt.mouse_shift ) {
    const int step = evt.mouse_shift ? 5 : 1;
```

- **SHIFT is 5, CTRL is 1.**
- **They are mouse-only.** There is no keyboard equivalent. On the keyboard,
  `+`/`=` and `-` (`INCREASE_COUNT` / `DECREASE_COUNT`) step by 1 and nothing
  steps by 5.

**Do not add keyboard SHIFT/CTRL handling as part of this work.** It is a
separate change and was not asked for. Just do not break the mouse path.

---

## 4. The implementation to copy

`src/inventory_ui.cpp` did exactly this for the main inventory. Read it before
writing anything: `git log --oneline -- src/inventory_ui.cpp`.

| Piece | What it does |
|---|---|
| `add_contained_items()` | recursive descent into pockets, restacking as it goes, one category copy per stack |
| `inventory_entry::topmost_parent` | the visible ancestor a nested row belongs to |
| `inventory_entry::indent` | depth; drives display and the tree-node test |
| `is_tree_node()` | `entry.indent > 0 \|\| entry.topmost_parent == nullptr` |
| `entries` / `entries_hidden` | collapsed children move between these two |
| `under_collapsed_parent` | walks `parent_item()` up to `topmost_parent` |

**Two bugs were found there by playtest, not by the suite. Do not repeat them:**

- `move_entries_to()` must move `entries_hidden` too, or collapsed rows vanish
  permanently when columns merge on a narrow screen.
- `clear()` must empty `entries_hidden`, or a reopened menu shows stale rows.

---

## 5. Tasks

Land each step as its own commit. Build and run the gate in section 6 between
them. Do not batch them.

> Checkbox warning: several older plans in this directory read as unticked
> despite the work having shipped. `git log` is the record, not these boxes.

### Step 1 - contents become rows (R1, R3, R4)

- [ ] In `pick_up_from_items()`, extend list construction so each tile item
      contributes its pocket contents as further entries, recursively, uncapped.
- [ ] Add a pass after `calculate_parents()` that sets `parent` for a contents
      row to its containing row's index. **Do not modify `calculate_parents()`
      itself** - leave the drop-token logic alone and layer on top of it.
- [ ] Decide, in a written comment next to the code, which wins when an item has
      both a drop-token parent and a pocket parent. Pocket containment is the
      live truth and should win; say so and say why.
- [ ] Verify R4 needs no work: parent selection already cascades into `children`.

**Trap:** `pick_one_up()` detaches children *after* the parent
(`src/pickup.cpp:477`) because removing the parent first re-indexes the stack.
Picking a pocketed item *without* its container is the mirror case. Confirm the
iterators survive it and write a test for that ordering specifically.

**Done when:** contents are visible and individually selectable, always expanded,
and picking up a container still brings its contents.

### Step 2 - collapse and expand (R2)

- [ ] Add a collapsed flag per row.
- [ ] Hide the children of a collapsed row from `matches`.
- [ ] Register `SHOW_HIDE_CONTENTS` in the `PICKUP` context and add the
      keybindings entry with key `c`.
- [ ] Set the initial state per row: expanded at 1-2 recursive descendants,
      collapsed at 3 or more.
- [ ] Cap the visual indent at 3 levels while letting nesting go deeper.

**Done when:** `c` toggles the highlighted container, a pouch with two things in
it opens by default, and a full backpack does not.

### Step 3 - container without contents (R5)

- [ ] Register `PICKUP_WITHOUT_CONTENTS` in the `PICKUP` context, key `e`, and
      add the keybindings entry.
- [ ] On that action against a container row: **move its pocket contents onto the
      tile the container is on**, then pick up the container.
- [ ] Re-read FACT 2. Skipping the child loop is not sufficient; the contents
      must physically leave the container.
- [ ] Contents land on **the container's tile**, not the player's tile. The
      helpers in `src/pocket_overflow.cpp` drop at the *character's* position, so
      either pass a position through or call `drop_on_map()` directly.

**Done when:** `e` on a full backpack leaves a pile of its contents on the tile
and puts the empty backpack in your hands.

---

## 6. Verification gate - run this, paste the output

### Build

```
cmd /c "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" && cmake --build out/build/cse-ninja
```

Check the exit code directly. **Never pipe a build through `tail` or `head`** -
the shell reports the pager's status and a failed build looks clean. This has
bitten twice.

### Test

```
export CATA_TEST_COMPUTE_ACCELERATION=cpu
out/build/cse-ninja/tests/cata_test-tiles.exe "[pickup],[pocket],[routing]"
out/build/cse-ninja/tests/cata_test-tiles.exe          # full suite, ~10 min
```

Read the **case counts**, never the exit code - Catch2's exit code is a
failed-assertion count. A clean full run is roughly **1,173 cases, 4 failed**.
Those four are `tests/vision_test.cpp:256`, they fail for environmental reasons
on this machine, and **anything else is a real failure you caused**.

### The gate itself - all four required

1. [ ] Full suite run, case counts pasted, only the four vision failures.
2. [ ] **For every test you added: break the code it covers, rebuild, and show
       the test failing.** A test never seen red is not evidence.
3. [ ] **Run each new test at least 5 times.** A test that passes 4 times in 5 is
       worse than no test. This exact check caught a flaky test on 2026-09-06.
4. [ ] Playtest matrix below walked by hand, or explicitly handed to the owner
       with a note saying which rows you did not cover.

### Playtest matrix

- container on the ground with items in pockets, collapsed and expanded
- picking up a single pocketed item, leaving the container behind
- picking up the container, contents coming with it
- `e` on the container, contents staying on the tile
- a container inside a container inside a container
- a stack of identical containers, each with contents
- autopickup over the same tile: must not prompt, must not change behaviour
- a character wearing nothing with a pocket: unaffected
- classic mode (`POCKET_*` world options off): unaffected

The last two are not optional. Classic mode is a standing promise that BN
content keeps working.

### Hand over a playable build

```
cmake --build out/build/cse-vcpkg --config RelWithDebInfo --parallel 6
bash .claude/rotate-game-exe.sh
```

Adding a file under `tests/` needs `cmake --preset cse-msvc` first - that glob
has no `CONFIGURE_DEPENDS`. Adding a file under `src/` does not.

---

## 7. Phase 2 - the trade menu (R6)

**Do not start this until phase 1 is committed and playtested.** The two share a
"descend into pockets and build rows" helper. If that helper is wrong, the bug
appears in two unrelated UIs at once and the owner's playtest cannot tell you
which signal is real.

`src/trade_win.cpp` is a different list model from `pickup.cpp`. Expect to reuse
the descent helper and nothing else.

Note for context: the trade window already needed teaching about pockets once, on
2026-09-05. Shopkeepers who had stowed their goods properly had nothing to sell,
because `init_selling()` read only the flat inventory. Assume other places in that
file make the same assumption.

---

## 8. Rules of this repo

- **`F:\Projects\CBN` and `F:\Projects\CDDA` are read-only.** Read them freely to
  compare; never write to them. If a task seems to need editing them, you have
  misread the task. CDDA solved this with `item_location` and is worth reading -
  but CSE cannot copy it, because CSE still has the flat inventory that classic
  mode is defined in terms of.
- **Do not force-push. Do not push anything from outside `F:\Projects\CSE`.**
- **Commit only when asked.** Short conventional-commit title plus short bullets.
- Every C++ edit is a future merge conflict with upstream BN. Cheapest to most
  expensive: JSON, then new files, then additive hooks, then edits inside
  existing function bodies. `pickup.cpp` needs the expensive kind - keep the diff
  tight and push logic into helpers that can live in their own file.
- Machine-local config goes in `.git/info/exclude`, never the tracked
  `.gitignore`.
- **Neither formatter is installed here.** Match the surrounding style by hand:
  padded parens, 100 columns. **Do not run `build-scripts/format-cpp.sh` over
  `tests/`** - it rewrote 3,116 lines of `item_pocket_test.cpp` that had to be
  reverted.
- The test avatar carries `DEBUG_STORAGE`, so its capacity is effectively
  infinite and no test using `g->u` can exercise an over-capacity path. Use
  `standard_npc` for those. It also has no valid character id, so any test
  touching item ownership must call `setID( character_id( 1 ), true )` first.
- Read the newest `SESSION_HANDOFF_*.md` first. The four addenda dated
  2026-09-06 cover the pocket work this feature sits on.
- Another AI may be working in this tree at the same time. Commit or stash before
  handing over, and write down anything you fixed in files you do not own.
