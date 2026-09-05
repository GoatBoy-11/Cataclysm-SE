#include <array>
#include <functional>
#include <list>
#include <memory>
#include <string>

#include "behavior.h"
#include "bodypart.h"
#include "character.h"
#include "character_oracle.h"
#include "inventory.h"
#include "item.h"
#include "item_contents.h"
#include "item_pocket.h"
#include "itype.h"
#include "player.h"
#include "make_static.h"
#include "weather.h"

namespace behavior
{

// To avoid a local minima when the character has access to warmth in a shelter but gets cold
// when they go outside, this method needs to only alert when travel time to known shelter
// approaches time to freeze.
status_t character_oracle_t::needs_warmth_badly() const
{
    // Use bodypart::temp_conv to predict whether the Character is "in trouble".
    for( const auto &pr : subject->get_body() ) {
        if( pr.second.get_temp_conv() <= BODYTEMP_VERY_COLD ) {
            return running;
        }
    }
    return success;
}

status_t character_oracle_t::needs_water_badly() const
{
    // Check thirst threshold.
    if( subject->get_thirst() > thirst_levels::parched ) {
        return running;
    }
    return success;
}

status_t character_oracle_t::needs_food_badly() const
{
    // Check hunger threshold.
    if( subject->get_kcal_percent() < 0.5f ) {
        return running;
    }
    return success;
}

/**
 * Does anything held in @p parent's CONTAINER pockets, at any depth, satisfy
 * @p filter?
 *
 * Magazine and gunmod pockets are left unread on purpose: a round in a
 * magazine is part of the magazine, not something the character is carrying
 * separately, and the same reasoning already governs item::accepts_item().
 */
static bool pocketed_item_matches( const item &parent,
                                   const std::function<bool( const item & )> &filter )
{
    for( const item_pocket &pocket : parent.contents.get_pockets() ) {
        if( pocket.definition().type != pocket_type::CONTAINER ) {
            continue;
        }
        for( const item *stored : pocket.all_items_top() ) {
            if( filter( *stored ) || pocketed_item_matches( *stored, filter ) ) {
                return true;
            }
        }
    }
    return false;
}

/**
 * Does @p who carry anything satisfying @p filter?
 *
 * The predicates below used to walk inv_const_slice() and look at front() of
 * each stack - the flat inventory, top level only. Routing gives worn pockets
 * first refusal on everything the character acquires, so that is no longer
 * where a picked-up lighter lives, and an NPC would sit in the cold beside its
 * own firestarter.
 *
 * Worn garments themselves are deliberately not offered to the filter, which
 * matches the behaviour these predicates have always had: a coat already on the
 * character is neither a coat it could put on nor fuel it ought to burn. Their
 * pockets are a different matter, and are searched.
 */
static bool carries_item_matching( const Character &who,
                                   const std::function<bool( const item & )> &filter )
{
    for( const auto &i : who.inv_const_slice() ) {
        const item &candidate = *i->front();
        if( filter( candidate ) || pocketed_item_matches( candidate, filter ) ) {
            return true;
        }
    }
    for( const item * const &worn_item : who.worn ) {
        if( pocketed_item_matches( *worn_item, filter ) ) {
            return true;
        }
    }
    return false;
}

status_t character_oracle_t::can_wear_warmer_clothes() const
{
    const player *p = dynamic_cast<const player *>( subject );
    // Check what is carried for wearable warmer clothes, greedily.
    // Don't consider swapping clothes yet, just evaluate adding clothes.
    const bool found = carries_item_matching( *subject, [p]( const item & candidate ) {
        return candidate.get_warmth() > 0 || p->can_wear( candidate ).success();
    } );
    return found ? running : failure;
}

status_t character_oracle_t::can_make_fire() const
{
    // Check what is carried for firemaking tools and fuel
    bool tool = false;
    bool fuel = false;
    const bool ready = carries_item_matching( *subject, [&tool, &fuel]( const item & candidate ) {
        if( candidate.has_flag( STATIC( flag_id( "FIRESTARTER" ) ) ) ) {
            tool = true;
        } else if( candidate.flammable() ) {
            fuel = true;
        }
        return tool && fuel;
    } );
    return ready ? running : success;
}

status_t character_oracle_t::can_take_shelter() const
{
    // See if we know about some shelter
    // Don't know how yet.
    return failure;
}

status_t character_oracle_t::has_water() const
{
    // Check if we know about water somewhere
    bool found_water = subject->has_item_with( []( const item & cand ) {
        return cand.is_food() && cand.get_comestible()->quench > 0;
    } );
    return found_water ? running : failure;
}

status_t character_oracle_t::has_food() const
{
    // Check if we know about food somewhere
    bool found_food = subject->has_item_with( []( const item & cand ) {
        return cand.is_food() && cand.get_comestible()->has_calories();
    } );
    return found_food ? running : failure;
}

} // namespace behavior
