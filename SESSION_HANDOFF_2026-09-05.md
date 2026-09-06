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

## Pocket system review — 2026-09-05

A sweep of the whole pocket surface, prompted by the owner. Two defect *classes*
came out of it, thirteen bugs between them, all now fixed. Both classes have the
same root: code that predates pockets asks "where is this item" in a way that
cannot express "inside another item".

### Class one: hand-walking the flat inventory (`0565afdf09`)

The grep this file already recommended - `inv_const_slice()` and `worn` walked by
hand - found five more systems, on top of the five fixed earlier today:

| System | Symptom |
|---|---|
| `npctrade::init_selling` | a shopkeeper who had put their wares away had **nothing to sell** |
| `npctrade::init_buying` | one level only, so a bag in a pocket hid its contents from trade |
| `npc::mug_player` | a mugger found nothing worth taking and fled |
| `memorial_logger` | the death memorial listed almost nothing |
| `advanced_inv_area::get_container` | a jar in a pocket could not be the AIM container |

Rather than a sixth hand-walk, these now share **`Character::items_in_pockets()`**
(`character.cpp`): worn pockets and inventory containers, recursive, excluding the
garments and top-level items every caller already has from `worn` and
`inv_const_slice()`. `advanced_inv_pane`'s local recursion was folded into it.

`character_oracle` deliberately keeps its own predicate walk: it short-circuits and
runs per NPC per turn, where building a vector would not pay.

### Class two: `get_item_position()` names the container (`26414b8121`)

This one is nastier than the `INT_MIN` case and was not on any list. It finds an
item's owner with `has_item()`, which **descends into contents**, so an item in a
pocket answers with the index of *the garment holding it*. Not `INT_MIN`, and
nothing that names the item - so callers acted on the wrong thing silently.

| Site | Symptom |
|---|---|
| `repair_item_actor`, `iuse_actor.cpp` | destroying a pocketed item removed **the garment** and spilled everything else in it |
| clothing mod actor | same |
| `monexamine` mech battery | `items_with()` offers a battery in a pocket, then `i_rem()` fitted **the backpack** into the mech |
| `inventory_column::set_stack_favorite` | see below |
| tent (`iuse.cpp`) | a tent in a pocket could not be folded - "take it off first" |
| stimpack (`iuse.cpp`) | activated from inside a backpack without being worn |
| instrument (`iuse_actor.cpp`) | playable from inside a bag |

The favourite case was **mis-diagnosed twice before the test caught it**. It looked
like "favourites the garment"; it is actually that a pocketed item reports
`item_location_type::container`, which matched none of the three branches, so
favouriting one was a **silent no-op**. That matters beyond tidiness: a favourite
is what keeps an item out of the overflow drop.

`get_item_position()` now carries a comment saying it cannot address a pocketed
item and naming the alternatives (`item::detach()`, the pointer, `is_worn()`).

**Nesting made this class reachable.** Pocketed items were barely selectable before
today; now they are selectable from every screen, so each of these was one keypress
away.

### Checked and found sound

- **Activities that store an item index.** `assign_activity(..., get_item_position(...))`
  appears seven times and looked like the same trap. It is not: only one handler
  dereferences a stored index (`activity_handlers.cpp:2820`, repair), and that line
  is dead - `repair_item_actor` puts the tool in `act->targets` and the handler
  resolves the pointer first. The other six pass the index into a field nothing
  reads.
- **`VisitResponse::NEXT` consumers.** Anything using `visit_items()`, `items_with()`
  or `has_item()` already sees pocketed items. That is *why* class two bites -
  the search finds the item, and only the addressing is wrong.

### Not fixed, and why

- **The overflow drop** (`activity_item_handling.cpp:631`) picks what to shed from
  the flat inventory only, so a character over capacity with everything in pockets
  has nothing it will drop. Guarded against walking off an empty inventory
  (`test_overfull_pocket_vest`), so it is not a crash - but it cannot relieve the
  overflow either. Changing what the drop takes decides which of the player's
  things hit the floor, which wants the owner's say-so.
- Findings 1, 5 and 6 stand as recorded above.

### Verification note

Three of the fixes here have no suite coverage and are inspection-verified only:
the two destroy paths and the mech battery all sit behind a `uilist` or an rng
critical failure. What *is* pinned is the trap itself -
`get_item_position answers with the container for a pocketed item` in
`inventory_ui_test.cpp` - so the root cause cannot change quietly. The favourite
and stimpack fixes have tests, both watched failing first.

## Per-pocket `rigid` is not the change it looks like — 2026-09-05

The owner cleared this to be applied. It was built, measured, and **reverted**,
because applying it alone does nothing at all. Read this before trying again.

