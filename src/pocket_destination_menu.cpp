#include "pocket_destination_menu.h"

#include "character.h"
#include "item.h"
#include "item_contents.h"
#include "item_pocket.h"
#include "output.h"
#include "translations.h"
#include "ui.h"

bool choose_pocket_destination( Character &who, item &it, const item *exclude )
{
    const std::vector<pocket_destination> destinations = who.pocket_destinations( it, exclude );
    if( destinations.size() < 2 ) {
        return false;
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
        return false;
    }

    const pocket_destination &chosen = destinations[menu.ret];
    detached_ptr<item> moved = it.detach();
    const std::string moved_name = moved->tname();
    const std::string container_name = chosen.container->tname();

    ret_val<bool> inserted =
        chosen.container->contents.insert_into( chosen.pocket_index, std::move( moved ) );
    if( !inserted.success() ) {
        // The item is still in `moved`; hand it back rather than lose it.
        who.i_add( std::move( moved ) );
        popup( _( "The %1$s will not go in there: %2$s" ), moved_name, inserted.str() );
        return false;
    }

    who.add_msg_if_player( _( "You put the %1$s in your %2$s." ), moved_name, container_name );
    return true;
}
