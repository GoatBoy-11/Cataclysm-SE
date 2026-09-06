# Monster Dialogue — Phase A (Lua prototype)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development
> or superpowers:executing-plans. Steps use checkbox (`- [ ]`) syntax for tracking.
> **Keep the boxes current — the older pocket plans' boxes rotted and CLAUDE.md now
> warns about them.**

**Spec:** `docs/superpowers/specs/2026-09-05-monster-dialogue-design.md`

**Goal:** A Lua-only mod that lets the player hold a branching conversation with a
monster whose type declares one, so the owner can playtest how monster dialogue
*feels* before any C++ merge debt is taken on. Phase B (the `talker` seam) is
gated on that playtest and is not in scope here.

**Why this is small:** every mechanism already exists and is bound.
`on_try_monster_interaction` fires for **any** monster on the examined tile
(`game.cpp:8624`), before every attitude gate. `UiList` and `query_popup` are
bound (`catalua_bindings_ui.cpp`). `game.add_hook` and `game.mod_runtime` are the
documented idiom (`docs/en/mod/lua/hooks.md`), and
`data/mods/NPC_lua_hook_test/` is a working example of exactly this hook family.
This plan writes content, not engine.

**Non-goals, stated so they are not drifted into:** no C++ edits, no new hooks,
no `mtype` field, no JSON `talk_topic` reuse, no missions, no trade, no
`talk_trial` rolls, no conditions from `condition.cpp`. Phase A is a parallel
dialogue system by design. If a task here starts wanting one of these, stop and
the answer is Phase B.

## Design

### Entry point

`game.add_hook("on_try_monster_interaction", ...)` in `main.lua`.

The hook receives `params.monster`. Returning `false` sets `results.allowed =
false` (`catalua.cpp:755`), which with the call site's `exit_early` both breaks
the hook chain and skips the pet/mech/pay-bot/friendly branches below. So:

- Monster has no dialogue declared → return nothing, the normal menus run.
- Monster has dialogue and the player chose to talk → run the conversation,
  then `return false`.
- Monster has dialogue and the player backed out → `return false` as well, so a
  pet does not then pop its pet menu behind the dialogue the player just closed.

**Do not use the per-mtype `game.monster_functions` route for this.** Its two
menu hooks live inside `monexamine::pet_menu()` (`monexamine.cpp:401`), which is
reached only for a monster with `effect_pet`. It cannot see a hostile. It stays
available as a *second* entry for tamed monsters if the playtest wants one; it is
not the primary.

### Data shape

A Lua table keyed by monster id, each value a table of topics keyed by topic id:

```lua
mod.dialogue = {
  mon_zombie_scientist = {
    start = {
      text = "It focuses on you.  \"...still... the samples...\"",
      responses = {
        { text = "Who were you?",        topic = "who"  },
        { text = "[Leave]",              topic = "DONE" },
      },
    },
    who = { ... },
  },
}
```

Deliberately mirroring the JSON `talk_topic` vocabulary — `dynamic_line` becomes
`text`, `responses[].topic` keeps its name — so Phase A content transcribes into
real `talk_topic` objects rather than being rewritten. `"DONE"` is the sentinel
that closes the window, matching `TALK_DONE`.

Keep the shipped content to **two or three monsters**. This is a feel test, not a
content drop; more monsters cost playtest time and answer nothing extra.

### The conversation loop

A `while` over topics: build a `UiList`, title it with the monster's name, set
`text` to the topic's `text`, add one entry per response, `query()`, follow
`topic`, exit on `"DONE"` or a cancelled query (`query()` returns a negative
value on escape — assert this rather than assuming it).

### Behaviour switches (the whole point of the prototype)

Every one of these is a **mod option the owner can flip mid-playtest**, not a
decision made in this plan. Put them in one `mod.opts` table at the top of
`main.lua`, commented, and mention them in the mod description:

