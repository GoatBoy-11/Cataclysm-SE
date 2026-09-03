#pragma once
/// Native (SDL, non-ImGui) mouse helpers for text-UI menus.
///
/// Built on top of `input_context::get_mouse_cell`, which exposes the last
/// mouse position as a window-relative cell. These helpers map that cell onto
/// a column (or row) of menu entries so list-style windows can support
/// hover-highlight + click-to-select without any ImGui dependency.

#include <optional>

#include "input.h"
#include "point.h"

namespace ui_mouse
{

/// Options for hit-testing a vertical column of entries in a window.
struct column_options {
    /// Cell offset inside the window where entry 0 begins (y = first entry row).
    point origin;
    /// Rows per entry (>= 1).
    int entry_height = 1;
    /// Number of entries (>= 0).
    int count = 0;
    /// Scroll offset: index of the entry drawn at `origin`.
    int offset = 0;
};

/// Maps the current mouse position in `win` to a hovered entry index.
/// Returns std::nullopt when no mouse input is pending, the pointer is
/// outside `win`, or it is not over any entry.
auto hovered_entry( input_context &ctx, const catacurses::window &win,
                    const column_options &opts ) -> std::optional<int>;

/// True when the pending input event is a mouse click (SELECT action carrying
/// mouse coordinates, i.e. left button up).
auto is_click( const input_context &ctx, const std::string &action ) -> bool;

/// True when the pending input event is a mouse-move (MOUSE_MOVE action).
auto is_hover( const input_context &ctx, const std::string &action ) -> bool;

} // namespace ui_mouse
