#include "catch/catch.hpp"

#include "point.h"
#include "ui_mouse.h"

TEST_CASE( "ui_mouse list hit testing respects bounds and scroll", "[ui_mouse]" )
{
    const ui_mouse::list_options opts {
        .origin = point( 2, 5 ),
        .width = 20,
        .entry_height = 1,
        .count = 10,
        .offset = 3,
        .visible_count = 4,
    };

    CHECK( ui_mouse::hit_test_list( point( 2, 5 ), opts ) == 3 );
    CHECK( ui_mouse::hit_test_list( point( 2, 8 ), opts ) == 6 );
    CHECK( !ui_mouse::hit_test_list( point( 1, 5 ), opts ).has_value() );
    CHECK( !ui_mouse::hit_test_list( point( 22, 5 ), opts ).has_value() );
    CHECK( !ui_mouse::hit_test_list( point( 2, 9 ), opts ).has_value() );
}

TEST_CASE( "ui_mouse rectangle and tab hit testing", "[ui_mouse]" )
{
    const std::vector<ui_mouse::indexed_rectangle> regions {
        { inclusive_rectangle<point> { point( 1, 1 ), point( 4, 1 ) }, 7 },
        { inclusive_rectangle<point> { point( 6, 1 ), point( 9, 1 ) }, 9 },
    };

    CHECK( ui_mouse::hit_test_rectangles( point( 3, 1 ), regions ) == 7 );
    CHECK( ui_mouse::hit_test_rectangles( point( 8, 1 ), regions ) == 9 );
    CHECK( !ui_mouse::hit_test_rectangles( point( 5, 1 ), regions ).has_value() );

    const std::vector<std::string> tabs { "Alpha", "Beta", "Gamma" };
    const ui_mouse::tab_options tab_opts {
        .origin = point( 2, 0 ),
        .current_tab = 1,
        .max_width = 80,
    };
    const auto tab_regions = ui_mouse::tab_rectangles( tabs, tab_opts );
    REQUIRE( !tab_regions.empty() );
    CHECK( ui_mouse::hit_test_tabs( tab_regions.front().bounds.p_min, tabs, tab_opts ) == 0 );

    const std::vector<std::string> bracket_tabs { "Page 1", "Page 2" };
    CHECK( ui_mouse::hit_test_bracket_tabs( point( 7, 0 ), bracket_tabs,
            { .origin = point( 7, 0 ) } ) == 0 );
    CHECK( ui_mouse::hit_test_bracket_tabs( point( 16, 0 ), bracket_tabs,
            { .origin = point( 7, 0 ) } ) == 1 );
}

TEST_CASE( "ui_mouse label hit testing ignores color tags and gaps", "[ui_mouse]" )
{
    // Two buttons laid out as query_popup lays them out: positions relative to
    // the message area, offset by the window border when drawn.
    const std::vector<ui_mouse::positioned_label> labels {
        { "<color_light_green>Y</color>es", point( 0, 3 ) },
        { "<color_light_green>N</color>o", point( 5, 3 ) },
    };
    const point origin( 1, 1 );

    const auto regions = ui_mouse::label_rectangles( labels, origin );
    REQUIRE( regions.size() == 2 );

    // "Yes" occupies 3 cells once the color tags are stripped, not 27.
    CHECK( regions[0].bounds.p_min == point( 1, 4 ) );
    CHECK( regions[0].bounds.p_max == point( 3, 4 ) );
    CHECK( regions[1].bounds.p_min == point( 6, 4 ) );
    CHECK( regions[1].bounds.p_max == point( 7, 4 ) );

    CHECK( ui_mouse::hit_test_labels( point( 1, 4 ), labels, origin ) == 0 );
    CHECK( ui_mouse::hit_test_labels( point( 3, 4 ), labels, origin ) == 0 );
    CHECK( ui_mouse::hit_test_labels( point( 6, 4 ), labels, origin ) == 1 );
    CHECK( ui_mouse::hit_test_labels( point( 7, 4 ), labels, origin ) == 1 );

    // The padding between two buttons belongs to neither.
    CHECK( !ui_mouse::hit_test_labels( point( 4, 4 ), labels, origin ).has_value() );
    CHECK( !ui_mouse::hit_test_labels( point( 5, 4 ), labels, origin ).has_value() );
    // Past the last button, and the row above, are misses.
    CHECK( !ui_mouse::hit_test_labels( point( 8, 4 ), labels, origin ).has_value() );
    CHECK( !ui_mouse::hit_test_labels( point( 2, 3 ), labels, origin ).has_value() );
}

TEST_CASE( "ui_mouse label hit testing tolerates empty labels", "[ui_mouse]" )
{
    const std::vector<ui_mouse::positioned_label> labels {
        { "", point( 0, 0 ) },
        { "Ok", point( 2, 0 ) },
    };

    // A zero-width label gets no rectangle at all, so it can never be hit, and
    // the labels after it keep their own indices.
    const auto regions = ui_mouse::label_rectangles( labels, point_zero );
    REQUIRE( regions.size() == 1 );
    CHECK( regions[0].index == 1 );
    CHECK( ui_mouse::hit_test_labels( point( 2, 0 ), labels, point_zero ) == 1 );
    CHECK( !ui_mouse::hit_test_labels( point( 0, 0 ), labels, point_zero ).has_value() );
}

TEST_CASE( "ui_mouse column hit testing splits on separators", "[ui_mouse]" )
{
    // The color manager's own layout: one separator before the first column and
    // one at x 48, giving two columns.
    const std::vector<int> separators { -1, 48 };

    CHECK( ui_mouse::hit_test_columns( 0, separators ) == 1 );
    CHECK( ui_mouse::hit_test_columns( 47, separators ) == 1 );
    // The separator cell itself still belongs to the column left of it.
    CHECK( ui_mouse::hit_test_columns( 48, separators ) == 1 );
    CHECK( ui_mouse::hit_test_columns( 49, separators ) == 2 );
    CHECK( ui_mouse::hit_test_columns( 200, separators ) == 2 );
    // Left of the first column is no column at all.
    CHECK( !ui_mouse::hit_test_columns( -1, separators ).has_value() );
    CHECK( !ui_mouse::hit_test_columns( 0, {} ).has_value() );
}