`item::volume()` gates the whole contents sum on the *itype*:

```cpp
// Non-rigid items add the volume of the content
if( !type->rigid ) {
    ret += contents.item_size_modifier();
}
```

**`itype::rigid` defaults to `true`** (`itype.h:1096`). So for almost every item
the sum never runs, and a per-pocket check inside `item_size_modifier()` is
never reached. A version of that check was written, tested against
`22lr_ammo_box_100`, and its two tests turned out to be measuring the outer gate
rather than the new code - the "rigid pocket does not bulge" test passed because
`type->rigid` already blocked it, and the classic-mode test failed for the same
reason.

**CDDA has no itype gate.** `item::volume()` there is simply
`ret += contents.item_size_modifier();` (CDDA `item.cpp:2302`), and rigidity is
decided entirely per pocket. So the real port is *removing* the itype gate, not
adding a pocket one beside it.

That is where the danger is, and it is the opposite of what this file said
earlier. The two defaults disagree:

| | default | count in `data/json` |
|---|---|---|
| `itype::rigid` | **true** | - |
| `pocket_data::rigid` | **false** | 28 pockets declare `true`, 69 declare `false`, ~388 declare nothing |

Drop the itype gate today and every one of those ~457 non-rigid-by-default
pockets starts swelling its container. That is the game-wide volume change the
original finding 5 warned about, and it lands on clothing, not just on the 28
ammo boxes that actually want it.

**So the order of work is:** author `"rigid": true` across the pockets that
should not bulge *first*, then remove the itype gate, then playtest. Not the
other way round. The classic-mode requirement the owner set - classic keeps the
BN inventory - is satisfied by making the pocket check skip when
`pockets_are_classic()`, which the reverted patch already did correctly; that
part was sound and can be lifted from `git show` of this session if useful.

## Takeoff routing: what the invlet blocker actually is — 2026-09-05

Re-reported from playtest: taking a garment off puts it loose in the inventory
"inside no pocket", while every other acquisition path routes. The one-line fix
was rebuilt and **reverted again**. This section records what the 116 failures
actually mean, because the earlier note was too vague to act on.

The routing itself is trivial and works - `i_add_routed()` in place of
`inv.add_item()` in `Character::takeoff()` (`character.cpp`, the `res == nullptr`
branch). A test wearing a backpack and socks, taking off the socks and asserting
they land in the backpack, passes immediately.

**The 116 `invlet_test.cpp` failures are duplicate inventory letters, not a
cosmetic test artifact.** The failure reads:

```
expect 1st item to have none invlet
1st item actually has cached invlet
```

The suite assigns the *second* item's letter **after** the move. Letter hygiene
runs on entry to the flat inventory, so unrouted the first item is stripped and
ends with none. Routed, it sits in a pocket where that reconciliation never
reaches it, and both items end up answering to the same key.

Two fixes were tried and **neither worked**:

1. Reconciling on the routing path - calling `inv.update_invlet( stored, false )`
   from `Character::note_pocketed_pickup()`. Wrong side: the conflict is created
   later, when the *other* item is granted the letter.
2. Making `Character::invlet_to_item()` descend (`VisitResponse::NEXT` instead of
   `SKIP`). Still 116. Worth knowing that its comment's first justification -
   "UIs don't support nested items" - **is now false**, since the nesting work
   this session; but changing it alone does not close this.

So the reconciliation lives somewhere in the assignment path that neither of
those touches, and finding it is the actual keystone task. Budget it properly
rather than as a one-liner. The patch for both attempts plus the routing change
is in the session scratchpad as `takeoff-experiment.patch`.

**Until then the current behaviour is not a bug to re-report:** a garment comes
off into the flat inventory, and is offered as a drop when it does not fit. What
the owner saw - a backpack forced to the ground, jeans going to the inventory -
is that capacity check working, not two different code paths.

---

## Addendum, 2026-09-06 - trade overflow: goods no pocket will hold

**Playtest report:** a double-barrel shotgun taken as a trade reward, and a
shovel given as a quest reward, both turned up in the flat inventory with no
message. Smaller reward items in the same trade went into pockets correctly.

**Not a routing failure.** `i_add_routed()` was present and running on all three
paths. The items simply fit no worn pocket - the shotgun on size, and the
`makeshift_sling` crafting report is the same story, since the sling is **5 L in
its own volume** and most worn pockets are 1-4 L. The flat inventory is the
unconditional fallback, so anything nothing will hold lands there loose and
silent.

The owner's call, after weighing three options: leave pickup alone (it already
prompts through `handle_problematic_pickup`), and make **trade, barter and NPC
rewards** ask instead. A flat-inventory size cap was considered and rejected on
the owner's objection, which is correct and worth recording: a cap makes the
inventory accept an item once and refuse the same item the second time, with
nothing on screen to explain the difference.

