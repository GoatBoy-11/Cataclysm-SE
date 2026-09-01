#pragma once

class Character;
class item;

/**
 * Ask which pocket an item should go into, and put it there.
 *
 * Returns true when the item moved. Returns false without prompting when the
 * player has no real choice: classic mode, or fewer than two destinations.
 */
bool choose_pocket_destination( Character &who, item &it, const item *exclude = nullptr );
