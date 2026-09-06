# Monster Dialogue — Design

**Date:** 2026-09-05 (revised 2026-09-06)
**Project:** Cataclysm: Slop Edition (CSE)
**Status:** Approved in principle by the owner. Implementation plan at
`docs/superpowers/plans/2026-09-06-monster-dialogue.md`.
**Reference implementation:** Cataclysm: DDA `talker` abstraction (pinned at `5b915aea09`)

## Problem

CSE wants monsters to hold real conversations — barter, negotiation, recruitment,
speech checks — inspired by Battlespire and Shin Megami Tensei, where talking to
a monster is an alternative to killing it.

**The owner's decision: monsters use the NPC dialogue system itself, not a
parallel one.** The reason is long-run flexibility, and it is the right call —
but the valuable inheritance is not the window. It is the JSON vocabulary:
`condition.cpp` is 1,178 lines of shipped, tested predicates, and `talk_trial`,
the effect verbs and the topic loader come with it. Per `CLAUDE.md`'s fork-cost
ladder, content expressed as JSON is tier 1, near-zero merge cost. A bespoke
system would make every future monster behaviour C++; this makes it data.

The obstacle is that a conversation is structurally an NPC. `struct dialogue`
(`dialogue.h:221`) holds:

```cpp
player *alpha = nullptr;   // dialogue.h:226 — always g->u
npc    *beta  = nullptr;   // dialogue.h:231 — "The NPC we talk to.  Never null."
```

and `npc::talk_to_u()` (`npctalk.cpp:1094`) is the only door in.

## Part 1 — Feasibility

### The coupling is shallower than it looks

Measured, not estimated:

| Measure | Count |
|---|---|
| `beta->` uses in the entire codebase | **79** |
| Files containing them | **2** (`npctalk.cpp`, `condition.cpp`) |
| `talk_function::` definitions taking `npc &` | 73 (`npctalk_funcs.cpp`) |
| `npc::talk_to_u()` call sites | 7 |
| Dialogue UI code needing changes | **0** |

The 79 uses split cleanly along the line that matters:

| Member | Uses | Monster needs it? |
|---|---|---|
| `chatbin` (missions, first topic, selected skill/style/spell) | 21 | No — NPC-only |
| `rules` (follower engagement, aim, CBM reserve, pickup) | 14 | No — NPC-only |
| `op_of_u` (trust/fear/value/anger/owed) | 7 | No — monsters use `anger`/`morale` |
| `name`, `disp_name` | 6 | **Yes** |
| `value()`, `get_faction()`, `wield_better_weapon()` | 9 | Trade/AI — NPC-only |
| one-offs: `has_effect`, `is_friendly`, `is_enemy`, `make_angry`, `bub_pos`, `abs_omt_pos`, `get_dimension`, `get_grammatical_genders`, `say` | ~22 | **Yes, all** |

Two thirds of the coupling is to machinery a monster has no business touching.

**The dialogue window is already decoupled.** `dialogue_win.h` mentions `npc`
only in a comment; `dialogue_window` stores a `std::string npc_name`
(`dialogue_win.h:66`) and `dialogue::opt()` takes that name as a parameter
(`npctalk.cpp:2158`). Point `beta` at a talker and the same screen renders a
monster. The UI needs no work at all.

### The hardcoded NPC response chain is already bypassed

`dialogue::gen_responses()` (`npctalk.cpp:1653`) contains a long chain of
hardcoded NPC topics dereferencing `p->chatbin`, missions and training. A monster
falling into it would crash. It cannot, because both entry points consult JSON
first and return early:

```cpp
const auto iter = json_talk_topics.find( topic );
if( iter != json_talk_topics.end() ) {
    if( jtt.gen_responses( *this ) ) {
        return;                       // npctalk.cpp:1660
    }
}
```

`dynamic_line()` (`npctalk.cpp:1300`) has the same shape. **With one condition:**
that early return is `return replace_built_in_responses;` (`npctalk.cpp:3670`).
A monster topic that omits `"replace_built_in_responses": true` falls straight
through into the NPC chain. This is therefore a hard requirement on monster
topics, enforced at load, not a convention.

### The one real gap in speech output

`say()` is declared on `npc` (`npc.h:961`), not `Creature`. The main dialogue
loop calls `d.beta->say( _( "Bye." ) )` (`npctalk.cpp:1273`). `talker_monster`
must implement speech itself — `sounds::sound()` at the monster's position, or
`add_msg`. **Do not add `say()` to `Creature`** to make this convenient; that is
a tier-4 edit to a file every upstream change touches.

