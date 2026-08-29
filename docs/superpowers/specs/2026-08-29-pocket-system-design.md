# Pocket System Port — Design

**Date:** 2026-08-29
**Project:** Cataclysm: Slop Edition (CSE)
**Status:** Approved design, not yet implemented
**Reference implementation:** Cataclysm: DDA at `5b915aea09`

## Problem

CBN forked from CDDA before CDDA's pocket system landed. CBN items declare a
single `"storage"` volume and hold a flat list of contents. CDDA items declare
`pocket_data` and hold a vector of pockets with independent volume, weight,
length and content restrictions, plus per-pocket player settings for priority,
whitelists, blacklists and auto-pickup.

CSE wants CDDA's pocket system, with three constraints:

1. CBN mods must keep working. Their items declare no `pocket_data` and never
   will, but they must gain functional pockets automatically.
2. The system should be switchable to a simpler "classic" behaviour, locked at
   world creation.
3. Existing saves must survive the transition.

## Decisions

| Decision | Choice |
|---|---|
| Port style | Faithful port, CDDA-compatible `pocket_data` JSON schema |
| Base-game JSON | Synthesize pockets for everything; hand-author a curated ~50–150 items |
| Optionality | Degenerate runtime mode, not a second container model |
| Save compatibility | Migrate on load |
| Port strategy | Reimplement behind CBN's existing `item_contents` seam |
| Mod item richness | Curated base items first; JSON pocket templates only if coverage falls short |

### Why a faithful port

Keeping CDDA's JSON schema identical means CDDA content and their ongoing
balance work drop into CSE as data. The two repositories share a root commit
(`69ffbb2953`), so CDDA can be added as a git remote and their commits
cherry-picked with real three-way merges rather than manual transcription.

Expected usefulness of future CDDA pocket work:

| CDDA change type | Usefulness | Why |
|---|---|---|
| `pocket_data` values on items | High, often cherry-picks cleanly | Pure data, no BN divergence |
| New `pocket_data` fields | Good | Self-contained: field, parse, one `can_contain()` check |
| `item_pocket.cpp` logic fixes | Moderate, hand-apply | Reasoning transfers; the patch does not |
| Item ownership / serialization | Low | Exactly where BN diverged |

Data compatibility survives code drift far better than code compatibility does.
That asymmetry is most of the justification for this design.

## Architecture

The port lives entirely behind CBN's existing `item_contents` seam.

```
item
 └── item_contents                 (public API held FIXED)
      └── std::vector<item_pocket>
           └── location_vector<item>   ← BN ownership, not CDDA's std::list
```

### `item_pocket` — new, `src/item_pocket.{h,cpp}`

Owns one `location_vector<item>`, a `const pocket_data*` into the itype's
definition, and mutable `favorite_settings` (priority, item and category
white/blacklists, auto-pickup and auto-unload flags).

`can_contain( const item& )` returns CDDA's `ret_val<contain_code>` so failure
reasons stay specific enough to surface in the UI ("too big", "not watertight",
"only holds one item").

This is the only file that must understand both codebases at once. CDDA's logic
is translated here; the ownership model is BN's.

### `pocket_data` — new, in `item_pocket.h`

Immutable, JSON-loaded, lives on `itype`, shared across all instances of a type.
Schema identical to CDDA's.

### `item_contents` — existing, internals replaced

Swaps its single `location_vector<item> items` for `std::vector<item_pocket>
pockets`. Existing public methods are reimplemented as fan-outs:

- `all_items_top()` concatenates across pockets
- `remove_top()` locates the owning pocket and delegates
- `front()` returns the first item of the first non-empty pocket

New pocket-aware methods are added alongside; callers migrate over time.

### The load-bearing constraint

**The 181 existing `.contents.` call sites must compile unchanged.** Anything
that would force a call-site edit is a design smell to be solved inside
`item_contents` instead.

This is what keeps the game runnable at every commit, and what confines future
upstream BN merge conflicts to one file rather than 181.

### Open implementation question

How `location_vector` obtains its owning-item back-pointer when it lives inside
a pocket rather than directly on `item_contents`. BN's ownership model requires
items to know their location. Resolve in the first implementation step rather
than guess.

## Legacy synthesis

`synthesize_pockets_from_legacy( itype &def )`, called from `Item_factory`
finalization alongside CDDA's existing `check_and_create_magazine_pockets()`,
after `copy-from` resolution.

### Guard

Fires only when `has_only_special_pockets( def )` — CDDA's existing predicate.
An item that already declares a CONTAINER pocket is skipped entirely.

| Item source | Declares `pocket_data`? | Result |
|---|---|---|
| CDDA content, curated CSE items | yes | Used as authored |
| CBN base game | no | Synthesized from `storage` |
| Any CBN mod, present or future | no | Synthesized from `storage` |

Mods load through the same `itype` pipeline, so mod support requires no
mod-specific code. A CBN mod written years from now against the old schema
still gets working pockets.

### Field mapping

```
islot_container.contains    -> max_contains_volume
islot_container.watertight  -> watertight
islot_container.seals       -> sealed_data
islot_container.preserves   -> spoil_multiplier = 0
islot_armor.storage         -> max_contains_volume, rigid = false
magazines / tool charges    -> check_and_create_magazine_pockets, near-verbatim
```

Unspecified fields take CDDA's defaults: `max_item_length` as the cube-diagonal
of the pocket volume, weight capacity effectively unbounded, `moves = 100`.
Armor storage is non-rigid so a loaded pack still encumbers correctly;
containers are rigid.

### Conflict rule

If an item declares both `storage` and `pocket_data`, `pocket_data` wins and a
`debugmsg` fires. Silent precedence produces bug reports nobody can reproduce.