### What was built

New file `src/trade_overflow.h` / `.cpp`. New files are the cheapest fork change
after JSON, and the three call sites take a one-line swap each:

| Path | File |
|---|---|
| Trade window | `npctrade.cpp` `transfer_items()` |
| "Ask for something" reward | `npctalk_funcs.cpp` `give_equipment()` |
| Dialogue and mission rewards | `npctalk.cpp` `set_u_buy_item()` |

`trade_overflow::deliver()` gives worn pockets first refusal exactly as
`i_add_routed()` does, and only what they all turn down reaches a menu offering
wield / wear (armour only) / carry loose / drop, plus two capital-letter
answers that stick for the rest of the same delivery so a ten-item trade asks
once.

### Two deliberate calls, both one line to reverse

- **Escape carries the item, it does not drop it.** Dropping on cancel would
  leave goods the player just paid for on a shop floor they are walking away
  from - the same class of surprise the menu exists to remove.
- **A character wearing nothing with a pocket is never asked.** Otherwise an
  ungeared character gets a menu for every traded item. This is the same guard
  `pickup.cpp` already applies, and it is what keeps classic mode untouched.

`test_mode` is checked in `deliver()` rather than in `overflow_needs_prompt()`,
deliberately: the suite runs with `test_mode = true`, so putting the guard in the
predicate would have made the predicate untestable and the whole feature
unverifiable.

### Verification

Every branch was broken and watched to fail before being trusted:

| Break | Tests that caught it |
|---|---|
| `overflow_needs_prompt` returns true always | 3 cases (ungeared avatar, NPC, liquid + casing) |
| drop branch neutered | 1 case, 2 assertions |
| wield and wear branches neutered | 2 cases |
| the "cannot lose it" `i_add` fallback removed | 2 cases |

Full suite afterwards: **1,164 cases, 1,160 passed, 4 failed** - the four
documented vision tests at `vision_test.cpp:256`.

### Still open

- **Which garments deserve more generous `max_item_length` / volume.** A balance
  call, not a code one. `makeshift_sling` was given an explicit 10 L / 150 cm
  pocket; nothing else has been reviewed.
- **The pickup-menu nesting feature** (collapsible container entries, ALT
  modifier) from the 2026-09-05 playtest still needs its own scoping pass.
- **Per-pocket `rigid`** remains dead until the JSON is authored and the
  `itype::rigid` gate is removed.

---

## Addendum, 2026-09-06 (2) - vehicle cargo, and crafting stops stashing silently

Follow-up to the trade-overflow work above, after reading what CDDA actually
does rather than recalling it.

### What CDDA does

CDDA has **no flat inventory** - `Character::inv` survives only as an invlet
shim, and `try_add()` goes to pockets and nowhere else. So the situation CSE hit
cannot arise there. `Character::i_add`
(`../CDDA/src/character_inventory.cpp:451`) runs a fixed ladder: best pocket,
then wield if hands are free, then `add_item_or_charges` on the ground, and
finally return `nowhere` if the caller passed `allow_drop = false`. Trade
(`npctrade.cpp:89`) opts into the whole ladder explicitly. **No menu anywhere on
the acquisition path.** CDDA's prompts appear only when something already
carried stops fitting - the reload-overflow menu at
`../CDDA/src/activity_actor.cpp:8064` is the closest analogue to ours, and it
offers Wield / Drop with escape meaning drop.

CSE cannot copy that model outright, because CSE still has the flat inventory
that classic mode is defined in terms of. What was worth stealing was the one
step CDDA has that CSE lacked everywhere: **vehicle cargo before bare ground**.

### Changes

`src/trade_overflow.{h,cpp}` renamed to **`src/pocket_overflow.{h,cpp}`** - it is
no longer trade-only. The `trade_overflow` class keeps its name, since it really
is the per-delivery trade helper.

1. **`pocket_capacity_binds( who )`** hoisted out of `overflow_needs_prompt()`.
   One predicate, two callers, one place to change the rule: false in classic
   mode and for anyone wearing nothing with a pocket.
2. **`drop_here()` now calls `put_into_vehicle_or_drop( ..., too_large, ... )`**
   instead of `map::add_item_or_charges`. Cargo space wins over the floor, the
   helper carries its own messaging, and it handles pickup ownership - all three
   were missing before.
3. **`crafting.cpp` `set_item_inventory()`**: for the avatar, when
   `pocket_capacity_binds()`, a result no worn pocket will hold now falls through
   to `set_item_map_or_vehicle()` with an explanatory message, rather than
   landing in the flat inventory unannounced. That helper already preferred a
   vehicle and a workbench over bare ground.