### Barter does not port, and the dialogue system is not why

`class monster : public Creature` (`monster.h:105`) — a monster is **not a
Character**, and trade is Character-to-Character throughout:

| Function | Signature |
|---|---|
| `npc_trading::trade` | `( npc &np, int cost, ... )` — `npctrade.cpp:305` |
| `transfer_items` | `( ..., Character &giver, Character &receiver, bool npc_gives )` |
| `setup_trade_state`, `npc_will_accept_trade`, `calc_npc_owes_you`, `update_npc_owed`, `pay_npc` | all take `npc &` |

The pricing model reads `np.value()`, `op_of_u.owed`, `is_shopkeeper`,
`wants_to_sell`, `max_willing_to_owe`, `shop_restock`. A monster has none of
them, and its inventory is a bare `location_vector<item> inv` (`monster.h:828`)
with `add_item`/`get_items` — no pockets, no worn slots, no capacity model, no
ownership.

The trade *window* is reusable (`trading_window` takes a `trade_state`,
`trade_win.h:20`); everything that fills a `trade_state` is NPC-shaped.

**Decision: trade is out of scope, replaced by offerings.** `u_consume_item`
branches on `is_npc` and only touches `d.alpha` (`npctalk.cpp:2613`), so "give it
meat" is expressible in JSON today: the item leaves the player, and a monster-side
effect drops its anger. This is thematically stronger than haggling — you do not
barter with a mi-go, you make an offering — and it lands on the `MEAT` placate
trigger the engine already has. Real haggling, if ever wanted, is its own spec.

## Part 2 — Gameplay design

### Drive the emotional model that already exists

CSE already has a full negotiation substrate with no UI. `monster` carries live
state the AI reads every turn:

| Field | Meaning |
|---|---|
| `anger`, `morale` | ints on the instance (`monster.h:634`) |
| `mtype::agro` | `[-100,100]` starting aggression (`mtype.h:323`) |
| `anger` / `fear` / `placate` trigger sets | 12 `mon_trigger` values (`mtype.h:46`, `mtype.h:269`) |
| `MF_FACTION_MEMORY` | tracks anger **per faction** — it remembers you |
| `make_friendly()`, `make_ally()`, `make_pet()` | recruitment, already implemented |

`monster::attitude()` (`monster.cpp:1809`) turns those into behaviour on
thresholds that are exactly the right negotiation targets:

```
morale < 0    → FLEE   (or FOLLOW if morale + anger > 0 and hp > 1/3)
anger <= 0    → IGNORE (or FLEE if hurt)
anger < 10    → FOLLOW
otherwise     → ATTACK
```

**So a successful negotiation is `anger -= 15`.** Not a bespoke reputation stat —
the variable the pathing, fleeing and targeting code already obeys. Three things
follow for free: every existing AI behaviour respects the outcome; `regen_morale`
means a talked-down monster drifts back toward hostile over time; and
`MF_FACTION_MEMORY` means a wronged faction remembers.

The precedent for "some characters can talk to some monsters" is already
throughout that function: `PROF_FERAL` befriends zombies, `THRESH_MYCUS`
pacifies fungals, `BEE`/`FLOWERS`, `ANIMALEMPATH`, pheromone mutations, and
per-mutation `anger_relations` / `ignored_by` keyed by species. The game already
believes in this idea.

### Sapience is a ladder of verbs, not a difficulty number

Four tiers, all derivable from existing `species` and `m_flag` data. **These are
authoring conventions in JSON, never C++ branches.**

| Tier | Who | The verb | Mechanism |
|---|---|---|---|
| **Mute** | zombies, most horrors | none — you shout, it hears | **No `chat_topic`.** Not a conversation. |
| **Reactive** | `MF_ANIMAL`, insects | calm / threaten / feed | Trials on morale; offerings |
| **Semantic** | robots, `MF_CARD_OVERRIDE`, turrets | commands, not persuasion | `TALK_TRIAL_CONDITION`; INT and computers |
| **Sapient** | mi-go, some mutants, named uniques | full conversation | Topic trees, all trials, recruitment |

**The mute tier must not open a dialogue window.** Showing a conversation screen
to say "it groans and lunges" is worse than a message line. No `chat_topic` means
it is a reaction, not a conversation: a message, a `SOUND` trigger, anger up,
horde drawn. Attempting to talk to a zombie should have a real cost, and teaches
the tier system in one attempt.

### Balance: gate on traits, not on tuning

