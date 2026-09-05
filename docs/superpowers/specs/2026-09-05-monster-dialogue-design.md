# Monster Dialogue — Design

**Date:** 2026-09-05
**Project:** Cataclysm: Slop Edition (CSE)
**Status:** Proposed. Nothing implemented. Phase gates in **Open questions**.
**Reference implementation:** Cataclysm: DDA `talker` abstraction (pinned at `5b915aea09`)

## Problem

CSE wants NPC-style branching dialogue available to monsters — not only to the
handful that already emit canned speech through `speech.json`, but to any `mtype`
that cares to declare it.

CBN forked from CDDA before CDDA generalised its dialogue system. In CSE today,
a conversation is structurally an NPC: `struct dialogue` (`src/dialogue.h:221`)
holds

```cpp
player *alpha = nullptr;   // dialogue.h:226 — always g->u
npc    *beta  = nullptr;   // dialogue.h:231 — "The NPC we talk to.  Never null."
```

and `npc::talk_to_u()` (`src/npctalk.cpp:1094`) is the only door in. A monster
cannot be a `beta`, so no monster can hold a topic, a response, a trial or an
effect.

CDDA solved this with a `talker` interface and `talker_npc` / `talker_monster` /
`talker_character` / `talker_item` implementations, plus a `chat_topic` field on
`mtype`. That refactor is expensive *in DDA* because it is entangled with their
newer effect and condition system. CSE's dialogue surface is much smaller, and
that is the whole reason this is worth doing here.

## The coupling is shallower than it looks

Measured, not estimated:

| Measure | Count |
|---|---|
| `beta->` uses in the entire codebase | **79** |
| Files containing them | **2** (`npctalk.cpp`, `condition.cpp`) |
| `talk_function::` definitions taking `npc &` | 73 (`npctalk_funcs.cpp`) |
| `npc::talk_to_u()` call sites | 7 |
| Dialogue UI code needing changes | **0** — see below |

The 79 uses cluster hard, and the clusters split cleanly along the line that
matters:

| Member | Uses | Monster needs it? |
|---|---|---|
| `chatbin` (missions, first topic, selected skill/style/spell) | 21 | No — NPC-only |
| `rules` (follower engagement, aim, CBM reserve, pickup) | 14 | No — NPC-only |
| `op_of_u` (trust/fear/value/anger/owed) | 7 | Not initially |
| `name`, `disp_name` | 6 | **Yes** |
| `value()`, `get_faction()`, `wield_better_weapon()` | 9 | Trade/AI — NPC-only |
| one-offs: `has_effect`, `is_friendly`, `is_enemy`, `make_angry`, `bub_pos`, `abs_omt_pos`, `get_dimension`, `get_grammatical_genders`, `say` | ~22 | **Yes, all** |

So roughly two thirds of the coupling is to machinery a talking zombie has no
business touching (missions, follower rules, opinion, trade), and the remaining
third is `Creature`-level state a monster already has.

**The dialogue window is already decoupled.** `dialogue_win.h` mentions `npc`
only in a comment; `dialogue_window` stores a `std::string npc_name`
(`dialogue_win.h:66`) and `dialogue::opt()` takes that name as a parameter
(`npctalk.cpp:2158`). The UI needs no work at all.

**The one real gap is speech output.** `say()` is declared on `npc`
(`npc.h:961`), not on `Creature`. A monster has no equivalent, so the talker
implementation has to provide one (`sounds::sound()` at the monster's position,
or a plain `add_msg`) rather than forward to an existing method.

## Decisions

| Decision | Choice |
|---|---|
| Overall shape | Two phases, gated on a playtest, not one big port |
| Phase A vehicle | Lua, via hooks that already exist. Zero C++ |
| Phase B vehicle | A `talker` seam replacing `dialogue::beta` |
| Phase B scope | Dialogue only. No missions, no trade, no follower rules for monsters |
| JSON schema | CDDA-compatible `"chat_topic"` on `mtype`; existing `talk_topic` objects reused verbatim |
| NPC-only effects | Gated at response-selection time, not at call time |
| Phase A's content | Survives Phase B as flavour data; it is not thrown away |

