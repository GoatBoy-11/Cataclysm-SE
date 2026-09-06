#include "catch/catch.hpp"
#include "avatar.h"
#include "filesystem.h"
#include "fstream_utils.h"
#include "game.h"
#include "image_viewer.h"
#include "item.h"
#include "player_helpers.h"
#include "state_helpers.h"
#include "type_id.h"
#include "world.h"

#include <filesystem>
#include <string>

namespace
{

auto write_dummy_png( const std::string &path ) -> void
{
    const auto writer = []( std::ostream & stream ) {
        stream << "not-a-real-png-but-the-resolver-only-checks-existence";
    };
    REQUIRE( write_to_file( path, writer, nullptr ) );
}

} // namespace

TEST_CASE( "image_dest_rect_native_centers_without_scaling", "[show_image]" )
{
    const auto dest = get_image_dest_rect( {
        .image_size = point( 100, 50 ),
        .screen_size = point( 200, 200 ),
        .mode = image_display_mode::native
    } );
    REQUIRE( dest );
    CHECK( dest->size == point( 100, 50 ) );
    CHECK( dest->pos == point( 50, 75 ) );
}

TEST_CASE( "image_dest_rect_fullscreen_letterboxes", "[show_image]" )
{
    const auto dest = get_image_dest_rect( {
        .image_size = point( 100, 50 ),
        .screen_size = point( 200, 200 ),
        .mode = image_display_mode::fullscreen
    } );
    REQUIRE( dest );
    CHECK( dest->size == point( 200, 100 ) );
    CHECK( dest->pos == point( 0, 50 ) );
}

TEST_CASE( "image_dest_rect_scale_multiplies_native_size", "[show_image]" )
{
    const auto dest = get_image_dest_rect( {
        .image_size = point( 100, 50 ),
        .screen_size = point( 400, 400 ),
        .mode = image_display_mode::scale,
        .scale = 2.0
    } );
    REQUIRE( dest );
    CHECK( dest->size == point( 200, 100 ) );
    CHECK( dest->pos == point( 100, 150 ) );
}

TEST_CASE( "image_dest_rect_rejects_non_positive_sizes", "[show_image]" )
{
    CHECK( !get_image_dest_rect( {
        .image_size = point( 0, 50 ),
        .screen_size = point( 200, 200 )
    } ) );
    CHECK( !get_image_dest_rect( {
        .image_size = point( 100, 50 ),
        .screen_size = point( 200, 200 ),
        .mode = image_display_mode::scale,
        .scale = 0.0
    } ) );
}

TEST_CASE( "resolve_image_path_rejects_parent_directory", "[show_image]" )
{
    CHECK( !resolve_image_path( "../demo_photo.png" ) );
    CHECK( !resolve_image_path( "folder/../../demo_photo.png" ) );
}

TEST_CASE( "resolve_image_path_finds_core_gfx_images", "[show_image]" )
{
    const auto resolved = resolve_image_path( "demo_photo.png" );
    REQUIRE( resolved );
    CHECK( resolved->find( "demo_photo.png" ) != std::string::npos );
}

TEST_CASE( "resolve_image_path_in_roots_prefers_earlier_root", "[show_image]" )
{
    namespace fs = std::filesystem;
    const auto base = fs::path( g->get_active_world()->info->folder_path() ) / "show_image_path_test";
    const auto first = base / "first" / "images";
    const auto second = base / "second" / "images";
    fs::create_directories( first );
    fs::create_directories( second );
    REQUIRE( fs::is_directory( first ) );
    REQUIRE( fs::is_directory( second ) );

    write_dummy_png( ( first / "mod_photo.png" ).generic_string() );
    write_dummy_png( ( second / "mod_photo.png" ).generic_string() );
    write_dummy_png( ( first / "only_first.png" ).generic_string() );

    const auto roots = std::vector<std::string> { first.generic_string(), second.generic_string() };
    const auto preferred = resolve_image_path_in_roots( "mod_photo.png", roots );
    REQUIRE( preferred );
    CHECK( preferred->find( "first" ) != std::string::npos );

    const auto only_first = resolve_image_path_in_roots( "only_first.png", roots );
    REQUIRE( only_first );
    CHECK( only_first->find( "only_first.png" ) != std::string::npos );

    CHECK( !resolve_image_path_in_roots( "missing.png", roots ) );
}