### Stated fidelity limit

A synthesized pocket is one generic pocket. It cannot invent the interesting
part of CDDA's design — a backpack's separate water-bottle holster and
tucked-behind-back sheath exist only because someone authored them. Synthesis
guarantees correctness and compatibility, not richness. Richness comes from the
curated set and from CDDA content.

## Pocket templates

Synthesis gives a legacy item one generic pocket. Templates are the mechanism
for giving recognisable *kinds* of item a richer, plausible pocket set without
their author writing `pocket_data`.

### Do the curated items first

251 files under `data/mods/` use `copy-from`. Mod authors overwhelmingly define
a custom backpack as `"copy-from": "backpack"` plus a few overrides, and
`copy-from` propagates `pocket_data` like any other field.

**Authoring pockets on base-game items therefore covers descended mod items for
free, with no template machinery at all.** Curate first, then measure how many
mod items remain uncovered. That number decides how much of the rest of this
section is worth building.

### Match precedence

Templates resolve in strict order; the first match wins and stops.

| Priority | Key | Rationale |
|---|---|---|
| 1 | Authored `pocket_data` | Explicit always wins |
| 2 | `copy-from` ancestry | Free, precise, matches how mods are written |
| 3 | Opt-in `"pocket_template": "backpack"` | Zero guessing; costs the author one line |
| 4 | Flags and `itype_id` patterns | Machine-stable; ids beat display names |
| 5 | Volume thresholds | Crude, but never wrong about what the item *is* |
| 6 | Name substring | Last resort, only as a deliberately written rule |

### Why name matching ranks last

It was the original proposal and is retained only as a fallback. Substring
matching cuts both ways: `backpack` catches `backpack frame` and `empty
backpack`, which should not gain pockets, while `rucksack`, `knapsack` and
`daypack` all miss. Display names are also the field mod authors vary most and
change most often, where `itype_id` is near-stable.

### Rules live in JSON, not C++

Templates are declared in `data/json/pocket_templates.json`, which mods may
extend or override. A wrong template is then fixable without a recompile, and
mod authors can correct their own items.

### Two non-negotiables

**Conservative by default.** A template granting generous pockets to a guessed
item is a balance bug wearing the costume of a feature. Under-grant and let
authors opt in.

**Every match must be inspectable.** A debug command listing *item X received
pockets from rule Y* for every loaded item. Without it a mis-fired template is
unfindable, and the only symptom is a backpack that feels subtly wrong.

## Classic mode

World-default option `POCKET_SYSTEM`, values `full` (default) and `classic`,
registered in `add_options_world_default()`. World options are copied into the
world directory at creation, so the setting is frozen per world by existing
machinery. Needs one additional guard to be non-editable in-game.

**Synthesis is identical in both modes.** Every item gets its full pocket set
regardless. Classic is a runtime behaviour switch changing exactly three things:

1. `item_pocket::can_contain()` checks only aggregate volume and weight,
   skipping length, rigidity and restriction tests
2. `best_pocket()` degenerates to first-fit, ignoring priorities and
   white/blacklists
3. The inventory organization menu is unreachable

### Why runtime rather than load-time

An earlier framing collapsed each item to a single pocket at load time. That was
rejected: item finalization and world-option loading both happen during world
setup, and the ordering is unverified. Worse, it would bake the mode into item
definitions, making a save's contents depend on which mode loaded them.

The runtime switch keeps save data byte-identical between modes. A character
created in classic opens in full and vice versa, gaining or losing restrictions
rather than corrupting. Classic mode also cannot drift out of sync with the
pocket code, because it *is* the pocket code with three predicates relaxed.

### Accepted cost

Classic is not a perfect recreation of old BN inventory. A backpack technically
still has four pockets; the player never sees or feels them. This is judged the
right trade against maintaining a second container model.

## Save migration

Bump `savegame_version`. `item_contents::deserialize()` branches on it: legacy
saves present `contents` as a flat item array, current ones as an array of
pockets.

The legacy path reads the flat list into `std::vector<detached_ptr<item>>` —
using the constructor CBN already provides "to aid migration" — then places each
item via `best_pocket()`.

Items that fit nowhere under the new restrictions go to `pocket_type::MIGRATION`,
CDDA's existing unrestricted pocket intended for exactly this. Nothing hits the
floor unattended; nothing vanishes.

**Invariant: migration never destroys an item.**

Serialization format follows CDDA's, keeping saves conceptually aligned with
theirs.

## Testing

TDD throughout, using the Catch2 suite already building as `cata_test-tiles`.

- **`can_contain()` matrix** — volume, weight, length, rigidity, watertight,
  restriction lists. Table-driven. This is where CDDA's semantics either
  transferred or did not.
- **`best_pocket()` ranking** — priority ordering, whitelist and blacklist
  precedence, first-fit fallback.
- **Non-rigid volume propagation** — stuffing a pack grows its outer volume and
  therefore encumbrance. Easy to get wrong, very visible in play.
- **Synthesis pass** — a synthetic `itype` carrying only `storage` yields one
  CONTAINER pocket of the right volume; one with authored `pocket_data` is
  skipped.
- **Classic mode** — identical insertion scenarios under both modes, asserting
  classic accepts what full rejects on restriction grounds.
- **Save migration** — hand-written legacy JSON fixture; assert every item is
  present and nothing sits in MIGRATION that should not.
- **Regression** — the existing inventory and item tests are the real safety net
  for the 181 untouched call sites. They must stay green at every commit. This
  is the entire justification for porting behind the seam.

## Out of scope

- Converting CBN base-game item JSON wholesale to `pocket_data`
- Tracking CDDA `master` during implementation; pin the reference commit
- Any second container model or dual serializer
