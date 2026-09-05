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
 * destination, or std::nullopt when the player escaped the menu or no pocket
 * would take the item at all. A lone destination is returned without asking:
 * a list of one is not a choice, and refusing it left the callers offering a
 * move that then did nothing.
 */
std::optional<pocket_destination> ask_pocket_destination( Character &who, const item &it,
        const item *exclude = nullptr );