**NPCs deliberately keep the flat-inventory backstop in crafting.** An NPC that
crafted something too big for its own pockets would otherwise leave it on the
floor and never think to pick it up again. This is why the crafting guard reads
`who.is_avatar() && pocket_capacity_binds( who )` rather than the predicate
alone.

### Verification

Each behaviour was broken and watched to fail:

| Break | Caught by |
|---|---|
| crafting strict path disabled | the new crafting test, 2 assertions |
| `drop_here` reverted to a bare map drop | the new vehicle-cargo test, 2 assertions |
| `pocket_capacity_binds` returns true always | 2 cases |

**One flaky test was caught and fixed before it shipped.** The crafting test was
first written against `carver_off`, which is difficulty 4 - so the craft itself
can fail, yielding nothing, and "nothing on the floor" is indistinguishable from
the bug being present. It failed one run in five. It now uses `crude_picklock`,
which has no difficulty and cannot fail: stable over 8 consecutive runs. The
comment above the test says so, because the next person will be tempted to pick
a more interesting recipe.

---

## Addendum, 2026-09-06 (3) - Drop and Leave in the pickup menu

**Playtest report:** picking up an item no pocket will hold offers only Wear and
Wield. Both work, but a player who has decided they do not want the item has no
visible way out.

Escape has always been that way out - `handle_problematic_pickup()` returns
`CANCEL` for it, and `pick_one_up()` returns the item to wherever it came from.
Nothing in the menu ever said so, which is the whole defect: the answer existed
and was invisible.

### Changes, all in `src/pickup.cpp`

Two entries added to `handle_problematic_pickup()`, and two values to
`pickup_answer` ahead of `NUM_ANSWERS` so the existing
`choice <= CANCEL || choice >= NUM_ANSWERS` guard admits them:

- **`LEAVE`** (`l`) - shares the `CANCEL` arm. The item stays in the tile,
  container, corpse or vehicle it was found in.
- **`DROP_UNDERFOOT`** (`d`) - takes the item out of whatever holds it and sets
  it down where the character stands, through
  `apply_overflow_choice( ..., overflow_choice::drop )`. **Reused deliberately
  rather than rewritten:** that is the same "put it down" crafting and trade
  use, so vehicle cargo beats the floor everywhere, and it already has tests.

A new `disposed` flag distinguishes "handled, but never carried" from
"picked up":

- The children block stays under `picked_up`. Dropping a container must not then
  pull its contents out of it and into the character.
- `u.moves -= moves_taken` moved out to `picked_up || disposed`, since setting an
  item down costs time too.
- The return is now `picked_up || disposed || !did_prompt`. Dropping is a
  deliberate answer, so a multi-item pickup carries on; **leaving still stops the
  batch**, exactly as escaping always has.

### Coverage, stated honestly

The drop *action* is covered - `apply_overflow_choice( drop )` has tests for both
the underfoot and the vehicle-cargo case. The **menu wiring is not unit-tested**,
because `handle_problematic_pickup()` is a `uilist` and `pick_one_up_options`
is internal to `pickup.cpp` with no way to inject an answer. The rest of that
menu (Wear, Wield, Spill, Empty) has never been covered either. If this needs
real coverage later, the seam to build is a testable answer-to-action function
that both the menu and a test can call.

The `disposed` refactor is covered by the existing pickup tests only in the sense
that they still pass; they do not exercise the new arms.

---

## Addendum, 2026-09-06 (4) - the pickup nesting feature is now scoped

The one outstanding item from the 2026-09-06 playtest that was deferred as "a
feature, not a bug" now has a written plan:

**`docs/superpowers/plans/2026-09-06-pickup-menu-pocket-nesting.md`**

It carries the owner's spec verbatim, six numbered requirements, a three-step
build order, and a phase 2 for the trade menu. **Every design question is
answered** - the owner settled all five on 2026-09-06 - so it needs no input
before someone starts.

It is deliberately written for a cheaper or non-Claude agent: the two
"this already exists" findings are hoisted into a STOP section at the top, the
judgement calls are stated as instructions rather than considerations, and the
verification step is a hard gate with exact commands rather than a principle.
The owner may hand this to Cursor's Composer 2.5.

The finding worth knowing before opening that file: **the pickup menu already
has parent/child rows** - `pickup_count::parent`, `::children`,
`::all_children_picked`, a child marker in the renderer, and selection that
already cascades from parent to children. All of it is keyed on `item_drop_token`
("these landed here together") rather than on live pocket containment. The work
is to give that existing machine a second source of children, not to build a new
tree beside it.
