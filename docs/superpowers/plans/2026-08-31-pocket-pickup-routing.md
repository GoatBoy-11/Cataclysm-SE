# Pocket Pickup Routing Plan (Milestone 1 of "pockets in play")

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development
> or superpowers:executing-plans. Steps use checkbox (`- [ ]`) syntax for tracking.
> **Keep the boxes current — the older pocket plans' boxes rotted and CLAUDE.md now
> warns about them.**

**Goal:** Picking an item up tries the pockets of what the character is wearing
before the flat inventory, so pocket priorities, rules and capacities finally
govern play. The flat inventory catches everything no pocket accepts, so nothing
is ever lost and classic worlds behave exactly as today.

**Why this is small:** every hard part already exists and is tested —
`best_pocket()` ranking (priority, whitelists, auto-insert off), `put_in()`
refusal, per-pocket capacity, the organizer and item-info views that show the
result. This plan only connects pickup to them. The seam is proven: BN already
routes ammo through `Character::i_add_to_container` (`src/pickup.cpp:308`),
which takes a `detached_ptr` and returns the leftover. Copy that shape.

**Gate:** everything below applies only when `POCKET_SYSTEM` is `full`
(`pockets_are_classic()` is the existing check). Classic mode must be
byte-for-byte today's behaviour.

## Design

### New method: `Character::i_add_to_worn_pockets`

`detached_ptr<item> i_add_to_worn_pockets( detached_ptr<item> &&it )` in
`src/character.cpp`, beside `i_add_to_container`.

- Classic mode, or a null/liquid/CASING-flagged item: return unchanged.
- For each worn item, ask `contents.best_pocket( *it )` (settings considered).
  Rank candidates across containers by the chosen pocket's
  `settings.priority()`; tie goes to wear order. This mirrors within-container
  ranking one level up.
- Winner takes the item via `container.put_in( std::move( it ) )` — which
  re-runs best_pocket internally and can refuse; a refusal hands the item back
  and the method returns it for the flat inventory. Never force-insert here.
- On success print the existing holster-path message: "You put the %1$s in your
  %2$s." and return an empty detached_ptr.
- No move cost in this method; pickup already charges moves.

### Call site: `src/pickup.cpp`

In `with_det`, directly after the ammo `i_add_to_container` line and under the
same `!opts.preferred_option` guard:

    newloc = u.i_add_to_worn_pockets( std::move( newloc ) );

The existing "picked up everything into containers" branch right below already
handles the now-empty detached_ptr. Do not touch the other `i_add` calls (wield
returns, unload, butchery) in this milestone.

### Capacity accounting (the one real trap)

`volume_carried()` is `inv.volume()` and `volume_capacity()` sums worn
`storage`. An item routed into a worn pocket consumes neither, so capacity
would double-count. Fix in `Character::volume_capacity_reduced_by`
(`src/character.cpp:3227`): when `POCKET_SYSTEM` is full, a worn item
contributes `max( 0_ml, storage - volume of its pocket contents )`. Total
usable capacity then stays the same wherever an item sits. Real per-pocket
enforcement replaces this arithmetic in milestone 3.

## Tasks

- [x] Tests first, in `tests/item_pocket_test.cpp` (`[pocket][routing]`):
  - [x] Picked-up item lands in a worn container's pocket, not the flat inv.
  - [x] A priority-5 pocket on one garment beats priority-0 on another.
  - [x] An item barred by rule from every pocket falls back to the flat inv.
  - [x] Auto-insert-off pockets are skipped.
  - [x] Classic mode: flat inv, pockets untouched.
  - [x] volume_capacity does not grow when an item moves from inv to a pocket.
  - Use `npc`/`avatar` fixtures as the existing `[visitable]` tests do; wear
    `test_two_pocket_bag` (it is GENERIC — if it cannot be worn, add an armor
    fixture `test_pocket_vest` to TEST_DATA, mirroring its pockets).
- [x] `i_add_to_worn_pockets` per the design.
- [x] Pickup call site.
- [x] Capacity arithmetic. (Landed as the cleaner model: `volume_carried()`
  counts worn-pocket contents in full mode, mirroring weight; capacity stays the
  plain storage sum; `can_pick_volume` asks carried. Classic math untouched.)
- [ ] Full suite green (the four `vision_*` CPU-backend failures stay
  environmental — see CLAUDE.md).
- [ ] In-game check: wear a rucksack, set a pocket to priority 5, pick items up
  off the floor, `o` on the rucksack shows them in that pocket; bar an item
  type and confirm it stays in the flat inventory.

## Out of scope (later milestones)

- Nested inventory-screen display (milestone 2).
- Refusing pickup when nothing fits — enforcement (milestone 3).
- Routing for unload / butchery / crafting returns.
- NPC-specific behaviour beyond "does not crash, tests stay green".

## Constraints (inherited, always)

- All 181 existing `.contents.` call sites compile unchanged.
- No item is ever destroyed: refusal always falls back to the flat inventory.
- Classic worlds and old saves behave exactly as before this plan.
