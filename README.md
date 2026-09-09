# Cataclysm: Slop Edition

<header align="center">
  <a><img src="docs/en/contribute/img/readme-title.png" title="screenshots of (clockwise from upper-right: Chaosvolt (x2), ExecutorBill, scarf005"></a>
</header>

> [!WARNING]
> **This is an AI-generated fork.** Cataclysm: Slop Edition (CSE) is a personal fork of
> [Cataclysm: Bright Nights][bn], with the overwhelming majority of its changes written by a large
> language model. It is not maintained by, endorsed by, or affiliated with the Bright Nights or
> Dark Days Ahead teams. Expect bugs, expect nonsense, expect things the upstream projects
> deliberately chose not to do. Do not report CSE problems to upstream.

Cataclysm: Slop Edition is a roguelike with sci-fi elements set in a post-apocalyptic world.

While some have described it as a "zombie game", there is far more to Cataclysm than that. Struggle
to survive in a harsh, persistent, procedurally generated world. Scavenge the remnants of a dead
civilization for food, equipment, or, if you are lucky, a vehicle with a full tank of gas to get you
the hell out of there.

Fight to defeat or escape from a wide variety of powerful monstrosities, from zombies to giant
insects to killer robots and things far stranger and deadlier, and against the others like yourself,
who want what you have.

Find a way to stop the Cataclysm ... or become one of its strongest monsters.

> Cataclysm: Slop Edition is a fork of [Cataclysm: Bright Nights][bn], which is itself a fork of
> [Cataclysm: Dark Days Ahead][dda]. Nearly everything good here came from them; the rest is mine.
> [See how Bright Nights differs from its own ancestor.](https://docs.cataclysmbn.org/game/changelog/)

[bn]: https://github.com/cataclysmbn/Cataclysm-BN
[dda]: https://github.com/CleverRaven/Cataclysm-DDA

## What CSE adds

Everything below is CSE-only. Anything not listed here behaves as Bright Nights does.

- **Item pockets.** Dark Days Ahead's pocket system, ported to Bright Nights. Containers and
  clothing hold items in real pockets bounded by volume, weight, length and flags, with curated
  pocket data for 114 wearables and synthesized pockets for magazines, mods and corpses. Legacy
  saves are read and converted. Switch it off per world with **Item pocket system**.
- **Pocket handling.** Inventory shows pocket contents nested, items you pick up are routed into
  worn pockets, and pockets can be organised with reusable presets, sealed, or set to preserve.
  **Choosing an item's pocket** adds a prompt at pickup. The organiser is bound to `o` and `P`.
- **Mouse support.** Hover and click across menus, prompts, pickup and trade, plus the X1 and X2
  side buttons. Toggle with **Enable mouse**.
- **Images in game.** Items and Lua scripts can raise a modal image overlay, including animated
  GIFs and spritesheets.
- **New traits.** A set of personality traits not found upstream, among them Coward's Sprint and a
  considerably expanded Anime Protagonist.
- **Imported items.** The Dark Days Ahead wallet family and wallet-sized money, resized ammo boxes,
  assorted small tools, and the item length values that make pocket limits bite.
- **Extra art.** civilian_variety sprites and ChibiUltica sheets grafted into the bundled MSX++
  UnDeadPeople tileset.
- **Faster loading.** Item migrations are applied in a single pass over the item groups, cutting
  roughly 7.5 seconds off loading a world.

## Downloads

### Executables

There are no prebuilt releases. Build from source (see below).

### Launchers

Unsupported. No launcher knows this fork exists.

### Third Party Mods

CSE keeps Bright Nights' data format, so most BN mods should load — including those in the BN
[mod registry](https://mods.cataclysmbn.org/). Compatibility is best-effort and not guaranteed;
breakage is a CSE problem, not a BN one.

CSE stores its saves and settings in a `cataclysm-cse` user directory, separate from Bright Nights,
so both games can be installed side by side without clobbering each other.

### Source Code

[![Source Code][source-badge]][source] [![Zip Archive][clone-badge]][clone]

[source]: https://github.com/GoatBoy-11/Cataclysm-SE/archive/main.zip "The source can be downloaded as a .zip archive"
[source-badge]: https://img.shields.io/badge/Zip%20Archive-black?style=for-the-badge&logo=github
[clone]: https://github.com/GoatBoy-11/Cataclysm-SE/ "clone from the CSE GitHub repo"
[clone-badge]: https://img.shields.io/badge/Clone%20From%20Repo-black?style=for-the-badge&logo=github

#### Building from source

CSE builds the same way Bright Nights does:

- [building with cmake](docs/en/dev/guides/building/cmake.md)
- [building with MSYS2](docs/en/dev/guides/building/msys.md)
- [building with vcpkg](docs/en/dev/guides/building/vs_vcpkg.md)

## Contributing

> Cataclysm: Slop Edition, like Cataclysm: Bright Nights and Cataclysm: Dark Days Ahead before it,
> is developed under the Creative Commons Attribution ShareAlike 3.0 license. The
> code and content of the game is free to use, modify, and redistribute for any purpose whatsoever.
> See http://creativecommons.org/licenses/by-sa/3.0/ for details. Some code distributed with the
> project is not part of the project and is released under different software licenses, the files
> covered by different software licenses have their own license notices.

## Documentation

Gameplay and development documentation lives in the [doc](./docs/) directory in markdown format.
Because CSE keeps Bright Nights' formats, the [Bright Nights docs](https://docs.cataclysmbn.org/)
apply to almost everything here, the CSE-only features above excepted. You can also
[build and serve the documentation locally](./docs/en/contribute/docs.md).

## Frequently Asked Questions

#### Is there a tutorial?

Yes, you can find the tutorial in the **Special** menu at the main menu (be aware that due to many
code changes the tutorial may not function). You can also access documentation in-game via the `?`
key.

#### How can I change the key bindings?

Press the `?` key, followed by the `1` key to see the full list of key commands. Press the `+` key
to add a key binding, select which action with the corresponding letter key `a-w`, and then the key
you wish to assign to that action.

#### How can I start a new world?

**World** on the main menu will generate a fresh world for you. Select **Create World**.

#### There is no music (or sound) in the game. How can I add it?

Find a soundpack such as [Otopack](https://github.com/NarandBD/Otopack-BN-Mk-2), unzip it into the
`sounds/` folder of your user directory, select it in the settings, then restart the game.

#### Where should I put 3rd-party mods?

In the `mods/` folder of your user directory. To find that directory, launch the game, select
**Help**, then **Resolved game directories**, and read the path after "user mods:".

On Windows that is usually `Documents/cataclysm-cse/mods`. On Linux using XDG directories it is
`~/.local/share/cataclysm-cse/mods`.

#### How do I update the game manually?

Delete the old `data` folder, and the `gfx` folder if you want to be safe, then overwrite the old
install with the new build. Deleting `data` first is necessary because overwriting alone will not
account for updates that delete files, as happens with the obsoletion folder.

Never delete your user directory — that is where saves and settings live.

#### I've found a bug. What should I do?

Open an issue on the [CSE issue tracker](https://github.com/GoatBoy-11/Cataclysm-SE/issues/new?template=bug_report.yml).

Do not report it to Bright Nights or Dark Days Ahead. Note that the in-game
`Submit a bug report on github` action still files against the Bright Nights repository, so do not
use it for CSE problems.

#### I would like to make a suggestion. What should I do?

Open an issue on the
[CSE issue tracker](https://github.com/GoatBoy-11/Cataclysm-SE/issues/new?template=feature_request.yml).
