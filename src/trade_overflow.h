#pragma once

#include <optional>

#include "detached_ptr.h"

class Character;
class item;

/** What to do with an item no worn pocket will hold. */
enum class overflow_choice {
    /** Take it in hand, displacing whatever is wielded. */
    wield,
    /** Put it on. */
    wear,
    /** Keep it loose in the flat inventory. */
    carry,
    /** Set it down where the character stands. */
    drop,
    /** Carry it, and everything else this delivery cannot fit. */
    carry_rest,
    /** Drop it, and everything else this delivery cannot fit. */
    drop_rest,
};

/**
 * Whether @p who should be asked what to do with @p it, rather than handed it
 * loose. False for anyone who cannot answer a menu or is not playing with
 * pockets at all - NPCs, classic mode, a character wearing nothing with a
 * pocket - and for the items routing refuses on principle rather than for want
 * of room, where the question would have no answer.
 */
bool overflow_needs_prompt( const Character &who, const item &it );

/**
 * Carry out @p choice. Cannot lose the item: wielding and wearing hand it back
 * when they fail and a full tile hands it back too, and every such branch falls
 * through to carrying it.
 */
void apply_overflow_choice( Character &who, detached_ptr<item> &&it, overflow_choice choice );

/**
 * Hands over items that came from an NPC - a trade, a quest reward, a gift in
 * dialogue - worn pockets first. Anything no pocket will hold asks what to do
 * with it instead of arriving in the flat inventory unannounced: BN's flat
 * inventory is a compatibility layer rather than storage the player can aim at,
 * and a shotgun that turns up in it with no message reads as a bug.
 *
 * One instance covers one delivery, so a "and the rest too" answer sticks and a
 * trade for ten oversized things asks once rather than ten times.
 */
class trade_overflow
{
    public:
        void deliver( Character &who, detached_ptr<item> &&it );

    private:
        std::optional<overflow_choice> standing;
};
