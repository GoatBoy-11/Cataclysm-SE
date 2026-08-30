# Pocket Favorites and Organization UI Plan

**Goal:** Port CDDA's per-pocket player settings and the menu that edits them, so
players can direct where items go rather than accepting `best_pocket()`'s guess.

**Blocked by:** enforcement (`phase-5-pocket-enforcement`) and the restriction
work through `62db9b4856`. All landed.

**Spec:** `docs/superpowers/specs/2026-08-29-pocket-system-design.md`, which lists
the organization menu as classic mode's third relaxation.

## What this actually is

Reading CDDA's `item_pocket::favorite_settings` (src/item_pocket.h:64), the
feature is not a menu with a data model behind it - it is a data model, saved per
pocket per item instance, with a menu on top:

- `priority_rating` - which pocket `best_pocket()` should prefer
- item whitelist / blacklist, by `itype_id`
- category whitelist / blacklist, by `item_category_id`
- `collapsed` - hide contents in the inventory view
- `disabled` - never auto-pick-up into this pocket
- `unload` - whether normal unloading empties it
- named presets, shareable between pockets

That means it touches save format, `best_pocket()`, the inventory UI, and
auto-pickup. It is the largest remaining piece of the port by some margin.

## Global Constraints

- **Settings are per item instance, not per itype.** Two backpacks must be able
  to have different priorities. They therefore serialize with the item, and the
  save format changes again - the pocket save block gains an optional settings
  object, absent when the player has not edited anything.
- **Absent settings must serialize to nothing.** Most pockets on most items will
  never be edited; writing an empty object per pocket would bloat every save.
- **Classic mode hides the menu entirely** and ignores the settings, per the
  spec's third relaxation. Settings already saved stay saved.
- **`best_pocket()` consults priority before its existing rules**, so a player
  choice beats the tightest-fit heuristic.
- Build: `cmake --build out/build/cse-vcpkg --config RelWithDebInfo --parallel 6`
- Tests: `out/build/cse-vcpkg/tests/RelWithDebInfo/cata_test-tiles.exe`
- Judge the suite by pass/fail and case count, never assertion count.

---

### Task 1: The settings data model

**Files:** `src/item_pocket.{h,cpp}`, `tests/item_pocket_test.cpp`

- [x] `pocket_favorite_settings` with CDDA's fields and accessors.
- [x] `accepts_item()`: whitelist wins over blacklist, empty lists accept all,
      matching CDDA's precedence exactly - verify against their implementation
      rather than assuming.
- [x] `is_null()` true when untouched, so serialization can skip it.
- [x] Tests: whitelist admits and excludes; blacklist excludes; category rules
      work; an untouched settings object is null.

### Task 2: Saving and loading settings

**Files:** `src/savegame_json.cpp`, `tests/item_pocket_test.cpp`

- [x] Serialize settings inside each pocket's save object, omitted when null.
- [x] Deserialize tolerantly: a pocket without settings is untouched, and a save
      written before this phase must load unchanged.
- [x] No `savegame_version` bump needed: the format reads tolerantly only if the format cannot be read tolerantly.
- [x] Tests: settings survive a round trip; a pre-settings save still loads; an
      unedited item's save block gains no bytes.

### Task 3: best_pocket() honours priority and filters

**Files:** `src/item_contents.cpp`, `tests/item_pocket_test.cpp`

- [x] Skip pockets whose settings reject the item.
- [x] Prefer higher priority before the existing restricted/tightest-fit rules.
- [x] Skip disabled pockets for automatic insertion, while still allowing
      deliberate placement into them.
- [x] Tests: a whitelisted pocket wins over a tighter one; a blacklisted pocket
      is skipped; priority beats tightest fit; classic mode ignores all of it.

### Task 4: The menu

**Files:** `src/item_contents.{h,cpp}`, `src/item.{h,cpp}`, an inventory entry point

- [x] `item_contents::favorite_settings_menu()` listing pockets with their
      capacity, contents and current settings. BN has no item_location, and its
      uilist is keyboard-driven rather than CDDA's ImGui, so this is a port in
      behaviour rather than in code.
- [x] Editing priority, whitelists, blacklists, collapse, disable, unload. One
      key cycles a rule none -> allowed -> barred, which suits a list menu
      better than CDDA's separate whitelist and blacklist modes.
