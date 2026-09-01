# Nested Inventory Display Plan (Milestone 2)

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans or
> superpowers:subagent-driven-development. Keep the checkboxes current.

**Goal:** Show what a container holds, indented beneath it, in the inventory
screens - and let those nested entries be acted on, so an item routed into a
pocket can be taken, eaten, wielded or dropped like any other.

**Why this is not cosmetic.** Pickup routes items into worn pockets. No
inventory screen renders pockets, so a routed item is invisible and
unreachable: the feature currently takes things away from the player. Oliver
found this in minutes of play (reports 3 and 4). Emptying a garment wholesale
via `unload` is the stopgap that landed first; this is the fix.

**Blocked by:** pickup routing (landed), enforcement (landed), the capacity
model (landed).

**Gate:** `POCKET_SYSTEM` full only. Classic pools storage into one
compartment and must look exactly like stock BN, so no nesting there.

## The hard part

CDDA's nesting rests on `item_location`, which BN does not have:
`inventory_entry` holds `std::vector<item *>` and knows nothing of parents.
`src/inventory_ui.cpp` is still byte-identical to CBN, so everything here is
first-party divergence in a heavily-touched upstream file - the most expensive
category in CLAUDE.md's ranking. Keep the surface as small as it can be.

BN already gives us the two pieces the port needs:

- `item::parent_item()` - who holds this item.
- `item_contents::pocket_containing()` - which pocket it sits in (added for
  the sealing work).

So an entry can carry a parent pointer without inventing an ownership type.

## Design

1. `inventory_entry` gains `item *topmost_parent = nullptr` and `int indent = 0`.
   Purely additive; existing construction sites are unaffected.
2. `inventory_selector::add_character_items` walks each worn item's
   general-purpose pockets and adds their contents as entries whose parent is
   the garment. Skip in classic mode.
3. `inventory_column::get_entry_indent` (already exists, line ~376) adds two
   spaces per indent level, so nested rows line up under their container.
4. Sort so children follow their parent immediately, before the next
   top-level entry. This is the part most likely to fight BN's existing
   category sorting - if it does, prefer a stable post-sort pass that lifts
   children into place over rewriting the comparator.
5. Actions on a nested entry act on that item. It is already an `item *`, so
   the existing action paths work unchanged - this is the payoff of BN's raw
   pointers over `item_location`.

## Tasks

- [x] Tests first (`[pocket][inventory]`): a worn container's contents appear
  as entries; they carry the right parent and indent; classic mode shows none;
  an item in a pocket can be selected and dropped. Drive
  `inventory_selector` headlessly as `tests/inventory_ui_test.cpp` does if one
  exists, otherwise assert on the entry list the selector builds.
- [x] `inventory_entry` fields.
- [x] Entry construction for worn pockets.
- [x] Indentation.
- [x] Ordering.
- [x] Full suite green (four `vision_*` CPU-backend failures stay
  environmental).
- [ ] In-game (Oliver, outstanding): pick items up, open inventory, see them nested under the
  garment; act on one directly; classic world unchanged.

## Out of scope

- The advanced inventory (AIM) screen.
- Nesting items inside items inside items - one level is enough for pockets.
- Collapsing branches. The `is_collapsed()` flag exists and is saved but has
  no reader; wire it up only if the display makes it obviously wanted.
