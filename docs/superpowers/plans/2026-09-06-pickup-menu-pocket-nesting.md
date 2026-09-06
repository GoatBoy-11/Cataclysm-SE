# Pickup menu: show and select what is inside pockets

Status: **not started**. Scoped 2026-09-06 against commit `01cde8c5fc`.
Line numbers are from that commit and will drift; the function names will not.

This is a feature, not a bug fix. It is the largest outstanding item from the
pocket work, and it touches a UI nobody has been in yet.

**Related plans, all landed:** `2026-08-31-pocket-pickup-routing.md` (routing
into worn pockets), `2026-08-31-pocket-nested-inventory.md` (nesting in the
*inventory* screens), `2026-08-31-pocket-enforcement-pickup.md` (the
wield-or-leave rule). This plan is the missing fourth: nesting in the *pickup*
list.

---

## 1. What the owner asked for

Verbatim, from the playtest report of 2026-09-06. Treat this as the spec;
everything below is analysis serving it.

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

Broken into requirements:

| # | Requirement |
|---|---|
| R1 | Items inside a pocket appear as their own rows in the pickup list |
| R2 | Those rows collapse and uncollapse, like the main inventory does |
| R3 | A pocketed item can be selected and picked up on its own |
| R4 | Selecting the container picks up the container **and** its contents |
| R5 | A modifier picks up the container **alone**; its contents are emptied out and stay in the pile |
| R6 | R5's binding: ALT + ENTER, and ALT + right mouse click |
| R7 | SHIFT / CTRL still mean "one" and "five" of a stack alongside the ALT modifier |

R7 needs checking against what SHIFT and CTRL actually do in this menu today -
see the open questions in section 6. Do not assume the owner's description of
the existing bindings is exact; confirm it in the code, and ask if it differs.

---

## 2. Why nothing shows today

`pick_up_from_items()` (`src/pickup.cpp:702`) builds the list from a flat
`std::vector<item_stack::iterator>` of what is **on the tile**. Items sitting in
a pocket are not in that vector at all, so they were never candidates for a row.

The row label at `src/pickup.cpp:933` is:

```cpp
item_name = this_item.display_name( stacked_here[true_it].size() );
```

and with the `ITEM_SYMBOLS` option on, `this_item.symbol()` is prepended. That is
where `=pants (2 items)` comes from: the armour symbol, the name, and the
contents count that `display_name()` already appends.

**The count is real information the menu is already showing and then refusing to
expand.** That is why this reads to a player as a defect rather than as a missing
feature, and it is the reason to build it rather than close it.

---

## 3. The finding that should shape the build

**The pickup menu already has parent/child rows. They are keyed on the wrong
thing.**

`pickup_count` (`src/pickup.cpp:82`) already carries:

```cpp
std::optional<size_t> parent;
std::vector<size_t> children;
bool all_children_picked = false;
```

The renderer already draws a child marker whose colour reflects the parent's
state (`src/pickup.cpp:889`):

```cpp
if( getitem[true_it].parent ) {
    const pickup_count &parent = getitem[*getitem[true_it].parent];
    nc_color color = parent.pick ?
                     ( parent.all_children_picked ? c_light_blue : c_yellow ) :
                     c_dark_gray;
    // TODO: Cute symbol here
    wprintz( w_pickup, color, "\\" );
}
```

Selection already cascades from parent to children (`src/pickup.cpp:1027-1037`),
and `pick_drop_selection::children` already carries them through to
`pick_one_up()`, which knows to detach children after the parent.

But `calculate_parents()` (`src/pickup.cpp:569`) derives all of it from
**`item_drop_token`** - "these items hit the ground together out of the same
container" - and not from live pocket containment:

```cpp
const item_drop_token &this_token = *item_iter->drop_token;
if( this_token.is_child_of( last_parent_token ) ) {
    parents[i] = last_parent_index;
}
```

So the whole parent/child machine exists and works. It simply cannot see a
pocket. **The job is to feed it a second source of children, not to build nesting
from scratch.** Anyone who starts by writing a new tree in this file has misread
it, and will end up with two systems that disagree.

---

## 4. The implementation to copy

`src/inventory_ui.cpp` gained exactly this feature for the main inventory
earlier in the same session. Its pieces are the reference:

| Piece | What it does |
|---|---|
| `add_contained_items()` | recursive descent into pockets, restacking as it goes, one category copy per stack |
| `inventory_entry::topmost_parent` | the visible ancestor a nested row belongs to |
| `inventory_entry::indent` | depth; drives both display and the tree-node test |
| `is_tree_node()` | `entry.indent > 0 \|\| entry.topmost_parent == nullptr` |
| `entries` / `entries_hidden` | collapsed children move between these two |
| `under_collapsed_parent` | walks `parent_item()` up to `topmost_parent` |

Two bugs were found in that work the hard way, both by playtest rather than by
the suite. They will recur here if the same mistakes are made:

- **`move_entries_to()` must move `entries_hidden` too.** Missing that made
  collapsed rows vanish permanently when the columns merged on a narrow screen.
- **`clear()` must empty `entries_hidden`.** Otherwise a reopened menu shows
  stale rows.

Read those commits with `git log --oneline -- src/inventory_ui.cpp` before
writing anything here.

---

## 5. Build order

Each step should build, pass, and be playtestable on its own. Do not do them in
one commit.

### Step 1 - contents become rows (R1, R3)