| Switch | Default | Question it answers |
|---|---|---|
| `allow_hostile` | `false` | Can you talk to something actively attacking you? |
| `move_cost` | `100` | Should talking cost a turn? (`0` = free) |
| `hostile_response` | `"anger"` | What a hostile does when addressed: ignore, anger, or listen |
| `require_adjacent` | `true` | Shout across a field, or only face to face? |

`allow_hostile` is the big one; the spec names it as the fork that changes the
Phase B entry point.

## Tasks

- [ ] **Confirm the entry point fires before writing content.** A throwaway hook
      that logs `params.monster:get_name()` and returns nothing, exercised
      against (a) a hostile zombie, (b) a tamed dog, (c) a friendly NPC's tile.
      This is the one assumption the whole plan rests on; verify it first, not
      last.
- [ ] `data/mods/Monster_Dialogue/modinfo.json` — id `monster_dialogue`,
      `"lua_api_version": 2`, `"dependencies": [ "bn" ]`, category `content`.
      Copy the shape from `data/mods/NPC_lua_hook_test/modinfo.json`.
- [ ] `preload.lua` — the `---@class` annotation block and
      `local mod = game.mod_runtime[game.current_mod]`, per the same example.
- [ ] `main.lua` — `mod.opts`, `mod.dialogue`, the conversation loop, and the
      `on_try_monster_interaction` registration.
- [ ] Content for two or three monsters, one of which must be **hostile by
      default**, since that is the case the playtest exists to judge.
- [ ] Run `dprint fmt` (config at `dprint.json`, Lua only — this mod is entirely
      within its `data/mods/**/*.lua` include).
- [ ] Hand the owner the build with the mod present but the switches at their
      defaults, plus the list of switches and where to flip them.

No `cata_test` work. There is no C++ change to cover, and a test asserting a Lua
table's shape would pass for the wrong reason — the exact failure mode
`CLAUDE.md` warns about. **The verification for Phase A is the playtest.**

## Playtest script

Hand these to the owner with the build. The answers are the Phase B gate.

1. Talk to a friendly/tamed talker. Does the dialogue window feel like NPC
   dialogue, or like a menu bolted onto a monster?
2. Flip `allow_hostile` on. Talk to a hostile mid-fight. Does the world tick
   while the window is open? Is it an exploit?
3. With `move_cost = 0`, is free conversation abusable? With `100`, is being
   interrupted mid-sentence unfair?
4. Does a talking zombie stop being a zombie? **This is the question.** If the
   answer is yes, Phase B should not happen, and that is a successful outcome
   for this plan, not a failure.
5. Should any base-game monster get this, or is it modding capability only?

## Traps

- **`return false` is load-bearing and easy to forget.** Omit it and the
  conversation runs *and then* the pet menu opens behind it. The NPC example mod
  gets this right (`force_talk` returns `false` after `talk_to_u()`); copy it.
- **The hook fires while mounted too.** The `else if( u.is_mounted() )` at
  `game.cpp:8646` pairs with `if( mon != nullptr )`, so it is the *no monster
  here* case; the monster branch itself carries no mounted guard, and the
  `!u.is_mounted()` checks live inside the individual `pet_menu` / `mfriend_menu`
  branches the hook runs before. Decide explicitly whether you can chat from
  horseback; do not inherit it by accident.
- **`game.monster_functions` has no existing user in `data/`.** Nothing in the
  tree exercises that registration path, so if the playtest later wants the pet
  route, budget for it being the first thing to shake out bugs in it.
- **`params.prev` and `params.results` are shared across hooks.** Another mod's
  hook on the same event has already run or will. Do not assume ours is the only
  one; read `docs/en/mod/lua/hooks.md` on priority and chaining before relying on
  ordering.
- **Content written here is a transcription source for Phase B, not a throwaway.**
  Keeping the topic/response vocabulary aligned with JSON `talk_topic` costs
  nothing now and saves rewriting every line later.
