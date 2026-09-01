#pragma once

#include "detached_ptr.h"

class Character;
class item;

/**
 * Ask which pocket an item should go into, and put it there.
 *
 * Takes ownership of @p it. Returns an empty detached_ptr when the item was
 * moved into the chosen pocket. Otherwise the item comes back to the caller
 * unchanged - the caller owns it exactly once, either way - because the
 * player escaped the menu, there were fewer than two destinations to choose
 * between, or the chosen pocket refused it.
 */
detached_ptr<item> choose_pocket_destination( Character &who, detached_ptr<item> &&it,
        const item *exclude = nullptr );
