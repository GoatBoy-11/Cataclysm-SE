# Session handoff — 2026-09-05

## Where things stand

`main` carries five pocket-system fixes from this session. The repo-root playtest exe
is the 00:37 build; `cataclysm-bn-tiles_old.exe` is the 00:20 build, which has the
first two fixes only.

The session started as "can a container with items in it go into a pocket?" (yes,
nothing forbids it) and turned into an audit of what the pocket port left half-done.
Everything below was found by reading the code or by the owner's playtests, not by
the suite — the suite passed clean through every one of them.

## Shipped this session

- **`accepts_item()` ignored pocketed containers.** It gated on
  `item::is_container()`, which reads only the legacy `container` itype slot, so a
  wallet — a `GENERIC` itype whose storage is pockets — was judged by itself rather
  than its contents. A blacklisted item rode into a pocket inside a wallet while the
  same item in a plastic bag was refused. Now walks CONTAINER pockets.
  Magazine/gunmod pockets stay unread, so a loaded magazine is still judged as a
  magazine.
- **The pocket manager could not reach nested containers.**
  `Character::pocket_destinations()` walked `worn` only, so a wallet carried *in* a
  pocket was never offered as a destination — the owner hit this directly. Now
  recursive, stopping at the excluded item so its whole subtree is pruned.
- **A carried container's contents were listed nowhere.**
  `inventory_selector::add_character_items()` enumerated pocket contents for worn
  garments but not for items in the flat inventory. A garment taken off with full
  pockets drew a row saying how much was inside with nothing beneath it to open: the
  items were held and completely unreachable. This is the bug behind the owner's
  "the pants showed 2 items inside but I couldn't collapse it" report.
- **Crafting could not see containers in pockets.**
  `get_eligible_containers_for_crafting()` scanned wielded items, worn garments and
  the flat inventory, all top-level. Since routing puts jars and bottles inside worn
  pockets, crafting announced there was nothing to store a liquid in while the
  backpack was full of jars. Now recurses into CONTAINER pockets at any depth.
- **`debugmsg` when emptying a container held in a pocket.**
  `consumption.cpp` did `inv.const_stack( inv.position_by_item( &target ) )`; for a
  container that is not in the flat inventory that position is `INT_MIN` and
  `const_stack()` debugmsgs on it. Guarded.

Ten tests were added across `tests/item_pocket_test.cpp` and
`tests/inventory_ui_test.cpp`. Every one was watched failing against the unfixed
code first.

## Tried and reverted: routing takeoff into worn pockets

`Character::takeoff()` sends the garment to the flat inventory with
`inv.add_item()` and never offers worn pockets, unlike every other acquisition
path. The obvious fix — route through `i_add_to_worn_pockets()` first — was built,
tested and **reverted**, because the full suite failed 116 assertions in
`invlet_test.cpp`.

The reason is not fixable at that call site. Inventory letter hygiene runs on entry
to the flat inventory: `inventory::update_invlet()` strips a letter another item has
claimed, and decides that by asking `Character::invlet_to_item()`, which does this:

```cpp
// Visit top-level items only as UIs don't support nested items.
// Also, inventory restack logic depends on this.
return VisitResponse::SKIP;
```

An item in a pocket is invisible to the letter system **by design**. Route a t-shirt
into your jeans and it keeps letter `a` while the jeans hold `a` too, with no code
that will ever reconcile them.

Note this is not a defect the takeoff change introduced. Pickup already routes into
worn pockets, so anything picked up into a pocket has the same letterless status
today. `takeoff()` is simply the path an upstream test happened to cover.

The work is saved as a patch under the session scratchpad —
`takeoff-routing.patch`, restorable with `git apply`. It should not be re-applied
until the letter system understands nested items.

## Open findings, not fixed

Ranked by what a player is likely to hit.

1. **Inventory letters do not exist for pocketed items** (`character.cpp`, in
   `invlet_to_item()`). The blocker above. Fixing it means teaching the letter and
   restack machinery about nested items, which the comment says the UIs do not
   support. This is the keystone: several other gaps close behind it, and takeoff
   routing becomes safe.
