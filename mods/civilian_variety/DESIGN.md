# Civilian Variety — Design Document

**Date:** 2026-08-18
**Status:** Implemented and in play.  Sections 1-11 are the original design rationale and remain accurate as *reasoning*; section 12 is the authoritative record of what exists.
**Target game:** Cataclysm: Bright Nights (`cataclysmbn/Cataclysm-BN`, verified against `main` @ `5432a46c40`)
**Mod id:** `civilian_variety`
**Location:** `mods/civilian_variety/` (third-party mod directory)

---

## 1. Purpose

Add a large roster of new civilian monster types to Bright Nights, purely for variety. The world
should feel like it was populated by *individual people* when the Cataclysm hit, rather than by a
handful of repeated archetypes.

The mod must work **standalone** and **alongside the bundled `civilians` mod**, with no hard
dependency on the latter in either direction.

### Success criteria

1. Loads cleanly with `civilians` enabled, and with it disabled.
2. Adds meaningful visual and narrative variety to the day 0–3 collapse window.
3. Does not measurably change total civilian density when `civilians` is also loaded.
4. Does not starve vanilla zombie spawns.
5. Ships playable with zero custom art, and accepts art later with no rework.

---

## 2. Key engine facts this design depends on

All verified against source at `main` @ `5432a46c40`. These are the load-bearing assumptions; if any
turns out wrong during implementation, stop and revisit this document.

### 2.1 There is no optional-dependency mechanism

`MOD_INFO` supports `dependencies` (hard) and `conflicts` only — see
`docs/en/mod/json/reference/mod_info.md`. There is no soft or optional dependency.

**Consequence — the central constraint of this design:** "works with or without `civilians`" cannot
be *declared*, only *achieved*. The mod must never reference an id owned by `civilians`. Every
monster, item group, monstergroup and gun it touches must be either vanilla `bn` or its own.

`dependencies` is therefore exactly `[ "bn" ]`.

### 2.2 Monstergroups auto-extend

`src/mongroup.cpp:385-390`:

```cpp
bool extending = false;  //If already a group with that name, add to it instead of overwriting it
bool allow_override = jo.get_bool( "override", false );
if( monsterGroupMap.contains( g.name ) && !allow_override ) {
    g = monsterGroupMap[g.name];
    extending = true;
}
```

Re-declaring an existing monstergroup **appends** to it. No `copy-from` or `extend` needed, and two
mods can both inject into `GROUP_ZOMBIE` without conflict.

`"override": true` would replace it instead. **This mod must never set `override`.**

### 2.3 Monster factions merge on redeclaration

`src/monfaction.cpp:310` resolves the faction via `get_or_add_faction`, returning the existing
faction if one of that name is already loaded and creating it otherwise. Attitude entries are then
added to the resulting faction's attitude map.

**Consequence:** this mod declares a `MONSTER_FACTION` named `civilians` itself, with the same
allegiances the bundled mod uses. With `civilians` loaded, the two declarations converge on one
faction and all civilians from both mods are mutually friendly and friendly to the player. Without
it, this mod creates the faction alone and behaves identically. This is the mechanism that delivers
the "with or without" requirement for faction behaviour.

### 2.4 Lua hooks stack per-mod

`src/catalua.cpp:376-405` — `add_hook` appends each registration to a list, tagging it with
`mod_id` and `priority`. Multiple mods hooking the same event all run. No coordination needed.

### 2.5 Loaded mods are visible from Lua

`src/catalua.cpp:315-317, 335` builds `game.active_mods` as a read-only array of every loaded mod id,
populated before any mapgen. Detecting the bundled mod is therefore trivial and reliable.

`civilians` keeps its `CONFIG` table as a Lua *local*, so its spawn rate is **not** readable. The
guard can detect presence only, and must assume that mod's shipped defaults.

### 2.6 Monsters inherit `looks_like` from `copy-from`

`src/monstergenerator.cpp:786-789`:

```cpp
if( was_loaded && jo.has_member( "copy-from" ) && looks_like.empty() ) {
    looks_like = jo.get_string( "copy-from" );
}
jo.read( "looks_like", looks_like );
```

A monster using `copy-from` without an explicit `looks_like` inherits the parent's. Setting
`looks_like` once on the shared base gives every archetype a sensible tileset sprite with no art.

### 2.7 `starts` / `ends` are hours; `freq` is per-mille of a fixed total

`src/mongroup.cpp:409` — `static const time_duration tdfactor = 1_hours;`, scaled by the
`MONSTER_UPGRADE_FACTOR` game option. `"ends": 72` is three days.

`src/mongroup.cpp:437` — `g.freq_total = jo.get_int( "freq_total", ( extending ? g.freq_total : 1000 ) );`

When extending, `freq_total` is **inherited, not enlarged**. Selection (`src/mongroup.cpp:104`,
`485-491`) rolls `rng(1, freq_total)` and walks entries subtracting each `freq`; any remainder falls
through to the group's `default` monster.

**Consequence — the spawn budget is zero-sum.** Measured state of `GROUP_ZOMBIE` in
`data/json/monstergroups/zombies.json`:

| | entries | summed freq | fallthrough to `mon_zombie` |
|---|---|---|---|
| Vanilla | 49 | 773 | 227 |
| + `civilians` | 55 | 841 | 159 |
| + this mod (budgeted) | 68 | ≤ 921 | ≥ 79 |

Every point of `freq` added to `GROUP_ZOMBIE` takes probability mass directly from vanilla zombies.
If the sum ever reached 1000, trailing entries would become unreachable and fallthrough would vanish.

**Hard constraint:** this mod's total added `freq` in `GROUP_ZOMBIE` must not exceed **80**. With the
13 Channel 1 entries of 5.1 and 5.3, that is roughly 6 `freq` each.

Note that `pack_size` multiplies the number of *creatures* a winning roll produces, without costing
any additional `freq`. The group packs of 5.3 therefore add visible bodies to the world at no budget
cost, and their `freq` should be set lower than the single-spawn types to compensate.

---

## 3. Architecture

### 3.1 Two spawn channels, deliberately split

The zero-sum budget in 2.7 makes it wrong to push forty archetypes through `GROUP_ZOMBIE`. The
design therefore splits the roster across two channels by where each archetype belongs:

**Channel 1 — `GROUP_ZOMBIE` injection (JSON only).** Street-level types: behavioural archetypes and
group packs. These are what the player meets outdoors during the collapse. Costs zombie budget, so
kept to a small, curated subset within the 80-point cap, all windowed with `starts`/`ends` to the
first three days.

**Channel 2 — Lua spawner (own monstergroups).** Indoor and contextual types: occupational, grim,
novelty. Spawned by our own `on_mapgen_postprocess` hook via `map:place_spawns()` against
monstergroups this mod owns. Because those groups have their own `freq_total`, weights inside them
are **free** — they do not compete with zombies at all.

Channel 2 has **two placement modes**, and picking the wrong one is why the first construction-site
build appeared to spawn nothing:

- *Interior* (default): roll `SPAWN_CHANCE` per qualifying piece of `TARGET_FURNITURE`. Correct for
  houses and shops, where people sit on sofas and beds and the furniture count is high.
- *Open site* (`LOCATION_GROUPS` entry carries a `count`): spawn that many anywhere walkable and
  skip the furniture scan. Correct for work sites and other outdoor locations.

Furniture-gating is a **house heuristic** and does not transfer. A `construction_site` contains one
or two chairs on the whole lot — at a 15% roll that is ~0.2 expected civilians per site, so most
visits produce none. Its characteristic furniture is crates, tables, dumpsters and barricades, none
of which anchor a standing worker anyway. Every new archetype must be assigned a placement mode
deliberately; inheriting the interior default is a decision, not a default.

Related trap when testing: `on_mapgen_postprocess` fires only when a chunk is **first generated**.
Anywhere already visited is baked and will never pick up spawner changes. Test on fresh chunks or a
fresh world.

This split is why the roster can be large without unbalancing anything.

### 3.2 The `civilians`-loaded guard

The Lua spawner checks once, at load:

```lua
local function is_mod_loaded(id)
  for _, m in ipairs(game.active_mods) do
    if m == id then return true end
  end
  return false
end

local CIVILIANS_PRESENT = is_mod_loaded("civilians")
```

When `civilians` is present, this mod **halves its own furniture spawn chance** (15 → 7). The
bundled mod scans the same furniture list at 15%, so the combined indoor density lands close to what
either mod produces alone. When absent, this mod runs at its full 15% and carries indoor spawning by
itself.

This satisfies success criterion 3 in both configurations. It is approximate, not exact — the two
mods roll independently and their furniture lists differ slightly — but it prevents the doubling
that motivated the guard.

### 3.3 One base template, many thin archetypes

A single locally-owned `mon_cv_base` carries the shared human statline (bodytype, species, material,
volume, weight, harvest, path settings, flags, `zombify_into`, faction). Every archetype is
`copy-from: mon_cv_base` plus only its deltas — name, description, `death_drops`, `speech`, and any
stat or flag that actually distinguishes it.