TEST_CASE( "show_image_returns_false_when_missing", "[show_image]" )
{
    CHECK( !show_image( { .image = "no_such_cse_image_zzz.png" } ) );
}

TEST_CASE( "show_image_returns_true_in_test_mode_when_resolved", "[show_image]" )
{
    CHECK( show_image( { .image = "demo_photo.png" } ) );
}

TEST_CASE( "demo_photograph_registers_show_image_use_action", "[show_image][iuse_actor]" )
{
    clear_all_state();
    REQUIRE( itype_id( "demo_photograph" ).is_valid() );
    auto photo = item::spawn( itype_id( "demo_photograph" ) );
    REQUIRE( photo->get_use( "show_image" ) != nullptr );

    auto &you = get_avatar();
    item &held = *photo;
    you.i_add( std::move( photo ) );
    // use() returns 0 (no charges consumed), so invoke_item reports false; the contract is that it does not hang.
    you.invoke_item( &held );
}

TEST_CASE( "uses_spritesheet_animation_requires_full_layout", "[show_image]" )
{
    CHECK( !uses_spritesheet_animation( {} ) );
    CHECK( !uses_spritesheet_animation( { .frame_width = 10 } ) );
    CHECK( !uses_spritesheet_animation( { .frame_width = 10, .frame_height = 10 } ) );
    CHECK( uses_spritesheet_animation( { .frame_width = 10, .frame_height = 10, .frame_count = 2 } ) );
}

TEST_CASE( "spritesheet_columns_default_to_horizontal_strip", "[show_image]" )
{
    const auto spec = image_animation_spec {
        .frame_width = 100,
        .frame_height = 50,
        .frame_count = 5,
    };
    CHECK( compute_spritesheet_columns( 500, spec ) == 5 );
    CHECK( compute_spritesheet_columns( 300, spec ) == 3 );
}

TEST_CASE( "spritesheet_frame_rects_index_left_to_right", "[show_image]" )
{
    const auto spec = image_animation_spec {
        .frame_width = 100,
        .frame_height = 50,
        .frame_count = 5,
        .columns = 2,
    };
    const auto first = get_spritesheet_frame_rect( 200, 100, spec, 0 );
    const auto second = get_spritesheet_frame_rect( 200, 100, spec, 1 );
    const auto third = get_spritesheet_frame_rect( 200, 100, spec, 2 );
    REQUIRE( first );
    REQUIRE( second );
    REQUIRE( third );
    CHECK( *first == spritesheet_frame_rect { .pos = point( 0, 0 ), .size = point( 100, 50 ) } );
    CHECK( *second == spritesheet_frame_rect { .pos = point( 100, 0 ), .size = point( 100, 50 ) } );
    CHECK( *third == spritesheet_frame_rect { .pos = point( 0, 50 ), .size = point( 100, 50 ) } );
    CHECK( !get_spritesheet_frame_rect( 200, 100, spec, 5 ) );
}

TEST_CASE( "advance_image_animation_loops_with_uniform_duration", "[show_image]" )
{
    const auto spec = image_animation_spec {
        .frame_width = 1,
        .frame_height = 1,
        .frame_count = 3,
        .loop = true,
    };
    auto state = image_animation_state {};
    advance_image_animation( state, spec, 250, 100 );
    CHECK( state.frame_index == 2 );
    CHECK( state.ms_into_frame == 50 );
    advance_image_animation( state, spec, 50, 100 );
    CHECK( state.frame_index == 0 );
    CHECK( state.ms_into_frame == 0 );
}

TEST_CASE( "advance_image_animation_holds_last_frame_when_not_looping", "[show_image]" )
{
    const auto spec = image_animation_spec {
        .frame_width = 1,
        .frame_height = 1,
        .frame_count = 2,
        .loop = false,
    };
    auto state = image_animation_state {};
    advance_image_animation( state, spec, 250, 100 );
    CHECK( state.frame_index == 1 );
    advance_image_animation( state, spec, 250, 100 );
    CHECK( state.frame_index == 1 );
}

TEST_CASE( "show_image_accepts_spritesheet_options_in_test_mode", "[show_image]" )
{
    CHECK( show_image( {
        .image = "test_photo_sheet.png",
        .animation = {
            .frame_width = 320,
            .frame_height = 240,
            .frame_count = 3,
            .frame_duration = 120,
        }
    } ) );
}
