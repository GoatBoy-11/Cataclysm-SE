# Monster Dialogue — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development
> or superpowers:executing-plans. Steps use checkbox (`- [ ]`) syntax for tracking.
> **Keep the boxes current — the older pocket plans' boxes rotted and CLAUDE.md now
> warns about them.**

**Spec:** `docs/superpowers/specs/2026-09-05-monster-dialogue-design.md` — read it
first. This plan assumes its measurements and does not repeat them.

**Supersedes:** `2026-09-05-monster-dialogue-phase-a.md` (deleted). That plan
proposed a Lua prototype; the owner has since decided monsters use the NPC
dialogue system itself, which Lua cannot construct. It is in git history if
needed.

**Goal:** Monsters hold conversations through the real NPC dialogue system —
same window, same JSON topics, same trials, same conditions — with negotiation
outcomes written to the emotional state the monster AI already obeys.

## How to work this plan

Five milestones. **M1 and M2 are the vertical slice**: after M2 the owner can
playtest a real conversation with a real monster in the real UI. M3–M5 add depth
on top of a shape already validated.

Each milestone is independently committable and leaves the build green. Do not
start a milestone before the previous one's suite is clean.

### Rules that apply to every milestone

- **Watch every new test fail first.** Break the code it covers, see red, restore.
  `CLAUDE.md` is emphatic about this and it has caught real defects here.
- **Assert preconditions, do not manufacture them.** A monster-dialogue test must
  assert `d.beta->get_npc() == nullptr`, so it cannot quietly stop testing a
  monster.
- **The NPC dialogue suite is the regression surface.** `tests/npc_talk_test.cpp`
  is 1,058 lines and must stay green through every milestone. It is the evidence
  the seam is behaviour-preserving.
- Build and test per `CLAUDE.md` (Windows presets; `CATA_TEST_COMPUTE_ACCELERATION=cpu`;
  four vision tests fail environmentally and nothing else may).
- Rotate the exe after every build: `bash .claude/rotate-game-exe.sh`.
- Adding a file to `tests/` requires re-running `cmake --preset cse-msvc`.

### When to stop and ask

Stop and put it to the owner if: a milestone wants a new `talker` virtual not
listed in M1; the NPC suite fails in a way that needs an NPC behaviour change to
fix; or an open question from the spec blocks progress. Do not guess on the five
open questions — M2's playtest exists to answer the first two.

---

## M1 — The talker seam (no behaviour change)

**Deliverable:** `dialogue::beta` is a `talker *`. NPCs behave exactly as before.
No monster can talk yet. This milestone is a pure refactor and is judged entirely
by the existing suite staying green.

### The interface

New file `src/talker.h` (+ `src/talker.cpp` if bodies grow). New file, so tier-2
fork cost per `CLAUDE.md`.

Derive the member list from the spec's table. It should be approximately:

```cpp
class talker
{
    public:
        virtual ~talker() = default;

        // identity and speech
        virtual std::string name() const = 0;
        virtual std::string disp_name() const = 0;
        virtual void say( const std::string &line ) = 0;

        // creature state the dialogue system reads
        virtual bool has_effect( const efftype_id &eff ) const = 0;
        virtual tripoint_bub_ms bub_pos() const = 0;
        virtual tripoint_abs_omt abs_omt_pos() const = 0;
        virtual int get_dimension() const = 0;
        virtual bool is_friendly( const Character &guy ) const = 0;
        virtual bool is_enemy() const = 0;
        virtual void make_angry() = 0;
        virtual std::unordered_set<std::string> get_grammatical_genders() const = 0;

        // the escape hatch — nullptr for monsters
        virtual npc *get_npc() { return nullptr; }
        virtual const npc *get_npc() const { return nullptr; }
};
```

Match the exact signatures to what `npctalk.cpp` and `condition.cpp` actually
call; the compiler will tell you. **Do not add members speculatively.** Adding a
virtual to `talker` needs the same justification as adding a field to `item` —
the spec explains why.

`talker_npc` implements everything by forwarding to `npc &`. It is the only
implementation in M1.

**Include hygiene:** `dialogue.h` already includes `npc.h` and `player.h`.
`talker.h` must forward-declare rather than include `dialogue.h`, or the cycle
bites.

### The retype

- [ ] Create `src/talker.h` with `talker` and `talker_npc`.
- [ ] Change `dialogue::beta` (`dialogue.h:231`) to `talker *`. Update the comment
      — it currently says "The NPC we talk to. Never null."
- [ ] Fix the fallout in `npctalk.cpp` and `condition.cpp`. The compiler finds all
      79 sites. For each, choose deliberately:
      - Generic state → call the `talker` virtual.
      - NPC-only (`chatbin`, `rules`, `op_of_u`, `value`, `get_faction`,
        `wield_better_weapon`, `getID`, `myclass`, missions) → route through
        `get_npc()`.
      - **Every `get_npc()` in the hardcoded chain gets a null guard**, even
        though M1 cannot reach it with a monster. M2 can.