This is the technique the bundled mod uses to fit 16 monsters into 1151 lines, and it is what makes a
40-type roster tractable rather than a wall of duplication. It also gives every archetype an
automatic tileset sprite via 2.6.

`mon_cv_base` is never spawned directly.

---

## 4. File layout (original plan - actual in section 12)

```
mods/civilian_variety/
  DESIGN.md                 this document
  modinfo.json              MOD_INFO, dependencies [ "bn" ]
  faction.json              MONSTER_FACTION "civilians" (merge-safe redeclaration)
  monsters_base.json        mon_cv_base abstract template
  monsters_generic.json     mon_cv_bystander, the common generic civilian
  monsters_behavioural.json behavioural archetypes
  monsters_occupational.json occupational archetypes
  monsters_group.json       pack archetypes
  monsters_grim.json        atmospheric archetypes
  monsters_novelty.json     rare / out-there archetypes
  monsters_survivor.json    survivor upgrade tier
  monstergroups.json        GROUP_ZOMBIE injection + own spawn/upgrade groups
  itemgroups.json           per-archetype loadouts and death drops
  speech.json               per-archetype barks
  preload.lua               hook registration
  main.lua                  config table + guarded spawner
  mod_tileset.json          sprite index map
  civilian_variety_normal.png  512x512, 16x16 grid of 32x32 cells
```

**Sheet occupancy as of 2026-08-19** (measured, not eyeballed): indices 1–31 generic civilians,
32 mechanic, 33–35 construction workers, 36–40 sex workers. Index 0 holds a 1px stray and indices
102–103 hold small stray marks; all three should be erased. Index math is
`index = row * 16 + col`, per `src/cata_tiles.cpp:1135-1136`.

Load order note: BN loads all files in the mod folder breadth-first, then lexically
(`docs/en/mod/json/explanation/loading_order.md`). `monsters_base.json` sorts before every other
`monsters_*.json`, so the template is defined before its children — but `copy-from` is resolved
during finalization rather than at parse time, so this is tidiness, not a requirement.

---

## 5. Roster (original plan - superseded by section 12)

Approximately 40 archetypes. Counts per category are targets, not quotas — the implementation may
merge or drop individual entries that turn out not to earn their place.

### 5.1 Behavioural — Channel 1 (`GROUP_ZOMBIE`)

Reactions to the apocalypse. Distinguished by aggression, morale, speed, and speech rather than gear.

| Archetype | Distinguishing behaviour |
|---|---|
| Looter | Aggressive, carries stolen goods |
| Hoarder | Overloaded, slow, drops a mess |
| Denier | Calm, normal speed, insists it is a hoax |
| Doomsayer | Very loud speech; attracts zombies |
| Good samaritan | Moves toward the wounded |
| Cowering survivor | Near-static, hides |
| Bolter | Fast, pure flight |
| Shell-shocked | Slow, unresponsive, minimal reaction |

### 5.2 Occupational — Channel 2 (Lua, furniture)

Defined by the job they died doing. Loadout and death drops carry the theme.

Nurse, paramedic, doctor, mechanic, line cook, delivery driver, construction worker, store clerk,
teacher, office worker, janitor, security guard, firefighter, bus driver, barista, sex worker.

Security guard is deliberately distinct from the bundled mod's police officer: baton and flashlight,
no firearm, lower `diff`.

### 5.3 Group-based — Channel 1 (`GROUP_ZOMBIE`, `pack_size`)

Small packs with an implied relationship, using `pack_size` and paired types.

Family unit, carload of evacuees, looting crew, church group, office evacuation.

### 5.4 Grim / atmospheric — Channel 2 (Lua, furniture)

Near-zero combat value, high atmosphere. Mostly stationary.

| Archetype | Note |
|---|---|
| The bitten | Short upgrade half-life; turns fast |
| Mourner | Static, beside a corpse |
| Would-be suicide | Static |
| Bedridden patient | Fills the niche the bundled mod's orphaned `mon_civilian_icu` gestures at, without referencing it |
| Trapped person | Static, screaming, draws zombies |

### 5.5 Novelty — Channel 2 (Lua, low weight)

Rare enough to stay surprising. Weighted well below the other Channel 2 types.

Clown, otaku/cosplayer, sports mascot, stripper, adult actress, cyclist in full kit, wedding party,
LARPer, marathon runner, influencer filming.

**Tone:** adult-themed archetypes are written the same way as any other occupation — the job stated
plainly, loadout and drops reflecting it, description covering who they were and how the Cataclysm
caught them. Descriptions stay non-pornographic, matching the dry register the base game uses for its
own adult-themed spawns. This is a register decision, not a content restriction.

### 5.6 Survivor tier

Three archetypes earn a survivor upgrade via `"upgrades": { "into": ..., "half_life": 35 }`,
mirroring the bundled mod's month-scale progression:

| Base | Becomes |
|---|---|
| Security guard | Survivor sentry |
| Mechanic | Survivor scrapper |
| Nurse | Survivor medic |

These get armour, `regen_morale`, a real weapon and a proper death-drop group. They are the only
types in this mod that persist meaningfully past the first week.

Survivors appear in **no spawn group on either channel**. They are reachable only by upgrade from
their base archetype, which means the player meets one exactly when the world is old enough to have
produced it. This also keeps them off the `GROUP_ZOMBIE` budget entirely.

---

## 6. Item and drop strategy

Every archetype gets a `death_drops` item group carrying its theme. Shared filler comes from one
local `cv_common_civilian_items` group.

**Hard rule:** every item id referenced must be verified to exist in `data/json/items/` before use.
No invented ids. Where a themed item does not exist in vanilla, substitute the nearest real one
rather than defining a new item — this mod adds creatures, not items.

Monster weapons follow the bundled mod's pattern where needed: a `monster_weapon` item group, and for
any ranged attacker a fake `GUN` defined via `copy-from` of a real gun with worsened dispersion. The
survivor tier is the only place this should be necessary.

---

## 7. Lua design

### `preload.lua`

Registers one hook — `on_mapgen_postprocess` — delegating to `main.lua`. No every-turn hook; corpse
pulping is the bundled mod's concern and duplicating it would double the work done per turn for no
gain.

### `main.lua`

Structure mirrors the bundled mod so the two are legible side by side:

- `merge_config(default, stored)` over `game.mod_storage`, so users can tune per-save.
- A `CONFIG` table with `SPAWN_CHANCE`, `TRY_TRIES`, `TARGET_FURNITURE`, `EXCLUDED_TERRAINS`,
  `VANISH_PERIOD_DAYS` and `VANISH_BASE_RATE`. Category-weighting knobs (`RARE_CHANCE`,
  `NOVELTY_CHANCE`) arrive with the archetypes that need them.
- **Decay is live, unlike the bundled mod's.** `VANISH_BASE_RATE` defaults to `0.5` over a
  14-day period, so living civilians are at 100% on day 0, 50% on day 14, 25% on day 28. This
  matters more here than on Channel 1: the `GROUP_ZOMBIE` entry is bounded by `ends: 72`, but the
  Lua spawner has no built-in time limit, so without decay you would still be finding untouched
  living civilians indoors on day 300. Set the rate to `1.0` to disable.
- `CIVILIANS_PRESENT` detection per 3.2, halving `SPAWN_CHANCE` when true.
- `on_mapgen_postprocess`: scan furniture, roll `SPAWN_CHANCE`, pick a spawn group by weight
  (occupational / grim / novelty), find a free adjacent tile, `map:place_spawns()`.

`EXCLUDED_TERRAINS` reuses the same reasoning as the bundled mod's `NPC_TERRAINS` — refugee centres,
labs, bunkers, prisons and similar should not have wild civilians appearing inside them. The list is
this mod's own copy, not a reference to theirs.

---

## 8. Sprites

The mod ships with **no art** and is fully playable that way: per 2.6, every archetype inherits
`looks_like` from `mon_cv_base`, which points at a vanilla human-shaped monster. Tileset users see a
sensible stand-in; ASCII users are unaffected.

Art is added later with no rework, because `mod_tileset.json` is standalone — nothing in the monster,
group or drop definitions references it.

**Agreed approach — sprite contract first.** `mod_tileset.json` is written *before* the art exists,
fixing sheet filenames, tile size, and the monster-to-sprite-index mapping. Art is then drawn to that
spec and drops straight in. Format, per the bundled mod's file:

```json
{ "type": "mod_tileset",
  "compatibility": [ "UNDEAD_PEOPLE_BASE", "UNDEAD_PEOPLE", "MshockRealXotto", "MSX++DEAD_PEOPLE" ],
  "tiles-new": [ {
      "file": "gfx/occupational.png",
      "sprite_width": 32, "sprite_height": 32,
      "sprite_offset_x": 0, "sprite_offset_y": 0,
      "tiles": [ { "id": "mon_cv_nurse",
                   "fg": [ { "weight": 3, "sprite": 0 }, { "weight": 3, "sprite": 1 } ],
                   "rotates": true } ]
  } ] }
```

Sprites are indexed row-major from 0 within each sheet. Multiple weighted `fg` entries per monster
give random visual variation between individuals of the same type.