### Why two phases

The question this feature actually turns on is not technical. It is whether a
talking zombie is still a zombie. That is a playtest question, and per
`CLAUDE.md` the suite cannot answer it. Phase A exists to put it in front of the
owner for the price of an evening, before any C++ merge debt is taken on.

Concretely, Phase A answers:

- Do you talk to a hostile mid-fight, or only to something already non-hostile?
- Does opening your mouth cost a turn? Does it break stealth?
- What does a hostile monster do while the dialogue window is open — does the
  world tick?
- How many monsters should have this? (One mod's worth, or base game?)

Phase B is a moderate C++ change to files that conflict with upstream BN. It
should not be spent on a feature that turns out to feel wrong.

## Phase A — Lua prototype (zero C++)

Everything needed is already wired.

| Hook | Fires at | Gives |
|---|---|---|
| `on_try_monster_interaction` | `game.cpp:8624`, inside `game::examine()` | `params["monster"]`; returns `allowed` |
| `on_monster_get_examine_menu_entries` | `monexamine.cpp:641` | `avatar` + `monster`; returns menu rows |
| `on_monster_examine_menu_entry` | `monexamine.cpp:824` | the chosen row |
| `on_dialogue_start` / `_option` / `_end` | `npctalk.cpp:1211` / `1250` / `1280` | observe real NPC dialogue |

`on_try_monster_interaction` is the important one: it runs for **any** monster on
the examined tile, hostile included, *before* the pet / mech / pay-bot / friendly
branches that gate `monexamine`'s menus. A Lua hook can therefore run its own
conversation there and return `allowed = false` to swallow the normal path.

There is also a **per-monster-type** vehicle: `lua_monster_callback_actor`
(`catalua_icallback_actor.h:275`) is attached to `mtype`
(`mtype.h:482`, reached via `monster::get_lua_callbacks()`, `monster.h:766`) and
already exposes `call_get_examine_menu_entries` and
`call_on_examine_menu_entry`. Dialogue declared per monster type in JSON, handled
in Lua, needs no global hook at all.

`uilist` and `query_popup` are bound in `catalua_bindings_ui.cpp`, so the
conversation UI is a few lines. `data/mods/` carries working Lua mods to copy
structure from, including `NPC_lua_hook_test`.

**What Phase A deliberately does not get:** JSON `talk_topic` objects,
`talk_trial` rolls, the condition system in `condition.cpp` (1,178 lines), and
the `u_*` effect vocabulary. It is a parallel dialogue system. That is acceptable
for flavour and simple branching, and unacceptable as a foundation — which is
exactly why it is a prototype and not the design.

## Phase B — the `talker` seam

Only if Phase A survives its playtest.

```
dialogue
  ├── player  *alpha            (unchanged)
  └── talker  *beta             (was: npc *)
                ├── talker_npc      → wraps npc &,     everything implemented
                └── talker_monster  → wraps monster &, NPC-only members refuse
```

`talker` is a narrow abstract interface — the ~22 generic uses from the table
above, plus explicit `get_npc()` returning `nullptr` for monsters. NPC-only
members do not appear on the interface at all; call sites that need them go
through `get_npc()` and are unreachable for a monster by construction.

### Work items

1. **`src/talker.h` (new file)** — interface plus both implementations. New file,
   so per `CLAUDE.md`'s fork-cost ladder this is tier 2: it conflicts only if
   upstream adds the same path.
2. **`dialogue::beta` retype** — 79 mechanical call-site edits in two files.
   Tier 4 work, but concentrated, and the compiler finds every one.
3. **`"chat_topic"` on `mtype`** — parsed in `mtype::load()`
   (`monstergenerator.cpp:759`), defaulting to empty. Empty means "cannot talk",
   which is every existing monster, so this is a no-op for all current content.
4. **`monster::talk_to_u()`** — mirrors `npc::talk_to_u()` (`npctalk.cpp:1094`)
   minus the mission, attitude, radio and follower blocks. Perhaps 30 lines.
5. **Entry point** — a `Talk` row in the monster menus, gated on a non-empty
   `chat_topic`. `game::examine()` (`game.cpp:8602`) is where the NPC path
   already lives.
6. **Response gating** — a `speaker` predicate on `json_talk_response` so a
   response whose effect is NPC-only is not offered when `beta` is a monster.
   See below.
7. **Condition audit** — the ~28 `d.beta->` sites in `condition.cpp` each need a
   monster answer or a documented refusal.

### Response gating is the part to get right

73 `talk_function::` entries take `npc &` and are reachable from JSON by name.
Left ungated, a JSON author points a monster topic at `assign_mission` and the
game either crashes or silently no-ops.

Gate at **selection** time, not call time: a response whose effect touches
NPC-only machinery is never added to the response list for a monster speaker.
The player never sees an option that cannot work, and there is no runtime branch
to forget. This mirrors how `gen_responses()` (`npctalk.cpp:1653`) already prunes
responses on conditions.

`load_talk_topic()` (`npctalk.cpp:3694`, registered at `init.cpp:450`) and
`json_talk_topic::check_consistency()` are where a load-time error belongs for a
topic that declares itself monster-usable while carrying NPC-only effects. That
turns a class of author mistake into a startup message rather than a playtest
crash.

## Verification plan

Per `CLAUDE.md`: **a green suite is not evidence.** Specifically here —

- Every new test must be watched failing against the unfixed code first. The trap
  in this feature is a monster dialogue test that passes because the topic never
  loaded and the response list was trivially empty, which is the same shape as
  the collapse-test failure recorded in the 2026-09-05 handoff.
- Assert the precondition rather than creating it: a monster-dialogue test must
  assert `beta->get_npc() == nullptr`, so the test cannot quietly stop testing a
  monster.
- The NPC path is the regression surface that matters. The full existing dialogue
  suite must stay green across the `beta` retype; that is the evidence the seam
  is behaviour-preserving.
- Response gating needs a negative test: an NPC-only effect on a monster topic is
  refused at load, and never offered at runtime.
- Playtest gates both phases. Phase A's whole purpose is the playtest.

## Open questions

Owner decisions, none of which should be made while AFK:

1. **Does Phase B happen at all?** Gated on the Phase A playtest.
2. **Hostile monsters.** Can you talk to something actively attacking you? If
   yes, does the dialogue window let the world tick? This is the single biggest
   design fork and it changes the entry point.
3. **Base game or mod?** Whether any shipped `mtype` gets a `chat_topic`, or
   whether this is purely a modding capability. Affects whether `data/json/`
   changes at all.
4. **Turn cost.** Talking is currently free for NPCs. A free conversation with a
   hostile is an exploit; a costly one is a trap.
5. **Opinion.** `op_of_u` is NPC-only and monsters have `faction_anger` instead
   (`catalua_bindings_creature.cpp` binds `add_faction_anger` /
   `get_faction_anger`). Whether monster dialogue can move faction anger, and
   whether that is the monster analogue of opinion, is a scope decision.

## Notes for whoever picks this up

- `npc::say()` is on `npc`, not `Creature`. `talker_monster` must implement
  speech itself. Do not add `say()` to `Creature` to make this convenient — that
  is a tier-4 edit to a file every upstream change touches.
- `dialogue_win.*` needs nothing. If a change there looks necessary, the talker
  interface is leaking a `name` it should have flattened to a string already.
- The `on_try_monster_interaction` hook returns `allowed` through
  `get_or( "allowed", true )` with `exit_early`. A Lua prototype that forgets to
  return `false` will run its conversation *and* the normal pet menu.
