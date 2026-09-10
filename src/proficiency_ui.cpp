#include "proficiency.h"

#include <string>
#include <vector>

#include "character.h"
#include "output.h"
#include "string_formatter.h"
#include "translations.h"
#include "ui.h"

/**
 * A read-only browser over a character's proficiencies: what they know, then
 * what they are part-way through learning, with the description of whichever
 * row is highlighted.
 */
void show_proficiencies_window( const Character &u )
{
    const std::vector<display_proficiency> profs = u.display_proficiencies();

    uilist menu;
    menu.title = _( "Proficiencies" );
    menu.desc_enabled = true;
    menu.text = string_format( _( "Known: %d.  Learning: %d." ),
                               u.known_proficiencies().size(),
                               u.learning_proficiencies().size() );

    if( profs.empty() ) {
        menu.text = _( "You have not picked up any proficiencies yet." );
    }

    int index = 0;
    for( const display_proficiency &prof : profs ) {
        std::string label = prof.id->name();
        if( !prof.known ) {
            // Learning entries carry how far along they are.
            label = string_format( _( "%s  (%d%%)" ), label,
                                   static_cast<int>( prof.practice * 100.0f ) );
        }

        std::string desc = prof.id->description();
        const proficiency_category_id category = prof.id->prof_category();
        if( category.is_valid() ) {
            desc = string_format( _( "%s\n\nCategory: %s" ), desc, category->name() );
        }

        menu.addentry_desc( index, true, MENU_AUTOASSIGN, colorize( label, prof.color ), desc );
        index++;
    }

    // Selecting a row does nothing; only leaving the menu closes it.
    do {
        menu.query();
    } while( menu.ret >= 0 );
}