---

## 9. Validation

1. **JSON formatting:** `just fmt-json mods/civilian_variety/*.json` — the recipe takes explicit
   file arguments, so it works outside `data/`.
2. **Lua formatting:** `dprint.json`'s include list covers `data/mods/**/*.lua` but **not**
   `mods/**/*.lua`, so `just fmt-lua` will skip this mod. Run stylua directly on the two Lua files.
3. **Load test, both configurations:** create a world with the mod alone, and a second world with the
   mod plus `civilians`. JSON errors surface at world creation with the offending file path.
4. **Spawn sanity:** debug-spawn each archetype to confirm it resolves, has a sprite or a glyph, and
   drops something sane on death.
5. **Budget check:** confirm summed `GROUP_ZOMBIE` additions stay within the 80-point cap of 2.7.

### Running the mod

`mods/` at the repo root is scanned only when the game's user directory resolves there. Per
`src/path_info.cpp:327`, `user_moddir()` is `<userdir>/mods/`, and on Windows `<userdir>` defaults to
`%LOCALAPPDATA%\cataclysm-bn\`. Launch with `--userdir .` from the repo root, or symlink the mod into
the default user mod directory.

---

## 10. Build phases (all complete - see section 12)

Ordered so there is something loadable and judgeable early, rather than forty archetypes arriving at
once.

| Phase | Contents | Why here |
|---|---|---|
| 1 | `modinfo.json`, `faction.json`, `mon_cv_base`, monstergroups skeleton, 3–4 behavioural types | Smallest thing that loads and spawns. Proves the faction merge and the budget maths in both configurations. |
| 2 | Remaining behavioural + group packs | Completes Channel 1. |
| 3 | Lua spawner with the `civilians` guard + occupational types | Proves Channel 2 and the density guard. |
| 4 | Grim + novelty types | Bulk content, no new mechanisms. |
| 5 | Survivor tier | Only phase needing fake guns and armour balance. |
| 6 | `mod_tileset.json` sprite contract | Written before art; art added on your schedule. |

Balance passes on spawn weights and drop tables happen at the end of phases 2, 3 and 5 — the
weights matter more to how this plays than the definitions do.

---

## 11. Explicit non-goals

Two of these were deliberately abandoned later; they are kept as written because
the reasoning still holds for everything else.

- No changes to any bundled mod, including `civilians`. **Still true.**
- ~~No new items, terrain, furniture or professions. Creatures only.~~
  **Superseded.** The mod now owns 27 items — 4 keepsakes, the 13-piece otaku
  collection, 4 deliberately-poor guns and 6 findable dead bodies. Still no
  terrain, furniture or professions.
- No corpse-pulping or other every-turn behaviour — that is the bundled mod's job.
  **Still true**; the two periodic hooks run on 300- and 600-turn intervals.
- ~~No real NPCs. These are monsters, so there is no dialogue, trade or recruitment.~~
  **Partly superseded.** Still no real NPCs and no trade or recruitment, but the
  examine menu in `main.lua` gives most archetypes flavour dialogue and some a
  one-shot favour.
- No `override: true` on any monstergroup, ever. **Still true.**

---

## 12. Current state

Authoritative as of 2026-08-25.  **47 monsters, 36 owned monstergroups, 3 factions, 30 items.**

### Files

```
modinfo.json               MOD_INFO, dependencies [ "bn" ], lua_api_version 2
faction.json               MONSTER_FACTION civilians (merge-safe), cv_bandit, cv_shopkeeper
effects.json               effect_cv_helped - the one-shot favour marker
morale_types.json          morale_cv_blessed - own type so blessings do not merge
monsters_base.json         mon_cv_base        - abstract, friendly civilian statline
monsters_generic.json      mon_cv_bystander, _panicked, mon_cv_elderly, _elderly_f
monsters_occupational.json 26 archetypes      - professions, police, clergy, retail, security
monsters_zombie.json       zombie clown, nerd, rot, priest, nun, retail, mall cop,
                           doctor, butler, maid
