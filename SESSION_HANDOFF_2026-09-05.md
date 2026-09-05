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