- [x] Reachable from the inventory screen; absent in classic mode. New
      `ORGANIZE_POCKETS` keybinding, bound to P.
- [ ] Presets last, and only if the rest is working - they are convenience, not
      function.

### Task 5: Verification

- [ ] Full suite green; case count up by the new tests.
- [ ] Human playtest: set a priority and confirm items follow it; whitelist a
      pocket and confirm nothing else enters it; save, reload, confirm settings
      persist; switch to classic and confirm the menu is gone and ignored.

## Out of scope

- Auto-pickup integration beyond respecting `disabled`.
- Sharing presets between saves.
- Any change to how pockets themselves are defined; this phase adds player
  preference on top of existing pockets, nothing more.

---

## Found along the way: two items that could not hold their own contents

Not planned work, but the enforcement detector had been logging ~3,800 data
errors per suite run, and two of them were live bugs rather than noise:

- [x] **Sheaths, holsters and bandoliers had no pocket at all.** Their capacity
      lives in a `holster` or `bandolier` use_action, which no synthesis branch
      read, so once insertion was enforced a sheath refused the very knife it
      exists to carry. `synthesize_use_action_pockets_from_legacy()` now derives
      the pocket from the action, which is what CDDA converted these actions
      into outright.
- [x] **Guns refused their own built-in mods.** The M240's bipod is fitted at
      the factory and the gun lists no underbarrel slot, so a mod pocket keyed
      on locations could never accept it. Built-in and default mods now get a
      pocket naming exactly them, leaving what the player may install untouched.

## The data errors, run to ground

The `put_in_expected` reports were ~3,800 a suite run and the test binary had
been exiting non-zero the whole time because of them - "treating result as
failure due to error logged during initialization" - which also masked any real
init error. They are now zero, and the binary exits clean.

Making the report say *why* and *by how much* was what turned this from guesswork
into a list. None of the causes were pockets being too small; every one was
structural:

- [x] Sheaths, holsters, bandoliers: capacity lived in a use_action nothing read.
- [x] Guns refusing their own built-in mods.
- [x] Seven tools whose `magazines` list omitted a cell their own professions
      hand them. CDDA allows any light battery via `flag_restriction`; BN's
      explicit lists had simply drifted from its own content.
- [x] `camera_pro` charges internally and has no magazine well at all, yet two
      professions told it to *contain* a cell. Split into camera + spare cell.
- [x] A pistol lanyard offered to a one-item holster, because `contents-item`
      attaches to the outermost item - the holster - when `container-item` is
      also given. CDDA orders it the same way, so the code was left alone and
      the three profession entries give the lanyard on its own.
- [x] `usb_drive` had no pocket, though `software` names it as its container.
      Given a 10 ml one; CDDA models this as an E_FILE_STORAGE pocket, which BN
      has no equivalent for.
- [x] Item-group contents that do not fit are kept without a report, and the
      pocket audit records the miss instead. A group cannot know what its own
      other rolls put in the same item, so this is not a fault in either
      definition. CDDA *drops* the item here, which suits CDDA's data - but BN's
      professions run through the same code, and silently deleting a loadout's
      chem tank is worse than an overfull pouch. Same reasoning, same treatment,
      for a stack spawned into its default container: a `box_small` holds one
      portion of fried fish by definition and a spawn can be ten.
- [x] A caliber conversion or magazine adapter rewrites which magazines its gun
      takes, but the pocket was synthesized from the gun's definition, which
      predates any mod - so a converted Luty refused the magazine its conversion
      gave it. Both magazine pocket kinds now ask `magazine_compatible()`, the
      same question asked of the gun as it *is*. Internal-clip guns matter here
      too: the handmade carbine never had a well at all, so its BAR adapter's
      magazine had nowhere in the gun to go.
- [x] `put_in_unchecked` and every `TODO(pocket-enforcement)` are gone. Its last
      caller was rain collection, where capacity is measured first, so a refusal
      there means the container is not one water belongs in and the rain runs
      off. The 59 test call sites became `put_in_expected`.

## Still open

- Presets, which CDDA shares between pockets. Convenience, not function.
- CDDA's `better_pocket()` also ranks by spoil multiplier, watertightness,
  extra encumbrance and ripoff chance. BN has the first two; the rest have no
  BN equivalent yet. Worth a pass once `volume_encumber_modifier` lands.