monsters_bandit.json       mon_cv_bandit_base - abstract, hostile statline + 5 bikers
monster_attacks.json       cv_syringe_inject  - the zombie doctor's poison jab
bandit_guns.json           4 GUN items        - deliberately poor copy-from weapons
items_personal.json        4 keepsakes, descriptions drawn from snippets
items_clothing.json        3 wearables vanilla lacks - 2 uniform shirts, medical scrubs
items_corpses.json         6 findable dead bodies - CONTAINERs, so they can be looted
snippets_corpses.json      36 snippet texts, one category per body
snippets_personal.json     43 snippet texts for the above
items_otaku.json           13 books/collectibles + 2 item_action declarations
uncraft_otaku.json         13 disassembly recipes
itemgroups.json            loadouts, death drops, favour pools, vanilla-group injections
monstergroups.json         GROUP_ZOMBIE + GROUP_MANSION injections, 36 GROUP_CV_* pools
speech.json                ambient barks, keyed per monster
mod_tileset.json           4 sheet blocks (see Sprites below)
main.lua / preload.lua     spawner, ambulance sweep, ambient dread, interactions, iuse
tools/check_sprites.py     sprite-index validator (13.2)
DESIGN.md                  this document
```

### Roster

| Group | Monsters |
|---|---|
| Generic | `bystander`, `bystander_panicked`, `bystander_fat`, `bystander_fat_f`, `elderly`, `elderly_f` |
| Trades | `construction_worker`, `mechanic`, `farmer`, `farmer_f` |
| Emergency | `firefighter`, `doctor`, `doctor_f`, `nurse` |
| White collar | `businessman`, `businesswoman`, `office_worker`, `office_worker_f` |
| Other | `geek`, `sex_worker`, `clown`, `priest`, `nun`, `shopkeeper`, `retail`, `retail_f` |
| Household | `butler`, `maid` |
| Police (friendly) | `police`, `swat`, `cop_pistol`, `mallcop` |
| **Zombies** | `zombie_clown`, `zombie_nerd`, `zombie_rot`, `zombie_priest`, `zombie_nun`, `zombie_retail`, `zombie_mallcop`, `zombie_doctor`, `zombie_butler`, `zombie_maid` |
| **Bikers (hostile)** | `biker_knife`, `biker_pistol`, `biker_pistol_f`, `biker_rifle`, `biker_shotgun` |

### Speed, against the player's base of 100 (`src/creature.cpp:188`)

Nothing outruns the player.  `firefighter` alone matches at 100; SWAT, police,
farmers, cop and knife biker sit at 95; bikers and the panicked bystander at 92;
the ordinary roster at 90; `geek` 85; `bystander_fat` 75.  `zombie_clown` 80 and
`zombie_nerd` 62 bracket the vanilla zombie's 70 in either direction.  The
`elderly` pair are the slowest living archetype at 62 - a deliberate floor, since
at that speed fleeing does not work and never will.

Their frailty is otherwise carried by hp 48 (bystander 75), dodge 0, vision_day 8
and a 1d1 bash-2 melee attack.  **Armor could not go lower:** `mon_cv_base` sets
no armor fields at all, so every value arrives at finalisation as -1 and is
clamped to 0 (`src/monstergenerator.cpp:426-440`).  Every civilian in this mod
already takes damage unmitigated; negative armor is not honoured.

### Turning

Eleven archetypes turn into something specific rather than a generic zombie, each
wired through **both** routes - `zombify_into` on death and an upgrade group over
time.  Setting only the first leaves the inherited bystander upgrade path intact
and the costume is lost.  See 13.4 for why it must be a group.

| Living | Becomes |
|---|---|
| `clown` | `zombie_clown` via `GROUP_CV_CLOWN_UPGRADE` |
| `geek` | `zombie_nerd` via `GROUP_CV_NERD_UPGRADE` |
| `firefighter` | `mon_zombie_fireman` via `GROUP_CV_FIREFIGHTER_UPGRADE` |
| `priest` | `zombie_priest` via `GROUP_CV_PRIEST_UPGRADE` |
| `retail` / `_f` | `zombie_retail` via `GROUP_CV_RETAIL_UPGRADE` |
| `mallcop` | `zombie_mallcop` via `GROUP_CV_MALLCOP_UPGRADE` |
| `butler` | `zombie_butler` via `GROUP_CV_BUTLER_UPGRADE` |
| `maid` | `zombie_maid` via `GROUP_CV_MAID_UPGRADE` |
| `nun` | `zombie_nun` via `GROUP_CV_NUN_UPGRADE` |
| `bystander_fat` | `GROUP_CV_FAT_UPGRADE` |
| `elderly` / `_f` | `mon_cv_zombie_rot` 97%, `mon_zombie_acidic` 3%, via `GROUP_CV_ELDERLY_UPGRADE` |

`mon_zombie_fireman` is vanilla and carries the turnout gear as armor - bash 6,
cut 10, bullet 14, fire 10 - so what made the living firefighter hard to kill is
still there afterwards. It defines no `upgrades` of its own, so it is a terminal
form: a turned firefighter stays a firefighter zombie for good.

**The elderly are the one asymmetric case.** `zombify_into` is a single
`mtype_id` (`src/mtype.h:416`) with no group form, so the corpse-revives path can
only ever name one species: an elderly you killed and left always comes back
decayed. The 3% acidic outcome rides on the timed upgrade, which *does* take a
weighted group. Worth remembering before wiring a chance into any other death
route - it will silently do nothing.

**The doctor is the deliberate counter-example.** Doctors turn into
`mon_cv_zombie_doctor` only **8% of the time**, through
`GROUP_CV_DOCTOR_UPGRADE`; the other 92% is `GROUP_CV_BYSTANDER_UPGRADE`'s
ordinary spread, rescaled. So `zombify_into` is left exactly as inherited
(`mon_zombie`) rather than pointed at the zombie doctor - doing that would make
every doctor corpse revive as one, which is the whole point of the asymmetry
above. A partial chance can only ever ride the timed upgrade.
`mon_cv_doctor_f` picks all of this up through `copy-from`.

### The devourer chain

The elderly turn into **`mon_cv_zombie_rot`**, the mod's own decayed zombie, not
vanilla's. It is deliberately identical in name, description and stats, and
declares no sprite so `looks_like` is inherited from `copy-from` - the player
cannot tell the two apart.

It exists for one reason. Vanilla `mon_zombie_rot` has
`upgrades: { half_life: 23, into: mon_devourer }` - a bare `into`, so *every*
decayed zombie in the game becomes a devourer sooner or later. Editing that
would change vanilla for every decayed zombie, which this mod does not do. So the
mod-owned copy rolls a weighted group instead:

| `GROUP_CV_ROT_UPGRADE` | freq | |
|---|---|---|
| `mon_cv_zombie_rot` | 750 | stays decayed, rolls again next interval |
| `mon_devourer` | 250 | diff 8, hp 112, speed 60 |

`half_life` 21 days, so roughly **25% by three weeks, 44% by six, 58% by nine**.
Listing the monster inside its own upgrade group is safe: `monster::try_upgrade`
(`src/monster.cpp:624-666`) loops while `upgrade_time <= current_day` and adds
`next_upgrade_time()` after each `poly`, so a self-upgrade advances the clock and
the loop terminates. It cannot spin.

`mon_zombie_rot` was the right base because it is the frailest classic zombie
(hp 55, speed 70), so the archetype's point survives its own death.
`mon_zombie_acidic` is diff 5 with a ranged acid attack, which is why its share
is 3% and not more.

### Spawn channels

**Channel 1 - `GROUP_ZOMBIE` injection**, zero-sum against vanilla zombies (2.7).
Currently **77 of the self-imposed 80 cap**:

| Monster | freq | window |
|---|---|---|
| `bystander` | 30 | ends 72 |
| `bystander_fat` / `_f` | 6 + 6 | ends 72 |
| `swat` | 6 | ends 72 |
| `cop_pistol` | 8 | ends 72 |
| `zombie_clown` | 4 | **none** |
| `zombie_nerd` | 4 | **none** |
| `zombie_priest` | 3 | **none** |
| `zombie_nun` | 3 | **none** |
| `zombie_retail` | 4 | **none** |
| `zombie_mallcop` | 3 | **none** |

The mod's own zombies deliberately carry no `ends` window.  The living injections are
bounded to the first 72 hours because civilians thin out; a turned clown is not a
collapse-window phenomenon and should keep turning up.

**Channel 2 - Lua spawner**, where weights inside our own groups cost nothing.
Interior mode rolls `SPAWN_CHANCE` per qualifying piece of furniture; open-site
mode carries a `count` and spawns anywhere walkable.  The fragment traps recorded
in the original build still apply: `bar` needs prefix matching or it catches
`barn_aban1`, `farm_` needs the trailing underscore or it catches `solar_farm`,
there is deliberately no `silo` entry, and a bare `office` would catch
`post_office`.  Roof layers are skipped wholesale by a `_roof` guard before the
table is consulted.

**Quiet venues, for the elderly.** These sit *above* the geek haunts in the
table, because `location_entry_for` returns the first entry that matches:

| match | mode | pool |
|---|---|---|
| `megastore`, `s_grocery` | interior | `GROUP_CV_BIGSTORE` - as retail, plus a mall cop |
| 9 shop fragments (below) | interior | `GROUP_CV_RETAIL` - customers 74%, staff 19%, already-turned 7% |
| `pawn`, `s_jewelry`, `s_antique`, `s_gun`, `s_hunting`, `s_liquor` | interior | `GROUP_CV_SHOP` - store owner 22%, customers 78% |
| `s_library` | interior | `GROUP_CV_LIBRARY` |
| `church` | interior | `GROUP_CV_CHURCH` - living clergy 30%, elderly 32%, bystander 22%, **zombie clergy 17%** |
| `park` (PREFIX) | count 1-3 | `GROUP_CV_ELDERLY` |
| `cemetery`, `Cemetery` | count 1-2 | `GROUP_CV_ELDERLY` |
| `communitygarden` | count 1-2 | `GROUP_CV_ELDERLY` |

Three things here were established by running the matcher over all 5,402
`overmap_terrain` ids rather than by reading it:

- **`park` must be PREFIX.** `is_ot_prefix` (`src/overmap.cpp:752-771`) accepts a
  full match or a partial one whose next character is `_`, so it claims `park`
  alone. A CONTAINS match would also have taken every `parking_*`,
  `parking_garage_*`, `trailerparksmall*` and `luna_park_*`.
- **Cemetery is listed twice** because the two mapgen families disagree on case -
  `cemetery_small` and `cemetery_4square_*` against `Cemetery_1a` / `_1b` - and
  CONTAINS is a plain `strstr`.
- **`s_library` is claimed before `library`,** so the public library is
  elderly-and-geek and `house_library`, a private study, stays purely geek.

**Geek haunts** are `house_library`, `s_bookstore`, `s_electronics`, `s_games`,
`s_arcade`, `museum`, and now `lancenter` ("LAN center") and `cs_internet_cafe`
("internet cafe") - the last two small, four and three overmap tiles, and both
carrying chairs the furniture scan can anchor to.  They reuse `GROUP_CV_GEEK`
rather than getting a pool of their own; a near-identical second group would only
drift out of step with this one.

Two large venues were surveyed and **deliberately left out**.  The spawner runs
per overmap tile, so a multi-tile building multiplies whatever it is given: the
convention center is 5x3 across three z-levels (45 tiles) and its palette is full
of `f_chair_folding`, and the movie theater is 20 tiles.  At `SPAWN_CHANCE` 15%
per qualifying furniture, either would flood.  Open-site `count` mode is worse,
not better - it spawns its count on *every* tile.  Any big structure needs a
throttle this system does not currently have.

### Favours

Each is one-shot, marked by `effect_cv_helped` on the monster.  The blessing is
the only one that **cannot fail** - no chance roll, nothing to run out of - and
the only one whose payout depends on the player rather than on luck:

| Archetype | Favour |
|---|---|
| `priest` / `nun` | a blessing: morale 10 (cap 20), or **25 (cap 40) with `SPIRITUAL`** |
| `shopkeeper` | protection money - 2 `money_bundle` to zero his anger. The only favour that *undoes* something |
| `doctor` | field treatment, an interruptible timed activity |
| `nurse` | medical supplies, 50% |
| `firefighter` / `farmer` | water / food |
| `mechanic` | a spare tool, 50% |
| `sex_worker` | paid company, 20% refusal |

`SPIRITUAL` costs a point at character creation and otherwise only pays out for
holy texts, so the blessing gives it something to do in the world.  The bonus
uses `morale_cv_blessed`, a type this mod declares, **not**
`morale_feeling_good`: `add_morale` merges entries of the same type, so sharing
one with the doctor and the sex worker would have a blessing silently overwrite a
treatment bonus.

The trait check is `you:has_trait(MutationBranchId.new("SPIRITUAL"))`.
`trait_id` is `string_id<mutation_branch>`, which luna exposes as
`MutationBranchId` (`src/catalua_luna_doc.h:265`); `has_trait` lives on
`Creature` (`catalua_bindings_creature.cpp:320`) and `add_morale` on
`Character` (`:1232`), both reachable from the avatar because it declares
`luna::bases<player, Character, Creature>()` (`:1572`).

### Retail workers

Two living ids sharing one archetype - `retail` (sprites 160-163) and `retail_f`
(164-167) - plus `zombie_retail` (168-171). All twelve sprites wear the same
green vest, which is what the loadout is built around.

They occupy **46 previously unclaimed overmap tiles** across eleven shop
fragments: `s_grocery`, `s_clothes`, `s_hardware`, `dollarstore`, `megastore`,
`s_butcher`, `s_thrift`, `s_sports`, `s_petstore`, `s_gardening`, `s_bike_shop`.
Food venues (`s_restaurant*`, `s_teashop`) are deliberately left out - waiting
tables is its own archetype.

`megastore` is 19 tiles, which would normally be a flooding risk. It is safe
here because `TARGET_FURNITURE` is only chairs, beds, sofas, stools, lockers and
wardrobes, and a shop floor has very few of those, so interior mode stays sparse.

`GROUP_CV_RETAIL` is **customer-heavy, because shops are**: 74% customers, 19%
staff, 7% already turned. Two or three people on shift and rather more people in
the aisles. A shop containing mostly employees reads as a diorama.

**The loadout is the uniform.** BN has no cotton apron, no box cutter, no name
tag and no clipboard - all checked - so `vest` at 85% carries the look, and a
`knife_folding` at 40% stands in for the blade every stockroom actually has. It
is also the most useful thing an early player can take off one. The till shows
up as `coin_quarter` rather than notes.

### Mall cops, and the TAZER attack

Sprites 172-175, one id, mixed gender, so the text avoids pronouns. They are
friendly civilians with a genuinely feeble weapon, and the answer to "can a
monster have a tazer" is **yes, with no new code**: `TAZER` is hardcoded and
registered for JSON use at `src/monstergenerator.cpp:635`.

What it actually does (`src/monattack.cpp:3616-3662`) is worth knowing before
balancing anything around it:

- **adjacent only** - it is a melee-range attack despite sounding ranged
- costs the attacker **200 moves**, which is most of its cost
- `rng(1, 5)` `DT_ELECTRIC` to a random body part, after a dodge check
- **it does not stun** in BN, unlike the CDDA version
- it returns false while the monster is disarmed

That last point is why the tazer is also their `monster_weapon`: disarming one
genuinely stops it shocking, rather than being cosmetic.

**The zombie mall cop's tazer is fused to its arm, and that is enforced rather
than described.** `mattack::tazer` only honours the disarm check when
`monster_weapon` is set:

```cpp
if( z->type->monster_weapon && z->has_effect( effect_monster_disarmed ) )
    return false;                                    // src/monattack.cpp:3618