Extend the list construction in `pick_up_from_items()` so each item on the tile
contributes its pocket contents as further entries, recursively. Then extend
`calculate_parents()` - or add a sibling pass that runs after it - so a contents
row gets its containing row as `parent`.

The existing drop-token parents and the new pocket parents must **not** fight.
Decide explicitly which wins when both apply, and write the reason in a comment
next to the code.

At the end of this step contents are visible and individually selectable, always
expanded. R4 should already work for free, because parent selection already
cascades into `children`.

**Watch:** `pick_one_up()` detaches children *after* the parent
(`src/pickup.cpp:477`) specifically because removing the parent first would
re-index the stack. Picking a pocketed item without its container is the mirror
case. Make sure the iterators survive it, and add a test for that ordering.

### Step 2 - collapse and uncollapse (R2)

Add a collapsed flag per row, hide the children of a collapsed row from
`matches`, and register an action to toggle it. Mirror the inventory's behaviour
so the two menus feel like one game.

The default state is an open question - see section 6.

### Step 3 - container without contents (R5, R6, R7)

Add an answer meaning "take the container, leave what is in it". On selection,
empty the container's pockets onto the tile first, then pick the container up.

The emptying belongs next to the existing put-it-down helpers rather than
inline. `apply_overflow_choice( ..., overflow_choice::drop )` in
`src/pocket_overflow.cpp` is the shared "set it down" path that crafting, trade
and the pickup prompt all use. **But note the difference:** contents left in the
pile should land on the tile the container was on, not on the player's tile. That
may mean a position argument, or calling `drop_on_map()` directly.

ALT as a modifier is the part with real unknowns; see section 6 before designing
around it.

---

## 6. Open questions - ask the owner, do not guess

1. **Collapsed or expanded by default?** A looted tile with five containers
   becomes a wall of text if everything expands. Collapsed-by-default matches the
   inventory; expanded-by-default matches the complaint that they could not be
   seen at all.
2. **Does the ALT chord even arrive in this menu on this machine?**
   `input_context` here registers plain actions (`CONFIRM`, `SELECT`,
   `SEC_SELECT` - `src/pickup.cpp:797-817`). Whether an ALT-modified key is
   distinguishable needs testing **before** the feature is designed around it. If
   it is not, a plain hotkey is the fallback and the owner should pick the
   letter.
3. **What do SHIFT and CTRL do in this menu today?** R7 assumes they already mean
   one and five. Confirm against `INCREASE_COUNT` / `DECREASE_COUNT` and the raw
   input handling around `src/pickup.cpp:1110-1135`. If it differs from the
   owner's description, report that rather than silently reinterpreting the spec.
4. **Should the trade menu get the same treatment?** The owner said "and maybe
   trade". Trade is a different UI (`src/trade_win.cpp`) and deserves its own
   plan. Do not let it expand this one.
5. **How deep should nesting go?** The inventory shows arbitrary depth. The
   pickup list is narrower and may want a cap.

---

## 7. Testing

**The pickup UI is not currently unit-testable, and this is the single biggest
risk to the work.** `pick_up_from_items()` owns its own `input_context` and
window loop, and `pick_one_up_options` is internal to `pickup.cpp` with no way to
inject an answer. That is why the Wear / Wield / Spill / Empty answers - and the
Drop / Leave answers added on 2026-09-06 - have no coverage at all.

**Build the seam first if the budget allows.** The valuable extraction is the
pure part: given the tile's items and a set of selections, produce the
`pick_drop_selection` vector. That is testable without a window, and it is
exactly where every bug in R1 to R5 will actually live. The rendering and the
input loop can stay untested without much loss.

Whatever you do, honour the project rule: **break the code and watch the test
fail before trusting it.** On 2026-09-06 a crafting test passed for the wrong
reason and was only caught because it was run eight times rather than once - the
recipe under test could fail, and "nothing on the floor" looked identical to the
bug it was meant to catch.

Manual playtest matrix, at minimum:

- a container on the ground with items in pockets, collapsed and expanded
- picking up a single pocketed item, leaving the container behind
- picking up the container, expecting the contents to come with it
- ALT-picking the container, expecting the contents to stay on the tile
- a container inside a container
- a stack of identical containers, each with contents
- autopickup over the same tile: it must not prompt, and must not change
- a character wearing nothing with a pocket, and classic mode: both unaffected

---

## 8. Project rules that apply

- `F:\Projects\CBN` and `F:\Projects\CDDA` are **read-only** reference trees.
  CDDA's equivalent is worth reading - it solved this with `item_location` - but
  CSE cannot copy it wholesale, because CSE still has the flat inventory that
  classic mode is defined in terms of.
- Every C++ edit is a future merge conflict with upstream BN. New files are
  cheap; edits inside existing function bodies are the most expensive kind.
  `pickup.cpp` needs the expensive kind, so keep the diff tight and push logic
  into helpers that can live elsewhere.
- Rotate the playtest exe after every build: `bash .claude/rotate-game-exe.sh`.
- Run the suite with `CATA_TEST_COMPUTE_ACCELERATION=cpu`. Four vision tests at
  `tests/vision_test.cpp:256` fail environmentally. Anything else is real.
- Read the newest `SESSION_HANDOFF_*.md` before starting. The three addenda dated
  2026-09-06 cover the pocket work this feature sits on top of.
- **Do not trust the checkboxes in the older plans in this directory.** Several
  read as unticked despite the work having shipped. `git log` is the record.