2. **Every UI nests exactly one level.** `inventory_selector` (both the worn and
   the newly added carried path), and `advanced_inv_pane.cpp` for `AIM_INVENTORY`,
   list a container's contents but not a container-inside-a-container's. Coins in a
   wallet in your jeans are carried and invisible. The recursion added to crafting
   and to `pocket_destinations()` this session is the pattern to copy.
3. **NPC AI reads the flat inventory only** (`character_oracle.cpp`, both
   `can_wear_warmer_clothes()` and `can_make_fire()` use `inv_const_slice()` and
   only `front()` of each stack). An NPC whose firestarter is in a pocket will not
   know it can make a fire.
4. **`iexamine.cpp` cloning vat** calls
   `p.i_rem( p.inv_position_by_item( items[x] ) )` on an item found via
   `all_items_with_id()`, which does see pocketed items. For one in a pocket the
   position is `INT_MIN` and the removal misses, so the vat runs without consuming
   the vial. Rare path, real duplication.
5. **`pocket_data::rigid` is never read for volume.** `item::volume()` adds contents
   only when the *itype* is non-rigid, and `item_contents::item_size_modifier()`
   says outright that gating on `pocket_data::rigid` "belongs with the phase that
   authors real pockets". So the wallet's three `"rigid": false` pockets do nothing
   and a stuffed wallet stays 200 ml. Only 31 of 193 pocketed armor itypes declare
   `"rigid": false`, so most clothing does not bulge when filled. Changing this moves
   the volume of every pocketed item in the game and wants its own session.
6. **Nested inserts do not re-check ancestor capacity.** Putting an item into a
   wallet inside a trouser pocket can push that pocket over its weight limit;
   nothing looks up the chain. The game already handles over-full pockets, so this
   is untidy rather than dangerous.
7. **`ask_pocket_destination()` declines below two destinations**
   (`pocket_destination_menu.cpp`) while `examine_item_menu.cpp` offers the
   MOVE_TO_POCKET entry at one or more. With exactly one destination the entry
   appears and silently does nothing.

## Notes for the next session

`item::is_container()` is `!!type->container` — the legacy slot, false for every
CDDA-style pocketed item. Two separate bugs this session came from code using it to
mean "holds things". Grep for it before trusting any call.

`VisitResponse::NEXT` descends into pockets, so anything using `visit_items()` or
`items_with()` already sees pocketed items. The gaps are all in code that walks
`worn` and `inv_const_slice()` by hand. That grep is the cheapest way to find the
next one.

## Addendum — second sitting, 2026-09-05

Two more open findings closed. `main` now carries seven pocket fixes; the repo-root
playtest exe is the 03:59 build and `cataclysm-bn-tiles_old.exe` is the 01:28 one.

- **Finding 7 was already fixed** and the list above is wrong about it.
  `ask_pocket_destination()` returns the single destination rather than declining,
  at `pocket_destination_menu.cpp:22`. Nothing to do.
- **Finding 3, the NPC oracle** (`6a70a056bd`). `can_wear_warmer_clothes()` and
  `can_make_fire()` now search the inventory recursively and the contents of worn
  items recursively. Worn garments themselves stay unread on purpose: that is the
  behaviour they always had, and a coat already on the character is neither a coat
  it could put on nor fuel it ought to burn. Three sections in `behavior_test.cpp`,
  each watched failing first, and each asserting `inv_position_by_item() == INT_MIN`
  so the test cannot quietly stop testing pocketed items.
- **Finding 4, the cloning vat** (`85fd08a533`). Removal now goes through
  `item::detach()`, which is location-agnostic.

  The fix uncovered a **latent use-after-free** that the broken removal was hiding.
  `selected_syringe` is read for its specimen vars all the way down the block, and
  the matched item is normally `selected_syringe` itself; the `detached_ptr` was
  scoped to the `if` branch, so in the flat-inventory case the item was freed and
  then read. It never fired only because the pocketed case removed nothing. The
  detached sample is now held in the enclosing scope.

  The vat is reachable only through a `uilist`, so the added test characterises the
  two properties the fix turns on rather than driving the vat. It passes against the
  unfixed code and says so in its comment.

Full suite after both: **1,131 cases, 1,127 passed, 4 failed** — the four documented
vision tests, unchanged.