```

The living mall cop sets `monster_weapon` and can therefore be disarmed. The
zombie deliberately does not, so nothing can ever take the tazer off it. The same
fact means the tazer cannot drop from it either - the weapon reaches the living
archetype through `monster_weapon` rather than through a drop table, so reusing
`cv_mallcop_worn` / `cv_mallcop_carried` yields the same inventory minus the
weapon, with no special-casing. Clothing is rolled at `damage: [2, 4]`, heavier
than the zombie retail worker's `[1, 4]`.

The living ones are **friendly**, so in practice their tazer gets used on zombies
rather than on the player. Making them territorial like the store owner would have them turn
on you inside the shop - not asked for, and a separate decision.

`megastore` and `s_grocery` were split out of `GROUP_CV_RETAIL` into
`GROUP_CV_BIGSTORE` so security appears in the two large formats and not in bike
shops and garden centres. Those entries sit **above** the retail block, because
`location_entry_for` returns the first match.

### The zombie doctor, and poison

**Poison is not a damage type.** `src/damage.h:20-33` has no `DT_POISON`; the
nearest, `DT_BIOLOGICAL`, is commented "internal damage, like from smoke or
poison" but carries no effect of its own. Poison is an `effect_type` applied on a
landed hit, which `melee_actor` does through an `effects` array - loaded at
`src/mattack_actors.cpp:373-377`, applied at `:459-467`:

```cpp
for( const auto &eff : effects ) {
    if( x_in_y( eff.chance, 100 ) ) {
        const bodypart_str_id &affected_bp = eff.affect_hit_bp ? bp : convert_bp( eff.bp );
        target.add_effect( eff.id, time_duration::from_turns( eff.duration ), affected_bp );
```

So the syringe is a mod-owned `monster_attack` in `monster_attacks.json`:

| `cv_syringe_inject` | | |
|---|---|---|
| `cooldown` | 20 | a pair of them cannot chain it |
| `move_cost` | 200 | at speed 72, most of three turns spent not attacking |
| damage | stab 4, `armor_multiplier` 0.5 | a needle finds a way past clothing, not past a riot suit |
| `effects` | `poison`, 60 turns, `chance` 60 | four jabs in ten land nothing but the stab |

Vanilla's `inject` was not reused. It runs on **cooldown 1** and stacks `bleed` on
top of poison (`data/json/monster_attacks.json:84-105`), which on anything that
can appear in twos is precisely the hassle this was meant to avoid; referencing it
by id would also inherit any future upstream retune.

`poison` alone, never `badpoison`. Badpoison adds `speed_mod: -10` on top of
roughly doubled stat penalties, and being slowed while surrounded is how a fight
stops being winnable. Plain poison is stat penalties, pain and the occasional
point of damage - survivable, and cured by any antitoxin. `bp` is `torso` rather
than `affect_hit_bp`, because poison is systemic.

The monster itself is weaker than what it copies: hp 76 against `mon_zombie`'s 80,
speed 72 against its 70. It is rare by placement rather than by statline -
**34 per mille in `GROUP_CV_HOSPITAL`** against the living doctors' 184 each, 8 in
`GROUP_CV_INDOOR_SPECIAL`, and **deliberately nothing in `GROUP_ZOMBIE`**, so the
80-point budget is untouched. Drops are the living doctors' wardrobe rolled at
`damage: [1, 4]` through a 50/50 distribution over `cv_doctor_worn_m` and `_f`,
plus the syringe at 30%.

### The store owner, and territorial behaviour

**He has his own faction, `cv_shopkeeper`.** He is territorial rather than
hostile, so the player usually ends up killing him in self-defence - and while he
sat in `civilians`, that turned every other bystander in sight against them.

No attitude tweak could have fixed that. `FRIEND_ATTACKED` propagates by exact
faction **identity**, not attitude:

```cpp
if( critter.faction != this->faction ) { continue; }    // src/monster.cpp:4222
```

Nothing else about him changes, because `resolve_attitude_map` walks the
*target's* `base_faction` chain (`src/monfaction.cpp:136-151`): with
`base_faction: human`, every "hates humans" relation still reaches him, so
zombies still attack him. `player` stays **friendly**, which is load-bearing -
see the threshold below. `civilians` also names him `neutral` explicitly, so the
relation is declared in both directions rather than left to inheritance, and
`main.lua` queries both faction ids for its civilian-proximity checks so he still
registers as a nearby civilian for ambient flavour.

A third attitude the mod did not have: neutral in the street, neutral right up to
the moment you are inside the shop, and then committed. It is pure JSON -
`anger_triggers: [ "PLAYER_CLOSE", "HURT", "FRIEND_ATTACKED" ]` - but it rests on
two engine facts that are easy to get wrong, both in 13.14.

- `PLAYER_CLOSE` is `mon_trigger::HOSTILE_CLOSE`, worth **+5 anger per planning
  tick** while the player is visible within **5 tiles** (`src/monmove.cpp:520,
  545`). Two ticks inside his shop and he is over the threshold.
- `attitude()` lets a FRIENDLY-faction monster fall past its `MATT_FRIEND` early
  return once `effective_anger >= 10` (`src/monster.cpp:1932`), and
  `MF_FACTION_MEMORY` plus anger >= 10 also skips the `MATT_IGNORE` guard below
  it. So a member of a player-friendly faction really can end up attacking.

**He is ranged**, with vanilla `pipe_shotgun` and birdshot. No bespoke gun: it
is the weakest real 12-gauge in the game (dispersion 855, durability 6, one shell
at a time), and 12 gauge is forced because `shot_bird` will not chamber in the
.410 or the 20ga muzzleloader. `shotgun_s` was the alternative and sits at
dispersion **210**, which would have made a shopkeeper a better shot than the
bikers.

Four things keep him from being a doorway execution:

- `shot_bird` is 10 pellets of 5 damage at `half_angle` 5. **The spread is the
  balance** - nasty at point blank, mostly air past a few tiles.
- He is tuned under `mon_cv_biker_shotgun` on every axis: effective range 7 vs
  10, `fake_skills` 0 vs 1, and **8 shells vs 20**.  His cooldown is 10 against
  their 15, which is *faster* - forced by the firing bug in 13.15, where the
  cooldown must stay under the targeting lock or the gun never goes off at all.
- He cannot open fire on a stranger. `PLAYER_CLOSE` only accrues within 5 tiles,
  so he must already have been angered at close range before the gun is in play;
  the 7-tile envelope just covers him backing across his own shop.
- Eight shells and he is dry, left clubbing with the barrel. That bounds the
  encounter.

Firing costs stay at the vanilla 150. Firing at the player is two-stage - one
attempt spends `targeting_cost` to aim, the next spends `move_cost` to shoot
(`src/mattack_actors.h:170-177`) - and the biker build already established that
raising the costs and shortening the range *together* means they never fire at
all.

They are also the one civilian worth robbing - the till is in the drop table,
along with the bat they did not reach for.

**Sprites 150-153 are mixed gender** (153 is a woman), and one monster id covers
all four rather than splitting into a gendered pair the way `doctor`/`_f` and
`farmer`/`_f` do. So the description, the eight barks, the ten dialogue lines and
the favour messages are all written without pronouns.

Two things make him fair rather than a trap: eight warning barks at volume 25-46,
and the protection-money favour, which is the **only** route back from his anger
(13.14).

**The clergy loot is split three ways** so the living and the dead can share it:
`cv_clergy_worn_priest` / `_nun` (clothing), `cv_clergy_pockets` (loose odds and
ends), and `holy_symbol` rolled separately by each drop table. The zombie
versions take the worn group with `damage: [1, 4]` - the same treatment the
zombie clown and zombie nerd get - and the symbol survives better than the
pockets do, 55% against 65%, because it is round the neck rather than in a
pocket. The pockets fall from certain to 40%.

Churches are the one venue in the mod that mixes the living and the dead in a
single pool - roughly 17% of a church's occupants are already turned. Nowhere
else does this, and it is the point of the location.

**Clergy are venue-locked on purpose** - churches, and a thin presence in the
cemetery/park pool. They are not in `GROUP_CV_INDOOR_SPECIAL`, so meeting one
means finding a church rather than opening enough front doors.

Churches and libraries take interior mode because both palettes are
furniture-rich (`church.json` has `f_bench`, `f_armchair`, `f_chair`, `f_bed`,
`f_wardrobe`; `library_palette` has `f_armchair`, `f_chair`, `f_sofa`, `f_stool`,
`f_locker`). Parks, cemeteries and gardens are open ground and take a `count` -
the same distinction that made the first construction-site build spawn nothing.

### Items

**Personal effects** - `cv_note_personal`, `cv_note_work`, `cv_note_medical`,
`cv_photo`.  Each draws its description from a snippet category, so one id yields
many finds; 43 texts across the four.  Wired into every `*_drops` table.  Only
`cv_photo` has art (sprite 128); the rest borrow base-tileset sprites through
`looks_like` (13.9).

**Otaku collection** - 7 fun-only books, 3 collectibles, 2 body pillows, 1 doll.
Titles live in inline `snippet_category` arrays so item names stay generic and the
joke sits in the description.  All 13 disassemble; none are craftable.

Two carry Lua use actions, both registered in **preload** (13.10):

- `CV_DOLL_CHAT` - one random line per press, one battery charge, matching
  vanilla `talking_doll`'s cadence.
- `CV_PILLOW_HUG` - a small morale bump on a 24-hour cooldown, stamped in mod
  storage so it is per player rather than per pillow.

Everything a person carries comes from one pool, `cv_otaku_carried`: geek and
zombie nerd at 50%, fat bystanders 6%, bystander and panicked 3%, elderly 2%.
World spawns
append to nine vanilla groups - `novels`, `SUS_fiction_bookcase`, `kids_books`,
`magazines`, `toy_box`, `toy_store`, `home_display_case`, `bed`, `bedroom`.  A
`bed` yields a body pillow instead of a pillow about 1.9% of the time.

A geek drops about 6.8 items, of which roughly 4 are the clothes they are wearing.

### Findable dead bodies

Six of them, sprites 154-159, and they are **`CONTAINER` items rather than
vanilla's `GENERIC` corpses** - which is what lets an item group put the dead
person's effects *inside* the body via `contents-group`. You search the body and
find their photograph in it, rather than finding it lying next to them.

That was worth checking rather than assuming, and the first answer was wrong. The
17 vanilla corpse items have no container slot, so at a glance it looks
impossible. But BN has no pocket system at all - there is no `item_pocket.h` -
and `is_container()` simply reads the container slot (`src/item.cpp:7765`), so
declaring `contains` is enough. `contents-group` is documented in
`item_spawn.md` and capacity-checked at load (`src/item_group.cpp:272-286`).
Vanilla's own `bag_body_bag` is the precedent.

They deliberately do **not** carry vanilla's `CORPSE` flag. That flag sets
`is_corpse()` with a *null* monster type (`src/item.cpp:297`) and puts the item
on the butchering path. These are things you search, not things you carve.

**The body's condition predicts its contents**, which is what makes them read as
evidence rather than as loot piles:

| Body | Sprite | Pockets | Keepsake chance |
|---|---|---|---|
| `stripped` | 154 | emptied - somebody was here first | 10% |
| `plain` | 155 | untouched; died of something invisible | 59% |
| `bloodied` / `_alt` | 156/157 | picked over | 37% |
| `eaten` | 158 | **untouched** - what ate them had no use for a wallet | 59% |
| `halved` | 159 | picked over, heavy damage | 37% |

Capacity is 1 L: room for papers, a wallet, a ring, and far too little to serve
as a free rucksack.

The stripped body has **no clothing entry at all**; the absence is the story. It
is also deliberately kept out of `corpse_male` / `corpse_female`, because vanilla
consumes those from inside collections that add a full outfit - which would put
clothes on the one body defined by not having any.

Placement appends to vanilla `corpses`, `corpse_male` and `corpse_female`, so
they reach every house, mansion and residential lawn that already places bodies.
`corpses` declares **no subtype** and therefore loads as a distribution (13.12).

Descriptions come from `snippets_corpses.json` - 36 texts, one category per body.
Each snippet needs an explicit `id`: `random_id_from_category`
(`src/text_snippets.cpp:154`) refuses a category with unlabelled entries, which
is a hard error at load and was hit on the first build.

### Clothing the game did not have

Two `ARMOR` items, both wearable by the player, both filling a genuine vanilla
gap - checked before building: BN has **no scrubs item of any kind** and no
security shirt. The only "scrub" strings in the whole game are a scrub brush and
a CBM.

| Item | Covers | Enc | Coverage | Warmth | Storage |
|---|---|---|---|---|---|
| `cv_shirt_security` | torso | 6 | 90 | 15 | 500 ml |
| `cv_shirt_retail` | torso | 6 | 90 | 15 | 500 ml |
| `cv_scrubs` | torso, legs | 4 | 90 | 14 | 1 L |

Both are statted against vanilla neighbours - `polo_shirt` (enc 7, warmth 15,
thickness 2) and `jumpsuit` (enc 2) - and both carry `looks_like`, so they still
render on a tileset that lacks this mod's sheet. Scrubs deliberately do **not**
cover arms: they are short-sleeved, and the sprite shows bare forearms.

**Worn-overlay tile ids are not free-form.** `find_overlay_looks_like`
(`src/cata_tiles.cpp:4408-4432`) tries the gendered id first and then the plain
one, so the naming has to be exactly:

```
overlay_male_worn_<id>  /  overlay_female_worn_<id>     tried first
overlay_worn_<id>                                        fallback
```

The security shirt has separate male and female art, so it uses the gendered
pair; the scrubs are unisex and use the plain form. Getting this wrong produces
no error - the clothing simply never appears on the character.

They are placed on the archetypes that would wear them: the security shirt is
70% of the mall cop's shirt slot, and scrubs are 55% under a doctor's lab coat
and 70% of a nurse's outfit. In each case a **distribution**, not an extra roll,
so a single body never drops two complete outfits.

The retail shirt is 85% of a retail worker's outfit, and `vest` - which stood in
for the uniform before the item existed - drops back to 20% as the tabard some
stores layer over it, with the plain shirts at 30% as what is worn underneath.
All three can co-occur without reading as three outfits. **Zombie retail workers
get it for free**, because `cv_zombie_retail_drops` already draws
`cv_retail_worn_m`/`_f` with `damage: [1, 4]`.

### Where the clothing turns up in the world

Two vanilla groups are appended to, both measured first so the odds are real
rather than guessed:

| Vanilla group | What it is | Ours | Share |
|---|---|---|---|
| `gear_medical` | what hospital `f_locker` tiles place from | `cv_scrubs` 25, `cv_shirt_security` 5 | ~11% and ~2% of 210 |
| `shirts_unisex` | the torso rack in `clothes_store_palette` | `cv_shirt_security` 6, `cv_scrubs` 5 | ~1% and ~0.8% of 600 |

`gear_medical` declares **no subtype**, so it loads as a distribution and must be
redeclared as one (13.12). The retail shirt is deliberately absent from both: a
store's own uniform is not stock, and it is not hospital kit.

### Sprites

Four blocks.  **Indices are global across the whole tileset** - see 13.2.

| Sheet | Cell | Cells | Range | Art |
|---|---|---|---|---|
| `civilian_variety_normal.png` | 32x32 | 256 | 0-255 | 0-195, contiguous |
| `civilian_variety_biker_handgun.png` | 32x48 | 6 | 256-261 | all |
| `civilian_variety_biker_shotgun.png` | 32x48 | 6 | 262-267 | all |
| `civilian_variety_cop_handgun.png` | 32x48 | 6 | 268-273 | all |

Cells 196-255 are free.  Filling them is safe; resizing a sheet, or inserting a
block anywhere but the end, renumbers everything after it.

`mon_cv_zombie_mallcop` takes 184-185 as a weighted pair and
`mon_cv_zombie_doctor` 186 alone. The household staff close the sheet and are the
first archetypes to arrive with their turned forms drawn as well:
`mon_cv_butler` 187-188, `mon_cv_maid` 189-192, `mon_cv_zombie_butler` 193 alone,
`mon_cv_zombie_maid` 194-195. No `looks_like` placeholders remain anywhere in the
mod. Both dropped
their `looks_like` placeholders when the art landed - an explicit `tiles` entry
wins over `looks_like`, but leaving a stale one behind is a lie in the JSON.

A wielded item needs a second tile id named `overlay_wielded_<item_id>`; the two
body pillows use this for their angled sprites (138 and 142).

### The mansion

The household staff are the first archetypes in this mod that do **not** touch
`GROUP_ZOMBIE`, and that is not restraint about the 80-point cap - it is that
`GROUP_MANSION` is a far better deal.

It lists 25 monsters summing to freq **321**, and `freq_total` defaults to 1000
rather than to that sum:

```cpp
g.freq_total = jo.get_int( "freq_total", ( extending ? g.freq_total : 1000 ) );
                                                    // src/mongroup.cpp:437
```

So **679 of every 1000 rolls fall through to the group's `default`**, which is
`mon_zombie`. Adding 60 points there costs vanilla's named monsters nothing at
all; it comes out of the plain-zombie fallback. That is the exact opposite of the
`GROUP_ZOMBIE` situation the rest of the mod budgets against, and it is worth
checking for before assuming any vanilla group is zero-sum.

The Lua spawner's `mansion` rule previously pointed at `GROUP_CV_BUSINESS`, a
fair stand-in while the mod had no household staff and a mansion was simply
somewhere wealthy people were. It now points at `GROUP_CV_MANSION`, where staff
outnumber the family. The `golfcourse` rule keeps `GROUP_CV_BUSINESS`.

**Vanilla already has `mon_feral_maid_*`, and they are not these.** Those are
living feral humans displayed as "feral servant", `species: [ HUMAN ]`, hostile,
mansion-only, and armed with a broom, a candlestick or a knife. A different
premise entirely - murderous rather than dutiful - so the two sets coexist
without reading as duplicates.

### Lua behaviour

| Hook | What it does |
|---|---|
| `on_mapgen_postprocess` | the spawner |
| `on_try_monster_interaction` | replaces the friendly-creature menu: Talk / favour / Other / Leave alone |
| `add_on_every_x_hook` @ 300 turns | ambulance sweep |
| `add_on_every_x_hook` @ 600 turns | ambient dread |
| `game.iuse_functions` | `CV_DOLL_CHAT`, `CV_PILLOW_HUG` |
| `game.activity_functions` | `cv_company_finished`, `cv_treatment_finished` |

**Favours** are one-shot per individual, marked with `effect_cv_helped`:

| Archetype | Favour |
|---|---|
| Doctor (+`_f`) | timed treatment - 15-30 min interruptible activity, then `healall(6-12)` |
| Nurse | 50% chance of medical supplies on the ground |
| Mechanic / Firefighter / Farmer (+`_f`) | 50% chance of a relevant item |
| Sex worker | 20% decline, else a paid 10-30 minute activity and morale |

Doctors treat and give nothing; nurses supply and heal nothing.  The doctor checks
`hp_percentage()` first and does **not** spend the favour on an uninjured player.
Because the healing lands in `on_finish`, an interruption costs both the treatment
and the favour.

**Ambient dread** fires at most once per ~2.8 hours behind a 90-minute floor, and
picks a bucket cheapest-first: night, indoors, civilians nearby, day.  27 lines.
It is `add_msg` only and cannot draw monsters - see 13.11.

---

## 13. Engine facts learned the hard way

Each of these cost a real bug.  None is discoverable from JSON, and several
produce **no error message at all**.

### 13.1 Faction hostility must be declared; the default is FRIENDLY

`resolve_attitude_map` walks up `base_faction` and, finding no relation, returns
**`MFA_FRIENDLY`** (`src/monfaction.cpp:149-152`).  Zombies get their hostility by
another route, so reasoning from them is wrong.  Always list the player.

Found much later, and related: for a faction that `hate`s the player,
`monster::attitude` returns `MATT_ATTACK` from an **early return** before morale or
anger is read at all (`src/monster.cpp:1925-1931`).  Fear triggers on a hostile
faction therefore do nothing to the player - they matter only against zombies.

### 13.2 mod_tileset sprite indices are GLOBAL, not per-file

Each block continues numbering where the previous ended, and reserves cells by its
image's **dimensions**, not by how many contain art (`src/cata_tiles.cpp:1120,
1135-1136`).  The bundled mod proves it: `civilian_survivor_guardian_elite_rifle.png`
holds six sprites and its block references **70-75**.

Using local indices made pistol bikers render as civilians and shotgun bikers
invisible - one mistake, two symptoms, no error.  `tools/check_sprites.py` guards
this now.  A tile's `fg` may be a bare index **or** a list of weighted entries;
both are valid and the validator handles both.

### 13.3 Gun attacks are two-stage, and the levers interact

`require_targeting_player` defaults true and `targeting_cost` to 100
(`src/mattack_actors.h:170-177`).  One attempt aims; the next fires.  Doubling both
costs while shortening range meant armed bikers could never finish the cycle and
walked up to punch people instead.  Costs are the vanilla 150, ranges 10-18.

### 13.4 copy-from merges `upgrades`; `into` plus inherited `into_group` is fatal

```
ERROR: both into and into_group defined for monster mon_cv_bystander_fat
```

Use a single-member group instead of `into` when the parent defines `into_group`.

**The mirror image bites too, and it is less obvious.** If the *parent* defines
`into` and the child adds `into_group`, the child inherits the parent's `into`
and hits the same error - `mon_cv_zombie_rot` did exactly this on first build:

```
ERROR: both into and into_group defined for monster mon_cv_zombie_rot
```

Omitting the key does not clear it. `monstergenerator.cpp:1038` reads it with
`optional( up, was_loaded, "into", ... )`, and with `was_loaded` true an absent
member keeps the inherited value. The only way to clear it from JSON is to set it
to the null id explicitly:

```json
"upgrades": { "half_life": 21, "into": "mon_null", "into_group": "GROUP_CV_ROT_UPGRADE" }
```

`mon_null` is `mtype_id::NULL_ID()` (`src/string_id_null_ids.cpp:74`), so this
reads as "no single target", and the check at `monstergenerator.cpp:1655` passes.

### 13.5 HIT_AND_RUN makes a monster unable to commit

`add_effect( effect_run, 4_turns )` after **every** melee attack
(`src/monster.cpp:2355`).  Bikers closed, swung once, fled four turns and repeated,
reading as though they never attacked at all.  It also broke the ranged ones,
because fleeing resets the two-stage targeting cycle.  Raider flavour is not worth
a monster that cannot fight.

### 13.6 Followers are not worth building on BN's pet AI

Built and removed.  A pet paths toward the player only when it has **no target**
and **currently sees them** (`src/monmove.cpp:873-953`), so followers wandered off.
`effect_led_by_leash` (`src/monmove.cpp:986`) is the only override.  Pet armour
cannot work either - every `PET_ARMOR` declares `"pet_bodytype": "quadruped"`.

Two mechanics kept from that work: `make_friendly()` sets a temporary countdown
while `make_pet()` is the real call, and hooks communicate by **mutating
`params.results`**, pre-set to `allowed = true` (`src/catalua.cpp:697-701`) - a
returned table is silently discarded.

### 13.7 Lua cannot create vehicles

`get_vehicles` and `replace_vehicle` only.  `WrappedVehicle` comes from the live
map; the `mapgen_constructor` returns raw `vehicle *` with no Lua type.  Hence the
ambulance sweep runs on a timer.  If `map_extra` is ever used: overlays merge
extras per entry (`src/regional_settings.cpp:80`) but `chance` is a straight
overwrite (`72-73`) that would silently clobber another mod.

### 13.8 Bindings, defaults, and the shapes that surprise

- **C++ default arguments do not reach Lua.**  `SET_FX_T` binds the explicit
  signature, so `get_item_with_id( itype, bool )` needs both arguments despite
  `character.h:1798` declaring `need_charges = false`.  Omitting it raises
  *"stack index 3, expected boolean, received no value"*.
- **`"stackable": true` items are one object carrying charges.**  `remove_item`
  took an entire stack of money bundles; `use_charges( id, 1 )` takes one.
- **Bodypart ids are not registered for Lua.**  Neither `bodypart_id` nor
  `bodypart_str_id` exists, so per-limb effects such as `bandaged` - read per part
  at `src/character.cpp:5492` - are unreachable.  `healall( n )` is the healing
  route, and it clamps to `min(dam, max - cur)` per part
  (`src/character.cpp:9864`).  `mod_all_parts_hp_cur` does a bare `hp_cur += mod`
  with **no clamp**, and would push a healthy player above maximum.
- **Grep for the macro form too.**  Searching for `"set_part_hp_cur"` found
  nothing because these bind as `SET_FX_T( set_part_hp_cur, ... )` without quotes,
  which led to a wrong conclusion that healing was impossible from Lua.

### 13.9 Items support looks_like, and it resolves at render time

`src/item_factory.cpp:2918` reads it; `find_tile_looks_like`
(`src/cata_tiles.cpp:4311`) walks the chain when an id has no sprite of its own.  A
new item pointing at `survnote` or `photo_album` renders correctly with no art at
all, and falls back to its ASCII symbol in a tileset that has neither.

### 13.10 Load order: preload runs before JSON

The item factory reads `game.iuse_functions` while finalising item definitions,
which happens **before** `main.lua` runs.  A `use_action` registered in main.lua
would not exist when the item referencing it loads, so both iuse functions live in
`preload.lua`.

For the same reason, ids are built **inside** handlers rather than at preload
top level: `MoraleTypeDataId.new(...)` at registration time would name a morale
type the game has not read yet.

Two smaller item rules, both hard errors: a `use_action` must have a matching
`item_action` object or the factory warns that the action is undescribed, and
`str_pl` must be omitted when the plural is regular - identical singular and
plural forms use `str_sp`, not `str` plus `str_pl`.

### 13.11 There is no world-sound binding in Lua

Only `play_variant_sound` / `play_ambient_variant_sound`, which push audio to the
speakers.  `sounds::sound()`, which propagates through the map and draws zombies,
is not exposed.  Ambient dread therefore **cannot** attract monsters, and must not
be "improved" into something that does.

Monsters are the exception: `PARROT_AT_DANGER` speech does go through
`sounds::sound`, which is precisely why the panicked bystander - shouting at volume
70-90 against an ordinary bystander's 30 - calls the horde down on itself.

### 13.12 Redeclaring an item_group appends; a missing subtype means distribution

`make_group_or_throw` reuses the existing group and errors only on a **subtype**
mismatch (`src/item_factory.cpp:3217`); `"purge": true` is the destructive opt-in.
So a mod extends vanilla loot by redeclaring the id, and no vanilla file is touched.

The trap: `jsobj.get_string( "subtype", "old" )` (`src/item_factory.cpp:3454`), and
`"old"` maps to **distribution**.  A vanilla group with no `subtype` key is a
distribution, not a collection.  `toy_store` and `home_display_case` are both like
this, and declaring them as collections threw
*"already defined with type distribution"*.

Note also that a prob is a **relative weight** in a distribution and a **straight
percentage** in a collection, so the same number means very different things.

### 13.13 Verification, and its limits

```
cd E:\Cataclysm_Test && ./cataclysm-bn-tiles.exe --check-mods civilian_variety
```

Validates all JSON **and runs the mod's Lua**, headless, with no world required.

**Exit code alone is not sufficient.**  The subtype clash above printed
`Error loading data: item group "toy_store" already defined...` and still exited
**0**.  Grep the output for `Error loading data` as well as checking the status,
and read `config/debug.log` for warnings that never reach stdout.

It does **not** execute anything gameplay reaches: hook bodies, menu branches,
periodic handlers and use actions all stay untouched.  That blind spot is exactly
where the `get_item_with_id` bug survived into a live world.

And a validator that agrees with you is worth nothing.  The subtype bug was
reported as "0 mismatches" by a check written with the same wrong assumption as
the code it was checking.  The game caught it; the checker did not.

### 13.14 Monster anger toward the player: two traps

**`aggression` is a no-op against the player for any FRIENDLY-faction
`MF_FACTION_MEMORY` monster.** `attitude()` computes `effective_anger` toward
the player as `get_faction_anger( "player" )` *alone*; the `anger` member is only
folded in when the faction's attitude is `MFA_BY_MOOD`
(`src/monster.cpp:1818-1834`). `civilians` is FRIENDLY, so every monster in this
mod ignores its own `aggression` field where the player is concerned. Setting it
to buy a grace period would have looked reasonable and done nothing.

**`faction_anger` never decays.** Grep the whole tree: the engine only ever adds
to it (`src/monster.cpp:4519`), saves it, and loads it back. `process_triggers`
explicitly skips the usual anger restoration for `FACTION_MEMORY` monsters -
*"Don't restore global anger for FACTION_MEMORY monsters"*. So a monster angered
once is angry for the rest of that save unless something subtracts it.

Lua can, and that is what the store owner's favour does:

```lua
local held = mon:get_faction_anger("player")
if held > 0 then mon:add_faction_anger("player", -held) end
```

`get_faction_anger` / `add_faction_anger` are bound at
`src/catalua_bindings_creature.cpp:594-601`, and `monster.anger` / `.morale` are
plain members via `SET_MEMB`. Anything built on `PLAYER_CLOSE` needs a route back
like this, or one careless step is permanent.

### 13.15 A monster gun that never fires

`gun_actor` shoots in **two stages**. The first attempt only *aims*: it applies
`effect_targeted` for `targeting_timeout` turns - **8 by default**
(`src/mattack_actors.h:176`) - spends `targeting_cost`, and returns without
firing. Only a later attempt, while that effect is still live, actually shoots.

The trap is that `attempt_shoot` returns **true even when it merely aimed**, and
`src/monmove.cpp:1551` resets the full cooldown on any attempt that returns true.
So if `cooldown` outlives the lock, every attempt is stage one:

```
turn N     ready -> no lock -> aim, 8-turn lock, cooldown = 25
turn N+8   lock expires
turn N+25  ready -> no lock -> aim again ... forever
```

**The invariant is `targeting_timeout` > `cooldown`.** The store owner shipped
with `cooldown: 25` against the default 8 and could not fire a single shell - he
simply walked up and clubbed people with the barrel, which is exactly what it
looked like in play. Fixed at cooldown 10, `targeting_timeout` 24,
`targeting_timeout_extend` 24.

Note that `--check-mods` cannot catch this. The JSON is valid and the mod loads
clean; the attack is simply unreachable at runtime (13.13).

**The bikers have the same latent defect** - cooldowns 12, 14 and 15 against the
default 8 - and have presumably never fired either. Left alone deliberately:
their difficulty was tuned by play while they were silently melee-only, so making
them shoot is a real balance change and belongs to a separate decision.

---

### 13.16 `special_attacks` on a copy-from monster CLEARS the parent's list

It does not merge. The loader is explicit about it:

```cpp
if( !was_loaded || jo.has_member( "special_attacks" ) ) {
    special_attacks.clear();
    special_attacks_names.clear();
    add_special_attacks( jo, "special_attacks", src );   // src/monstergenerator.cpp:1009
} else {
    // Note: special_attacks left as is, new attacks are added to it!
    if( jo.has_object( "extend" ) ) { ... }              // :1016-1020
```

Merely *mentioning* the member takes the clearing branch. So a zombie that
`copy-from`s `mon_zombie` and declares `"special_attacks": [ [ "TAZER", 8 ] ]`
does not gain a tazer - it **trades away** `bite`, `GRAB` and `scratch` for one,
and with `bite` goes the entire infection vector. The monster still fights, via
`melee_dice`, so nothing looks broken and `--check-mods` passes (13.13); it simply
stops being able to infect anyone.

`mon_cv_zombie_mallcop` shipped that way for one session and was corrected.
**Anything adding an attack to an inherited monster must use `extend`:**

```json
"extend": { "special_attacks": [ { "id": "cv_syringe_inject" } ] }
```

A bare `special_attacks` is correct only where the full list is being stated -
`mon_cv_mallcop`, which inherits from `mon_cv_bystander` and restates both
`TAZER` and `PARROT_AT_DANGER`, is fine.

---

## 14. Open items

### Known, unresolved

- **Rifle and shotgun bikers share one sheet** and are identical until one fires.
- **`mon_cv_firefighter` alone matches the player's speed** at 100.
- **`GROUP_ZOMBIE` budget at 77 of 80** - 3 points of headroom left.
- **The main sheet is filled through cell 195**; cells 196-255 are free.  Filling
  empty cells is safe, appending new blocks is safe, resizing is not.

### Backlog, as of 2026-08-25

Ordered by how much they were wanted, not by effort.

1. **A favour for the elderly.** They are the only archetype with real dialogue and
   no favour. Best version inverts the pattern: they ask the player for something,
   and the reward is a keepsake or the cane rather than a consumable.
3. **`cv_elderly_meds` into vanilla world groups** - pharmacy, softdrugs, bathroom
   cabinets. Same technique as the otaku world-spawn pass (12, item groups append).
4. **Dog walker**, using vanilla `mon_dog` and `pack_size`. `pack_size` multiplies
   creatures without costing `freq`, so it is nearly free against the 2.7 budget.
5. **More zombie counterparts.** Clown, nerd, firefighter and clergy have them
   now. The construction worker is the obvious next one - distinctive silhouette,
   and the gear surviving death is the point.
6. **Hospital evacuee** - patients in gowns, tied to the ambulance sweep.
7. **Named individuals** - a handful of fixed people, rare spawns, high recall.
8. **Scavenger faction** - a neutral third party. The biker faction already proved
   the hostile-faction machinery; this is its counterpart.
9. Also raised, not costed: group packs, seasonal archetypes, sleepers.

**Scrapped:** witnesses (civilians remembering a killing). Dropped 2026-08-25.

**Needs a throwaway probe before any design work:** anything requiring Lua to
*write* game state not yet confirmed writable - faction anger, overmap reveals,
death callbacks. That unknown is what sank witnesses; do not design around one
again without checking first (13.13: a validator that agrees with you is worth
nothing).

