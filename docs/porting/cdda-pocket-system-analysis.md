# What CDDA's pocket system has that CSE's does not

Written 2026-09-05, against the pinned CDDA reference (`5b915aea09`) in
`F:\Projects\CDDA`. Read-only comparison; nothing here has been ported yet.

CSE finished phase 1 of the pocket port: pockets exist, are authored in JSON,
route acquired items into worn pockets, and now display and nest to any depth.
The three findings the handoff still lists as open — **1 (inventory letters),
5 (rigid volume) and 6 (ancestor capacity)** — all have a worked answer
upstream. Two of them turn out to be *the same mechanism*.

## 1. `pocket_constraint` answers findings 5 and 6 together

`item_pocket.h` defines a `pocket_constraint` that carries a pocket's effective
limits *after* its ancestors are taken into account:

```cpp
void pocket_constraint::constrain_by( const item_pocket *outer )
{
    if( !in_rigid ) {
        volume_capacity = std::min( volume_capacity, outer->volume_capacity() );
        remaining_volume = std::min( remaining_volume, outer->remaining_volume() );
        ...
    }
    in_rigid |= outer->rigid();
    max_containable_length = std::min( max_containable_length,
                                       outer->max_containable_length() );
    ...
}
```

The whole design is in the `if( !in_rigid )`. An inner pocket is clamped by
every outer pocket **until a rigid one intervenes**; past that point outer
limits stop applying, because a rigid box does not bulge and so what is inside
it cannot press on what is outside. That is simultaneously:

- the answer to **finding 6** — a coin going into a wallet in your jeans is
  measured against the jeans as well, without anyone hand-walking the chain;
- the reason **finding 5** matters — `rigid` is not decoration, it is the thing
  that terminates the constraint walk.

`pocket_constraint` also tracks `is_dominated`: a pocket whose contents could
equally well go in its parent, used to stop the UI listing redundant spaces.

**Porting caveat, and it is the significant one.** The chain is built by
`item_location::get_pocket_constraints_recursive()`, and its consumers are
`inventory_ui.cpp` and `character_attire.cpp` — this is CDDA's *available
space* machinery, not demonstrably its insertion gate. CSE has no
`item_location`: it uses `location_ptr`/`detached_ptr`, and the equivalent
handle is `item::parent_item()`, which this session already used for the
inventory tree. So the type ports cleanly but its constructor does not; it
would have to be rebuilt on `parent_item()`.

## 2. Rigidity is decided per pocket, not per itype

This is the concrete defect behind finding 5, and the fix is four lines.

| | CDDA (`item_pocket::item_size_modifier`) | CSE (`item_contents::item_size_modifier`) |
|---|---|---|
| gate | `if( data->rigid ) return 0_ml;` per pocket | none — sums every pocket |
| where rigidity is read | the pocket | the **itype**, one level up in `item::volume()` (`if( !type->rigid )`) |
| extras | subtracts `magazine_well`, applies `volume_multiplier` | neither |

CSE's own comment admits it: *"Rigidity is still decided per item by
item::volume(); gating this on pocket_data::rigid belongs with the phase that
authors real pockets."* That phase has happened. The handoff's own numbers say
only 31 of 193 pocketed armor itypes declare `"rigid": false`, so making this
per-pocket moves the volume of most clothing in the game — which is why it
still wants its own session and a playtest, not a drive-by.

## 3. `remaining_weight()` exists upstream

`item_pocket::remaining_weight()` is `weight_capacity() - contains_weight()`.
CSE has `remaining_volume()` and no mass equivalent, which is one of the two
things that made finding 6 look bigger than it is. It is a one-line addition
with an obvious upstream name to match.

## 4. The invlet keystone: CDDA simply descends

Finding 1 is the blocker behind the reverted takeoff routing. The difference is
a single `VisitResponse`:

```cpp
// CDDA, Character::invlet_to_item
visit_items( [&]( item * it, item * ) {
    if( it->invlet == invlet ) { invlet_item = it; return VisitResponse::ABORT; }
    return VisitResponse::NEXT;      // descends into pockets
} );
```

CSE returns `VisitResponse::SKIP` there, with the comment *"Visit top-level
items only as UIs don't support nested items. Also, inventory restack logic
depends on this."* The first half of that comment is now stale — this session's
work means the UIs do support nested items.

The second half is the real question, and CDDA answers it too. Its
`inventory::restack` does:

```cpp
const item *invlet_item = p.invlet_to_item( topmost.invlet );
if( !inv_chars.valid( topmost.invlet ) ||
    ( invlet_item != nullptr && position_by_item( invlet_item ) != idx ) ) {
    assign_empty_invlet( topmost, p );
    ...
}
```

Because `invlet_to_item` descends, `invlet_item` may be a pocketed item, whose
`position_by_item` is `INT_MIN` — never equal to `idx` — so the flat-inventory
item yields and takes a fresh letter. A pocketed item may hold a letter and the
loose one gives way. That is coherent, and it is exactly the reconciliation CSE
lacks, which is why routing a t-shirt into your jeans currently leaves two
items claiming `a`.

## What CSE has that CDDA does not

Worth stating so none of this gets thrown away in a port:

- **Routing.** `i_add_to_worn_pockets()` / `i_add_routed()` / `pocket_destinations()`
  and the `POCKET_PICKUP` prompt are CSE's own. CDDA has `best_pocket()` but not
  the "worn pockets get first refusal, flat inventory backs them up" contract.
- **`POCKET_SYSTEM: classic`.** The mode that pools storage and nests nothing,
  keeping save data byte-identical between modes. No upstream equivalent.
- **The dry-run enforcement audit** (`record_pocket_audit_miss` /
  `pocket_audit_report`), which is how CSE intends to decide when insertion
  gating is safe to switch on.

## Suggested order, cheapest first

1. **`remaining_weight()`** — one line, no behaviour change, unblocks the rest.
2. **`invlet_to_item` → `NEXT`, plus CDDA's restack conflict rule.** Self-contained,
   and `invlet_test.cpp` already exists to judge it — the 116 failures the
   takeoff experiment hit are the acceptance criterion. Closes finding 1 and
   makes the reverted `takeoff-routing.patch` re-appliable.
3. **Per-pocket `rigid` in `item_size_modifier`.** Small diff, wide blast radius;
   needs a playtest, not a suite pass.
4. **`pocket_constraint`,** rebuilt on `parent_item()`. Only worth it once 3 is
   in, since rigidity is what makes the constraint walk terminate.

Steps 3 and 4 change what the player sees and carries; they are the owner's call
on timing, not a background task.
