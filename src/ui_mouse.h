#pragma once

#include <limits>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "cuboid_rectangle.h"
#include "point.h"

namespace ui_mouse
{

struct indexed_rectangle {
    inclusive_rectangle<point> bounds;
    int index = 0;
};

struct list_options {
    point origin;
    int width = 0;
    int entry_height = 1;
    int count = 0;
    int offset = 0;
    int visible_count = std::numeric_limits<int>::max();
};

struct tab_options {
    point origin = point( 2, 0 );
    int current_tab = 0;
    int max_width = 0;
};

/// Return the index of the bounded, visible list row under `cell`.
auto hit_test_list( point cell, const list_options &opts ) -> std::optional<int>;

/// Return the payload of the first rectangle containing `cell`.
auto hit_test_rectangles( point cell, std::span<const indexed_rectangle> regions ) ->
std::optional<int>;

/// Reproduce `draw_tabs` geometry for mouse hit testing.
auto tab_rectangles( const std::vector<std::string> &labels, const tab_options &opts ) ->
std::vector<indexed_rectangle>;

auto hit_test_tabs( point cell, const std::vector<std::string> &labels,
                    const tab_options &opts ) -> std::optional<int>;

struct bracket_tab_options {
    point origin = point_zero;
    int separator_width = 1;
};

/// Build hit regions for inline `[label]` tab strips such as world page tabs.
auto bracket_tab_rectangles( const std::vector<std::string> &labels,
                             const bracket_tab_options &opts ) -> std::vector<indexed_rectangle>;

auto hit_test_bracket_tabs( point cell, const std::vector<std::string> &labels,
                            const bracket_tab_options &opts ) -> std::optional<int>;

struct subtab_options {
    point origin = point( 2, 0 );
    int tab_step = 3;
    int height = 1;
};

auto subtab_rectangles( const std::vector<std::string> &labels,
                        const subtab_options &opts ) -> std::vector<indexed_rectangle>;

auto hit_test_subtabs( point cell, const std::vector<std::string> &labels,
                       const subtab_options &opts ) -> std::optional<int>;

} // namespace ui_mouse
