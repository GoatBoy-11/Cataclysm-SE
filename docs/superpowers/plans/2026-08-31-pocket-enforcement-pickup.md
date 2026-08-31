# Pocket Pickup Enforcement Plan (Milestone 3 of "pockets in play")

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:executing-plans or
> superpowers:subagent-driven-development. Keep the checkboxes current.

**Goal:** In a full-pocket world, what no worn pocket accepts cannot be stashed
by picking it up — wield it, or leave it. This is the CDDA feel: pockets are
the storage, not a view over it.

**Blocked by:** pickup routing
(`2026-08-31-pocket-pickup-routing.md`) — must be landed and green.

**Gate:** `POCKET_SYSTEM` full only. Classic worlds keep BN pickup exactly.

## Design

In `src/pickup.cpp`, the `with_det` flow after routing: when
`pockets_are_classic()` is false and `i_add_to_worn_pockets` handed the item
back, do not fall through to the flat-inventory stash options. Reuse the
existing "does not fit" path that pickup already has for over-volume items, so
the player still gets the wield / cancel choices that path offers today.

Deliberately narrow:

- **Only characters who wear usable pockets.** A character with none keeps the
  legacy flat stash (`wears_usable_pockets` in pickup.cpp): ungeared avatars,
  NPCs and pre-pocket content keep working, and headless tests with naked
  fixtures stay green. Discovered via `invlet_test.cpp:226`.
- **Pickup only.** Unload, crafting returns, quest rewards, AIM transfers and
  NPC behaviour keep flat-adding. Safe (nothing is lost), inconsistent on
  purpose; each migrates in its own later step if wanted.
- **No capacity rewrite.** Milestone 1's arithmetic already keeps totals
  honest. The flat inventory keeps existing as the hidden fallback layer for
  the non-pickup paths above.
- **No item is ever destroyed or dropped silently.** The refused item stays
  where it was (on the ground), which is where the player left it.

## Tasks

- [x] Tests (`[pocket][routing][enforce]`): full mode + an item every worn
  pocket bars: pickup flow classifies it as unstashable; classic mode: stashes
  to flat inventory as today. Drive the decision helper directly rather than
  the interactive menu, mirroring how existing pickup logic is tested.
- [x] The pickup.cpp change.
- [ ] Full suite green (four CPU-backend `vision_*` failures stay
  environmental).
- [ ] In-game: bar an item from every pocket of everything worn, walk over it,
  `g` — expect the too-big/wield flow, not a quiet stash. Classic world:
  stashes as always.

## Out of scope

- Nested inventory display (milestone 2, parked).
- Enforcement on unload / crafting / AIM / NPCs.
- Removing the flat inventory. It stays as the compatibility layer.