If negotiation is reliable, combat becomes optional. Four mitigations, in the
order they should be leaned on:

1. **Gate availability on mutations and traits.** Talking to monsters becomes a
   *build*, not a universal verb. This fits the game's identity, reuses the
   existing `u_has_trait` condition, and solves the balance problem by
   construction rather than by numbers. This is the primary mitigation.
2. Failure has teeth — anger up, `SOUND` fires.
3. Once per encounter; a refusal sticks via an effect or faction anger.
4. Refusal scales with `mtype::difficulty`; the big ones do not listen.

### Tone

Cataclysm is horror; SMT is a comedy of manners with demons. If most monsters
chat, the horror evaporates. Verbal dialogue should stay **rare and disturbing**
— the mi-go that negotiates should be worse to talk to than to fight. The
playtest question "does a talking zombie stop being a zombie?" has the answer:
only if it is friendly about it.

### Directions worth keeping in view

- **Queens and hives.** `MF_QUEEN` plus faction anger means one negotiation could
  set an entire faction's attitude.
- **Thralls.** `MATT_ZLAVE` and `effect_pacified` already exist — a darker
  recruitment outcome than befriending.

### Content budget

There are hundreds of `mtype`s. Tier-level generic behaviour covers almost all of
them with **no writing at all**; hand-written dialogue is reserved for the sapient
tier and named uniques — on the order of fifteen to thirty entities, ever.
Scarcity is what makes the special ones land.

## Part 3 — Architecture

```
dialogue
  ├── player  *alpha            (unchanged)
  └── talker  *beta             (was: npc *)
                ├── talker_npc      → wraps npc &,     everything implemented
                └── talker_monster  → wraps monster &, NPC-only members absent
```

`talker` is a **deliberately narrow** abstract interface: the ~22 generic uses
from the table above, plus `get_npc()` returning `nullptr` for monsters. NPC-only
members do not appear on the interface at all — call sites needing them go
through `get_npc()`, so a monster reaching NPC machinery is a compile error, not
a runtime check.

That narrowness is the main design risk, not the main design cost. Once `beta` is
a talker, every NPC feature looks one virtual method away — missions, trade,
followers, opinion. That gravity is how a 300-line seam becomes a 3,000-line one.
**Adding a virtual to `talker` requires the same justification as adding a field
to `item`.**

### Response and effect gating

73 `talk_function::` entries take `npc &` and are reachable from JSON by name
through `static_functions_map` (`npctalk.cpp:3142`). Ungated, a JSON author points
a monster topic at `assign_mission` and the game crashes or silently no-ops.

Gate at **selection** time, not call time: a response whose effect is NPC-only is
never added to the response list for a monster speaker. The player never sees an
option that cannot work, and there is no runtime branch to forget. This mirrors
how `gen_responses()` already prunes on conditions. `load_talk_topic()`
(`npctalk.cpp:3694`, registered at `init.cpp:450`) and
`json_talk_topic::check_consistency()` are where a **load-time** error belongs for
a topic that declares itself monster-usable while carrying NPC-only effects —
turning an author mistake into a startup message rather than a playtest crash.

## Part 4 — Known defects and traps in the code being touched

Found while specifying. All three are in code this work must modify.

1. **`repeat_responses` will null-deref on a monster.** `npctalk.cpp:3646` does
   `actor = dynamic_cast<player *>( d.beta )` when `repeat.is_npc`, then calls
   `actor->charges_of(...)` with no null check. Needs a guard regardless.
2. **`parse_mod`'s `NPC_INTIMIDATE` is an upstream bug.** At `npctalk.cpp:1791`
   it computes `character_effects::intimidation( u )` — `u` is the *player*
   (`player &u = *d.alpha`) — identical to `U_INTIMIDATE` on the line above. It
   should be `p`. BN's defect, not CSE's, but it sits in the exact function
   monster trials extend and should be fixed here rather than inherited.
3. **`replace_built_in_responses` is load-bearing**, per Part 1.

## Open questions

Owner decisions. None should be made while AFK.

1. **Hostile monsters.** Can you talk to something actively attacking you? This
   changes the entry point. Deferred to the milestone-2 playtest.
2. **Turn cost.** Talking is free for NPCs. Free conversation with a hostile is an
   exploit; a costly one is a trap.
3. **Base game or mod?** Whether any shipped `mtype` gets a `chat_topic`.
4. **Which traits gate it**, given the balance decision above.
5. **Recruitment outcome** — friendly, pet, or `MATT_ZLAVE` thrall — and whether
   monster dialogue may move faction anger.
