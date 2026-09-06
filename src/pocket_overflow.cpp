#include "pocket_overflow.h"

#include <string>

#include "cached_options.h"
#include "character.h"
#include "enums.h"
#include "flag.h"
#include "item.h"
#include "item_contents.h"
#include "item_pocket.h"
#include "activity_handlers.h"
#include "map.h"
#include "output.h"
#include "ret_val.h"
#include "string_formatter.h"
#include "translations.h"
#include "ui.h"

namespace
{

enum overflow_answer {
    OVERFLOW_CANCEL = 0,
    OVERFLOW_WIELD,
    OVERFLOW_WEAR,
    OVERFLOW_CARRY,
    OVERFLOW_DROP,
    OVERFLOW_CARRY_REST,
    OVERFLOW_DROP_REST,
};

/**
 * Whether anything worn offers a usable pocket. Kept in step with the copy in
 * pickup.cpp, which gates the same question for the pickup menu: pocket
 * enforcement only binds a character who actually has pockets, so ungeared
 * characters, NPCs and old content keep the legacy flat stash.
 */
bool wears_usable_pockets( const Character &who )
{
    for( const item *garment : who.worn ) {
        for( const item_pocket &pocket : garment->contents.get_pockets() ) {
            if( pocket.definition().type == pocket_type::CONTAINER &&
                pocket.can_hold_anything() ) {
                return true;
            }
        }
    }
    return false;
}

/**
 * Set the item down where the character stands. Cargo space in a vehicle they
 * are standing in comes first: dropping a rifle just traded for through the
 * floor of the truck you are sitting in is not what "drop" means here. The
 * helper carries its own messaging and handles ownership, and it always places
 * the item somewhere, so nothing comes back.
 */
void drop_here( Character &who, detached_ptr<item> &&it )
{
    put_into_vehicle_or_drop( who, item_drop_reason::too_large, std::move( it ) );
}

} // namespace

bool pocket_capacity_binds( const Character &who )
{
    // Classic mode is the promise that BN's flat inventory still behaves like
    // BN's, and a character wearing nothing with a pocket is not playing with
    // pockets either - enforcing them would leave that character unable to keep
    // anything at all.
    return !pockets_are_classic() && wears_usable_pockets( who );
}

bool overflow_needs_prompt( const Character &who, const item &it )
{
    // Only the player can answer a menu.
    if( !who.is_avatar() || !pocket_capacity_binds( who ) ) {
        return false;
    }
    // Routing turns these two away whatever the room available - a liquid needs
    // a watertight decision the pickup flow owns, and a casing routed into
    // clothing would shadow the gun's own casings pocket. Asking about them
    // would be a question with no useful answer.
    return !it.made_of( LIQUID ) && !it.has_flag( flag_CASING );
}

void apply_overflow_choice( Character &who, detached_ptr<item> &&it, overflow_choice choice )
{
    if( !it ) {
        return;
    }
    switch( choice ) {
        case overflow_choice::wield:
            it = who.wield( std::move( it ) );
            break;
        case overflow_choice::wear:
            it = who.wear_item( std::move( it ) );
            break;
        case overflow_choice::drop:
        case overflow_choice::drop_rest:
            drop_here( who, std::move( it ) );
            return;
        case overflow_choice::carry:
        case overflow_choice::carry_rest:
            break;
    }
    // Wielding and wearing hand the item back when they fail. Carrying it loose
    // is the branch that cannot fail, so it backs up the rest.
    if( it ) {
        who.i_add( std::move( it ) );
    }
}

void trade_overflow::deliver( Character &who, detached_ptr<item> &&it )
{
    if( !it ) {
        return;
    }
    // test_mode covers the suite, which has no one to answer a menu; the
    // predicate itself stays free of it so tests can exercise it directly.
    if( test_mode || !overflow_needs_prompt( who, *it ) ) {
        who.i_add_routed( std::move( it ) );
        return;
    }

    // Worn pockets get first refusal exactly as they do on every other
    // acquisition path; only what they all turn down reaches the menu.
    it = who.i_add_to_worn_pockets( std::move( it ), nullptr, false, false );
    if( !it ) {
        return;
    }

    if( standing ) {
        apply_overflow_choice( who, std::move( it ), *standing );
        return;
    }

    uilist menu;
    menu.text = string_format( _( "No pocket will hold %s." ), it->display_name() );
    if( who.is_armed() ) {
        menu.addentry( OVERFLOW_WIELD,
                       !who.primary_weapon().has_flag( flag_NO_UNWIELD ) && who.can_wield( *it ).success(),
                       'w', _( "Dispose of %s and wield %s" ),
                       who.primary_weapon().display_name(), it->display_name() );
    } else {
        menu.addentry( OVERFLOW_WIELD, who.can_wield( *it ).success(), 'w', _( "Wield %s" ),
                       it->display_name() );
    }
    if( it->is_armor() ) {
        menu.addentry( OVERFLOW_WEAR, who.can_wear( *it ).success(), 'W', _( "Wear %s" ),
                       it->display_name() );
    }
    menu.addentry( OVERFLOW_CARRY, true, 'c', _( "Carry %s in your hands" ), it->display_name() );
    menu.addentry( OVERFLOW_DROP, true, 'd', _( "Drop %s here" ), it->display_name() );
    menu.addentry( OVERFLOW_CARRY_REST, true, 'C',
                   _( "Carry it, and anything else that will not fit" ) );
    menu.addentry( OVERFLOW_DROP_REST, true, 'D',
                   _( "Drop it, and anything else that will not fit" ) );

    menu.query();

    switch( menu.ret ) {
        case OVERFLOW_WIELD:
            apply_overflow_choice( who, std::move( it ), overflow_choice::wield );
            return;
        case OVERFLOW_WEAR:
            apply_overflow_choice( who, std::move( it ), overflow_choice::wear );
            return;
        case OVERFLOW_DROP:
            apply_overflow_choice( who, std::move( it ), overflow_choice::drop );
            return;
        case OVERFLOW_CARRY_REST:
            standing = overflow_choice::carry_rest;
            apply_overflow_choice( who, std::move( it ), overflow_choice::carry_rest );
            return;
        case OVERFLOW_DROP_REST:
            standing = overflow_choice::drop_rest;
            apply_overflow_choice( who, std::move( it ), overflow_choice::drop_rest );
            return;
        case OVERFLOW_CARRY:
        default:
            // Escaping carries it. Dropping on cancel would leave goods the
            // player just paid for on a shop floor they are about to walk away
            // from, which is the surprise this menu exists to remove.
            apply_overflow_choice( who, std::move( it ), overflow_choice::carry );
            return;
    }
}
