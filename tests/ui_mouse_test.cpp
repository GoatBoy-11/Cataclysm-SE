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