- [ ] `npc::talk_to_u()` (`npctalk.cpp:1094`) constructs a `talker_npc` and points
      `d.beta` at it. Mind the lifetime — the talker must outlive the dialogue
      loop; a stack local in `talk_to_u` is correct and simplest.
- [ ] Leave `talkfunction_ptr`, `dialogue_fun_ptr` and the private
      `add_response` overloads typed to `npc &`. They are only used by the
      hardcoded chain, which monsters never reach. Retyping them is scope creep.
- [ ] `talk_effect_t::apply()` applies `npc_opinion` to `op_of_u`. Guard it on
      `get_npc()` so it no-ops for a monster.

### Defect fixes that belong here

Both are in code M1 touches. Each needs a test watched failing first.

- [ ] **`repeat_responses` null-deref** (`npctalk.cpp:3646`):
      `dynamic_cast<player *>( d.beta )` with no null check before
      `actor->charges_of(...)`. Guard it. Under M1 this is latent; under M2 it is
      a crash.
- [ ] **`parse_mod` `NPC_INTIMIDATE`** (`npctalk.cpp:1791`) computes
      `intimidation( u )` — the player — identical to `U_INTIMIDATE` above it.
      Should be `p`. Upstream BN's bug; fix it rather than inherit it.

### Verification

- [ ] Full `tests/npc_talk_test.cpp` green. **This is the milestone's proof.**
- [ ] Full suite: expect ~1,131 cases with only the four documented vision
      failures.
- [ ] A test asserting `talker_npc::get_npc()` returns the wrapped NPC, and that
      a `dialogue` built the old way still resolves NPC-only topics. Watch it fail
      by making `get_npc()` return `nullptr`.
- [ ] Playtest: hold an ordinary NPC conversation, including a mission and a
      trade. Nothing should feel different. If anything does, M1 is not done.

---

## M2 — A monster that talks (PLAYTEST GATE)

**Deliverable:** one monster, one JSON topic, the real dialogue window. Nothing
persuasive happens yet — the conversation exists and the shape can be judged.

- [ ] `talker_monster` in `src/talker.h`, wrapping `monster &`.
      - `say()` has no `monster` equivalent — implement it with `sounds::sound()`
        at the monster's position, or `add_msg`. **Do not add `say()` to
        `Creature`.**
      - `get_npc()` returns `nullptr`. That is the whole safety model.
- [ ] `mtype::chat_topic` — `std::string`, empty by default, parsed in
      `mtype::load()` (`monstergenerator.cpp:759`). Empty means "cannot talk",
      which is every existing monster, so this is a no-op for all current content.
- [ ] `monster::talk_to_u()` — mirror `npc::talk_to_u()` (`npctalk.cpp:1094`)
      minus the mission, attitude, radio, follower and needs blocks. Roughly 30
      lines: build the dialogue, push `type->chat_topic`, run the same loop,
      fire the same `on_dialogue_*` Lua hooks.
      - The loop's `d.beta->say( _( "Bye." ) )` (`npctalk.cpp:1273`) now goes
        through the talker, so it works unchanged.
- [ ] Entry point in `game::examine()` (`game.cpp:8602`), where the NPC path
      already lives. Offer `Talk` only when `chat_topic` is non-empty.
      - **The mute tier must not reach this.** No `chat_topic` means no window —
        it is a reaction, not a conversation. Per the spec, that path is a message
        plus a `SOUND` trigger, but M2 need not implement the reaction; just do
        not open a window.
      - Hostile monsters: **leave gated off for M2** unless the owner has answered
        open question 1. The playtest is what answers it.
- [ ] **Enforce `replace_built_in_responses`.** A topic reachable by a monster
      must set it, or `json_talk_topic::gen_responses()` returns false
      (`npctalk.cpp:3670`) and execution falls into the hardcoded NPC chain. Belt
      and braces:
      1. `monster::talk_to_u` refuses (with a `debugmsg`) a topic that does not
         set it;
      2. `check_consistency()` reports it at load.
- [ ] One test monster and one JSON topic under `data/mods/TEST_DATA/`, plus one
      shipped example if the owner has answered open question 3.

### Verification

- [ ] Tests in a new `tests/monster_talk_test.cpp` (remember the `cmake --preset`
      re-run), modelled on `tests/npc_talk_test.cpp`'s harness:
      - A monster with a `chat_topic` produces responses from its JSON topic.
      - **Asserts `d.beta->get_npc() == nullptr`** so it cannot stop testing a
        monster.
      - A monster topic without `replace_built_in_responses` is refused, not
        crashed into.
      - A monster with an empty `chat_topic` offers no Talk entry.
      - Watch each fail first. The specific trap: a test that passes because the
        topic never loaded and the response list was trivially empty. Assert a
        named response is present, not merely that nothing threw.
