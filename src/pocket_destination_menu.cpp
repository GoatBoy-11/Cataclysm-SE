#include "pocket_destination_menu.h"

#include "character.h"
#include "item.h"
#include "item_contents.h"
#include "item_pocket.h"
#include "output.h"
#include "translations.h"
#include "ui.h"

detached_ptr<item> choose_pocket_destination( Character &who, detached_ptr<item> &&it,
        const item *exclude )
{
    const std::vector<pocket_destination> destinations = who.pocket_destinations( *it, exclude );
    if( destinations.size() < 2 ) {
        return std::move( it );
    }

    uilist menu;
    menu.title = string_format( _( "Where should the %s go?" ), it->tname() );

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
        return std::move( it );
    }

    const pocket_destination &chosen = destinations[menu.ret];
    const std::string moved_name = it->tname();
    const std::string container_name = chosen.container->tname();

    ret_val<bool> inserted =
        chosen.container->contents.insert_into( chosen.pocket_index, std::move( it ) );
    if( !inserted.success() ) {
        // insert_into() only moves out of `it` on success, so on failure the
        // item is still ours to hand back.
        popup( _( "The %1$s will not go in there: %2$s" ), moved_name, inserted.str() );
        return std::move( it );
    }

    who.add_msg_if_player( _( "You put the %1$s in your %2$s." ), moved_name, container_name );
    return detached_ptr<item>();
}
