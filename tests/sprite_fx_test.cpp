#include "catch/catch.hpp"
#include "sprite_fx.h"

#include <cstddef>

namespace
{

constexpr auto k_pi_2 = 1.57079632f;

auto row_dx( const sprite_fx_mesh &mesh, const int row, const sprite_fx_rect &dest ) -> float
{
    return mesh.vertices.at( static_cast<std::size_t>( row ) * 2 ).x - dest.x;
}

} // namespace

TEST_CASE( "plant sway amplitude prefers tree then young then shrub", "[sprite_fx]" )
{
    CHECK( plant_sway_amplitude( true, true, true ).value() == Approx( 1.5f ) );
    CHECK( plant_sway_amplitude( false, true, true ).value() == Approx( 1.0f ) );
    CHECK( plant_sway_amplitude( false, false, true ).value() == Approx( 0.55f ) );
    CHECK_FALSE( plant_sway_amplitude( false, false, false ).has_value() );
}

TEST_CASE( "plant sway weather scale is quiet in still air and grows with wind and rain", "[sprite_fx]" )
{
    CHECK( plant_sway_weather_scale( {} ) == Approx( 0.35f ) );
    CHECK( plant_sway_weather_scale( { .windspeed_mph = 40 } ) == Approx( 1.75f ) );
    CHECK( plant_sway_weather_scale( { .windspeed_mph = 80 } ) == Approx( 1.75f ) );
    CHECK( plant_sway_weather_scale( { .windspeed_mph = 40, .precip_rank = 4 } ) == Approx( 2.15f ) );
    CHECK( plant_sway_weather_scale( { .sheltered = true } ) == Approx( 0.175f ) );
    CHECK( plant_sway_weather_scale( { .windspeed_mph = 40, .sheltered = true } ) ==
           Approx( 0.385f ).margin( 0.001f ) );
}

TEST_CASE( "plant sway frame budget maps quality ids", "[sprite_fx]" )
{
    CHECK_FALSE( plant_sway_frame_budget_ms( "off" ).has_value() );
    CHECK( *plant_sway_frame_budget_ms( "8" ) == 125 );
    CHECK( *plant_sway_frame_budget_ms( "30" ) == 33 );
    CHECK( *plant_sway_frame_budget_ms( "60" ) == 16 );
}

TEST_CASE( "make_plant_sway_fx is none without plant flags", "[sprite_fx]" )
{
    const auto fx = make_plant_sway_fx( {} );
    CHECK( fx.kind == sprite_fx_kind::none );
}

TEST_CASE( "sway mesh is empty when fx is none", "[sprite_fx]" )
{
    const auto dest = sprite_fx_rect{ .x = 10.0f, .y = 20.0f, .w = 32.0f, .h = 64.0f };
    const auto mesh = build_sway_mesh( dest, {} );
    CHECK( mesh.vertices.empty() );
    CHECK( mesh.indices.empty() );
}

TEST_CASE( "sway mesh shears the canopy and plants the base", "[sprite_fx]" )
{
    const auto dest = sprite_fx_rect{ .x = 10.0f, .y = 20.0f, .w = 32.0f, .h = 64.0f };
    const auto fx = sprite_fx{
        .kind = sprite_fx_kind::sway,
        .amplitude_px = 4.0f,
        .phase = k_pi_2,
        .time = 0.0f,
    };
    const auto mesh = build_sway_mesh( dest, fx );
    REQUIRE_FALSE( mesh.vertices.empty() );
    REQUIRE( mesh.vertices.size() % 2 == 0 );

    const auto last_row = static_cast<int>( mesh.vertices.size() / 2 ) - 1;
    CHECK( row_dx( mesh, 0, dest ) == Approx( 4.0f ).margin( 0.001f ) );
    CHECK( row_dx( mesh, last_row, dest ) == Approx( 0.0f ).margin( 0.001f ) );
    CHECK( mesh.vertices[0].x == Approx( mesh.vertices[1].x - dest.w ).margin( 0.001f ) );
    CHECK( mesh.vertices[0].y == Approx( dest.y ).margin( 0.001f ) );
    CHECK( mesh.vertices[mesh.vertices.size() - 2].y == Approx( dest.y + dest.h ).margin( 0.001f ) );

    const auto mid_row = last_row / 2;
    CHECK( row_dx( mesh, mid_row, dest ) == Approx( 1.25f ).margin( 0.05f ) );
    CHECK( mesh.vertices.size() > 18 );
}

TEST_CASE( "sway mesh top offset depends on phase", "[sprite_fx]" )
{
    const auto dest = sprite_fx_rect{ .x = 0.0f, .y = 0.0f, .w = 16.0f, .h = 16.0f };
    auto fx = sprite_fx{
        .kind = sprite_fx_kind::sway,
        .amplitude_px = 3.0f,
        .phase = 0.0f,
        .time = 0.0f,
    };
    const auto mesh_zero = build_sway_mesh( dest, fx );
    fx.phase = k_pi_2;
    const auto mesh_peak = build_sway_mesh( dest, fx );

    REQUIRE_FALSE( mesh_zero.vertices.empty() );
    REQUIRE_FALSE( mesh_peak.vertices.empty() );
    CHECK( row_dx( mesh_zero, 0, dest ) == Approx( 0.0f ).margin( 0.001f ) );
    CHECK( row_dx( mesh_peak, 0, dest ) == Approx( 3.0f ).margin( 0.001f ) );
}

TEST_CASE( "plant sway time advances continuously with milliseconds", "[sprite_fx]" )
{
    CHECK( plant_sway_time( 0 ) == Approx( 0.0f ) );
    CHECK( plant_sway_time( 1000 ) == Approx( 4.2f ).margin( 0.001f ) );
    CHECK( plant_sway_time( 500 ) == Approx( 2.1f ).margin( 0.001f ) );
}

TEST_CASE( "glow amplitude is none at zero luminance and clamps when bright", "[sprite_fx]" )
{
    CHECK( glow_amplitude_from_luminance( 0.0f ) == Approx( 0.0f ) );
    CHECK( glow_amplitude_from_luminance( -4.0f ) == Approx( 0.0f ) );
    CHECK( glow_amplitude_from_luminance( 1.0f ) == Approx( 2.0f ) );
    CHECK( glow_amplitude_from_luminance( 8.0f ) == Approx( 3.65f ).margin( 0.01f ) );
    CHECK( glow_amplitude_from_luminance( 1000.0f ) == Approx( 6.0f ) );
}

TEST_CASE( "make_glow_fx is none without amplitude", "[sprite_fx]" )
{
    CHECK( make_glow_fx( 0.0f ).kind == sprite_fx_kind::none );
    const auto fx = make_glow_fx( 3.0f );
    CHECK( fx.kind == sprite_fx_kind::glow );
    CHECK( fx.amplitude_px == Approx( 3.0f ) );
}

TEST_CASE( "make_distortion_fx is none without amplitude", "[sprite_fx]" )
{
    CHECK( make_distortion_fx( { .amplitude_px = 0.0f } ).kind == sprite_fx_kind::none );
    const auto fx = make_distortion_fx( { .elapsed_ms = 0, .x = 1, .y = 2, .amplitude_px = 1.6f } );
    CHECK( fx.kind == sprite_fx_kind::distortion );
    CHECK( fx.amplitude_px == Approx( 1.6f ) );
    CHECK( fx.phase == Approx( plant_sway_phase( 1, 2 ) ) );
}
