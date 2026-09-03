#include "ui_mouse.h"

#include <algorithm>

namespace ui_mouse
{

auto hovered_entry( input_context &ctx, const catacurses::window &win,
                    const column_options &opts ) -> std::optional<int>
{
    if( opts.entry_height < 1 || opts.count <= 0 ) {
        return std::nullopt;
    }
    const auto cell = ctx.get_mouse_cell( win );
    if( !cell ) {
        return std::nullopt;
    }
    const auto row = cell->y - opts.origin.y;
    if( row < 0 ) {
        return std::nullopt;
    }
    const int idx = opts.offset + row / opts.entry_height;
    if( idx >= opts.count ) {
        return std::nullopt;
    }
    return idx;
}

auto is_click( const input_context & /*ctx*/, const std::string &action ) -> bool
{
    return action == "SELECT";
}

auto is_hover( const input_context & /*ctx*/, const std::string &action ) -> bool
{
    return action == "MOUSE_MOVE";
}

} // namespace ui_mouse
