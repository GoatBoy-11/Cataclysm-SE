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