- [ ] NPC suite still green.
- [ ] **PLAYTEST GATE.** Hand the owner a build and these questions:
      1. Does it feel like NPC dialogue, or a menu bolted onto a monster?
      2. Should you be able to talk to a hostile mid-fight? Does the world tick?
      3. Should talking cost a turn?
      4. Does a talking zombie stop being a zombie? *If yes, stop here and say so
         — that is a successful outcome, not a failure.*
      5. Should any base-game monster get this, or is it modding capability only?

**Do not start M3 until the owner has answered.** The answers change M3 and M4.

---

## M3 — Speech checks against monsters

**Deliverable:** `talk_trial` works with a monster on the other side.

- [ ] Add `virtual int talk_skill() const` and `virtual int intimidation() const`
      to `talker`.
      - `talker_npc` forwards to `character_effects::talk_skill( npc )` /
        `intimidation( npc )` (`character_effects.cpp:133`), preserving today's
        numbers exactly.
      - `talker_monster` substitutes from monster state: `type->difficulty`,
        `anger`, `agro`. Propose the formula to the owner before tuning it; the
        shape matters more than the constants.
- [ ] `talk_trial::calc_chance()` (`npctalk.cpp:1818`) uses those virtuals for the
      opposed side. `op_of_u.trust` / `.fear` / `personality.bravery` terms apply
      only when `get_npc()` is non-null; monsters use anger and morale instead.
- [ ] `parse_mod` (`npctalk.cpp:1784`) gains monster attributes — `ANGER`,
      `MORALE`, `AGGRO`, `DIFFICULTY` — alongside the NPC ones, each guarded on
      speaker kind.

### Verification

- [ ] Trial-chance tests for each of LIE / PERSUADE / INTIMIDATE against a
      monster, watched failing first.
- [ ] A regression test pinning NPC trial chances to their current values, so the
      refactor cannot silently move NPC difficulty. Compute expected values from
      the formulas in `calc_chance`, not from whatever the code returns.

---

## M4 — Monster effect verbs and gating

**Deliverable:** a conversation can change what the monster does.

- [ ] New `talk_effect_fun_t` setters writing monster state, following the
      existing `set_*` pattern (`dialogue.h:100`) and reading `d.beta`:
      `mon_add_anger`, `mon_add_morale`, `mon_make_friendly`, `mon_make_pet`.
      The spec's outcome model: a successful negotiation is `anger -= 15`,
      because `monster::attitude()` (`monster.cpp:1809`) already obeys that.
- [ ] **Offerings**, the barter replacement. `u_consume_item` already touches only
      the player (`npctalk.cpp:2613`), so an offering is that plus a monster
      effect. No new item-transfer code, no monster inventory.
- [ ] **The `speaker` gate.** A `speaker` field on `json_talk_response` marking a
      response NPC-only or monster-usable, applied at *selection* time in
      `json_talk_response::gen_responses` so an unusable option is never shown.
      Default must keep every existing NPC response working unchanged.
- [ ] **Load-time validation** in `check_consistency()`: a topic declared
      monster-usable carrying an NPC-only effect from `static_functions_map`
      (`npctalk.cpp:3142`) is a startup error, not a playtest crash.

### Verification

- [ ] An effect test per verb, asserting the monster's `attitude()` actually
      changes across a threshold — not merely that the int moved. That is what
      makes the outcome real.
- [ ] A gating test: an NPC-only response is absent from a monster's response
      list, and present for an NPC.
- [ ] A load-time test for the invalid-topic error.
- [ ] Full suite green; NPC dialogue unchanged.

---

## M5 — Content and the tiers

**Deliverable:** the sapience tiers exist as authoring convention, with enough
content to play.

- [ ] The mute-tier reaction: talking at a monster with no `chat_topic` costs a
      turn, prints flavour, fires a `SOUND` anger trigger.
- [ ] Trait gating on availability (spec: primary balance mitigation), using the
      existing `u_has_trait` condition. Which traits is open question 4.
- [ ] Reactive tier: one or two animals — calm / threaten / feed.
- [ ] Semantic tier: one robot using `TALK_TRIAL_CONDITION` for authority rather
      than persuasion.
- [ ] Sapient tier: one hand-written entity, ideally a named unique.
- [ ] Lint JSON with the VS tree's `json_formatter.exe` (`CLAUDE.md` — it does not
      link under Ninja).
- [ ] Document the tier conventions in `docs/en/mod/json/` so mod authors can use
      the system.

**Content budget: tiers are conventions, never C++ branches.** If a tier seems to
need a code change, it is a sign the mechanism is wrong — re-read the spec's
Part 2 before writing the branch.

---

## Rollback

Each milestone is one commit and independently revertable. M1 is the only one
that touches NPC behaviour; if it goes wrong, revert it and nothing else is
affected, because M2–M5 all sit on top of it.