### Tooling notes

- **Neither formatter is installed.** `astyle` is absent entirely, and
  `build-scripts/format-cpp.sh` only finds `clang-format` if
  `C:/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/Llvm/x64/bin`
  is on PATH. Both src files here were written to astyle style by hand.
- **Do not run the formatter over `tests/`.** `format-cpp.sh` sends `tests/` to
  clang-format, but the test files are not clang-format-clean:
  `item_pocket_test.cpp` came back with 3,116 lines of churn. Format the file you
  touched, read the diff, and revert if it reformats anything you did not write.
- **`vcvars64.bat` prints `'vswhere.exe' is not recognized`** and then initialises
  x64 correctly anyway. Harmless. `Enter-VsDevShell` does not work here because
  `vswhere.exe` is not at the path the module expects, so the batch file via
  `cmd /c` is the working route.

### What is left

Findings 1, 2, 5 and 6 stand as written above. Ranked as before, the invlet
keystone (1) still gates the most.

## Addendum — third sitting, 2026-09-05

Finding 2 closed: every inventory screen now nests to any depth.

- **`inventory_ui.cpp`.** A new `add_contained_items()` recurses through CONTAINER
  pockets and replaces both one-level loops, the worn one and the carried one added
  earlier today. `topmost_parent` deliberately keeps meaning *outermost* container:
  it is what the category-list copy names in its caption, and what bounds the walk
  up the chain. Depth is carried by `indent` instead.
- **Collapse now asks the whole chain.** `under_collapsed_parent` walks
  `item::parent_item()` from the entry up to `topmost_parent`. Asking only
  `topmost_parent` meant collapsing a bag *inside* a garment did nothing at all -
  its contents kept drawing until the garment itself was shut.
- **The reorder pass is depth-first**, keyed on the item's own container rather
  than its outermost one, so a bag's contents follow the bag instead of being
  flattened in beside it. Each level rescans `entries` in sorted order, which is
  what keeps siblings sorted.
- **Nested containers are collapsible.** The `[+]`/`[-]` marker and the collapse
  key both now test `is_tree_node()`, one shared predicate, so the marker and the
  key cannot disagree about what can be folded.
- **`advanced_inv_pane.cpp`.** `collect_pocketed_items()` recurses. It also walks
  containers held in the flat inventory, whose contents the pane never listed at
  all - the stack loop lists the bag but never what is in it.

Six tests in `inventory_ui_test.cpp`. Five fail on a full revert of the three source
files. The collapse test needed a second pass: it first passed against the unfixed
code, because the rock was never drawn at all, so `CHECK_FALSE` was trivially true.
It now asserts the rock is visible while the bag is open and gone once it is shut,
and was re-verified against a build with the enumeration fixed but *only* the
ancestor walk reverted, where it fails on the collapse assertion itself.

### Finding 6 is bigger than it looks

Read before attempting it. Insertion *is* gated at the direct pocket -
`item_contents::insert_item_impl()` refuses when `best_pocket()` returns null,
unless forced - so finding 6 is not "add a missing check", it is "extend refusal to
the ancestor chain", which starts failing insertions that succeed today.

Two things make that more than a small edit:

- **There is no `remaining_weight()`.** `item_pocket` exposes `remaining_volume()`
  and nothing equivalent for mass; `max_contains_weight` is only read inside
  `can_contain()`.
- **It is entangled with finding 5.** Volume does not propagate out of a rigid
  container today, so an ancestor volume check would be nearly a no-op and the
  weight check would carry the whole behaviour. Finding 5 changes exactly that.

`item_pocket.h` also documents a **dry-run enforcement audit** from phase 1 -
`record_pocket_audit_miss()` / `pocket_audit_report()` - whose stated purpose is
that "an empty report after exercising the game is the evidence that enforcement
can be enabled". Extending that ledger to ancestor over-capacity is the additive,
zero-gameplay-change move; turning refusal on is a gameplay change and wants a
playtest behind it. That call was left to the owner rather than made while AFK.

### Still open

1 (the invlet keystone), 5 (rigid volume) and 6 (above). 5 and 6 want each other,
and 6 wants the owner's decision on whether to start refusing.
