#include "pocket_destination_menu.h"

#include "character.h"
#include "item.h"
#include "item_contents.h"
#include "item_pocket.h"
#include "output.h"
#include "translations.h"
#include "ui.h"

std::optional<pocket_destination> ask_pocket_destination( Character &who, const item &it,
        const item *exclude )
{
    const std::vector<pocket_destination> destinations = who.pocket_destinations( it, exclude );
    if( destinations.size() < 2 ) {
        return std::nullopt;
    }

    uilist menu;
    menu.title = string_format( _( "Where should the %s go?" ), it.tname() );

    for( const pocket_destination &dest : destinations ) {
        const item_pocket &pocket = dest.container->contents.get_pockets()[dest.pocket_index];
        // Name the pocket by container and remaining room: two pockets on one
        // garment are otherwise indistinguishable in a list.
        menu.addentry( menu.entries.size(), true, MENU_AUTOASSIGN,
                       _( "%1$s - %2$s free" ),
                       dest.container->tname(),
                       format_volume( pocket.remaining_volume() ) );
    }

    menu.query();
    if( menu.ret < 0 || static_cast<size_t>( menu.ret ) >= destinations.size() ) {
        return std::nullopt;
    }

    return destinations[menu.ret];
}
