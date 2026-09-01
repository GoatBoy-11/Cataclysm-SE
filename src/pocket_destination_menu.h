#pragma once

#include <optional>

#include "character.h"

class Character;
class item;

/**
 * Ask which of the worn pockets that would take @p it it should go into.
 *
 * Query only: never touches @p it or the world, so it is safe to call before
 * the caller has committed to moving the item anywhere. Returns the chosen
 * destination, or std::nullopt when the player escaped the menu or there
 * were fewer than two destinations to choose between - in which case there
 * is nothing to ask.
 */
std::optional<pocket_destination> ask_pocket_destination( Character &who, const item &it,
        const item *exclude = nullptr );
