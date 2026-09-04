#include "ui_mouse.h"

#include <algorithm>
#include <climits>

#include "catacharset.h"
#include "output.h"

namespace ui_mouse
{

auto hit_test_list( point cell, const list_options &opts ) -> std::optional<int>
{
    if( opts.count <= 0 || opts.entry_height <= 0 ) {
        return std::nullopt;
    }
    if( cell.y < opts.origin.y ) {
        return std::nullopt;
    }
    if( opts.width > 0 && ( cell.x < opts.origin.x || cell.x >= opts.origin.x + opts.width ) ) {
        return std::nullopt;
    }
    const int row = cell.y - opts.origin.y;
    const int visible_rows = std::min( opts.count, opts.visible_count ) * opts.entry_height;
    if( row < 0 || row >= visible_rows ) {
        return std::nullopt;
    }
    const int idx = opts.offset + row / opts.entry_height;
    if( idx < 0 || idx >= opts.count ) {
        return std::nullopt;
    }
    return idx;
}

auto hit_test_rectangles( point cell, std::span<const indexed_rectangle> regions ) ->
std::optional<int>
{
    for( const auto &region : regions ) {
        if( region.bounds.contains( cell ) ) {
            return region.index;
        }
    }
    return std::nullopt;
}

auto tab_rectangles( const std::vector<std::string> &labels, const tab_options &opts ) ->
std::vector<indexed_rectangle>
{
    std::vector<indexed_rectangle> result;
    if( labels.empty() ) {
        return result;
    }

    const int tab_step = 3;
    const int max_tab_width = opts.max_width > 0 ? opts.max_width : INT_MAX;

    int total_used_width = 0;
    int current = 0;
    for( size_t i = 0; i < labels.size(); ++i ) {
        total_used_width += utf8_width( labels[i] ) + tab_step;
        if( static_cast<int>( i ) == opts.current_tab ) {
            current = total_used_width;
        }
    }

    int start = 0;
    calcStartPos( start, current, max_tab_width, total_used_width );

    int running = 0;
    int start_i = 0;
    if( start != 0 ) {
        for( size_t i = 0; i < labels.size(); ++i ) {
            running += utf8_width( labels[i] ) + tab_step;
            if( running >= start ) {
                start_i = static_cast<int>( i ) + 1;
                break;
            }
        }
    }

    int x = opts.origin.x;
    for( size_t i = start_i; i < labels.size(); ++i ) {
        const int tab_width = utf8_width( labels[i] ) + 1;
        const int newx = x + utf8_width( labels[i] ) + tab_step;
        if( newx <= max_tab_width + opts.origin.x ) {
            result.push_back( {
                inclusive_rectangle<point> {
                    point( x - 1, opts.origin.y ),
                    point( x + tab_width, opts.origin.y + 2 )
                },
                static_cast<int>( i )
            } );
        }
        x = newx;
    }
    return result;
}

auto hit_test_tabs( point cell, const std::vector<std::string> &labels,
                    const tab_options &opts ) -> std::optional<int>
{
    const auto regions = tab_rectangles( labels, opts );
    return hit_test_rectangles( cell, regions );
}

auto bracket_tab_rectangles( const std::vector<std::string> &labels,
                             const bracket_tab_options &opts ) -> std::vector<indexed_rectangle>
{
    std::vector<indexed_rectangle> result;
    int x = opts.origin.x;
    for( size_t i = 0; i < labels.size(); ++i ) {
        const int label_width = utf8_width( labels[i] );
        const int tab_width = 1 + label_width + 1;
        result.push_back( {
            inclusive_rectangle<point> {
                point( x, opts.origin.y ),
                point( x + tab_width, opts.origin.y )
            },
            static_cast<int>( i )
        } );
        x += tab_width + opts.separator_width;
    }
    return result;
}

auto hit_test_bracket_tabs( point cell, const std::vector<std::string> &labels,
                            const bracket_tab_options &opts ) -> std::optional<int>
{
    const auto regions = bracket_tab_rectangles( labels, opts );
    return hit_test_rectangles( cell, regions );
}

auto subtab_rectangles( const std::vector<std::string> &labels,
                        const subtab_options &opts ) -> std::vector<indexed_rectangle>
{
    std::vector<indexed_rectangle> result;
    int x = opts.origin.x;
    for( size_t i = 0; i < labels.size(); ++i ) {
        const int tab_width = utf8_width( labels[i] ) + opts.tab_step;
        result.push_back( {
            inclusive_rectangle<point> {
                point( x, opts.origin.y ),
                point( x + tab_width, opts.origin.y + opts.height )
            },
            static_cast<int>( i )
        } );
        x += tab_width;
    }
    return result;
}

auto hit_test_subtabs( point cell, const std::vector<std::string> &labels,
                       const subtab_options &opts ) -> std::optional<int>
{
    const auto regions = subtab_rectangles( labels, opts );
    return hit_test_rectangles( cell, regions );
}

auto label_rectangles( std::span<const positioned_label> labels, point origin ) ->
std::vector<indexed_rectangle>
{
    std::vector<indexed_rectangle> result;
    for( size_t i = 0; i < labels.size(); ++i ) {
        // Measure the drawn width: color tags take up no screen cells.
        const int width = utf8_width( labels[i].text, true );
        if( width <= 0 ) {
            continue;
        }
        const point p_min = origin + labels[i].pos;
        result.push_back( {
            inclusive_rectangle<point> { p_min, p_min + point( width - 1, 0 ) },
            static_cast<int>( i )
        } );
    }
    return result;
}

auto hit_test_labels( point cell, std::span<const positioned_label> labels,
                      point origin ) -> std::optional<int>
{
    const auto regions = label_rectangles( labels, origin );
    return hit_test_rectangles( cell, regions );
}

auto hit_test_columns( int x, std::span<const int> separators ) -> std::optional<int>
{
    int result = 0;
    for( size_t i = 0; i < separators.size(); ++i ) {
        if( x > separators[i] ) {
            result = static_cast<int>( i ) + 1;
        }
    }
    return result > 0 ? std::optional<int>( result ) : std::nullopt;
}

} // namespace ui_mouse
