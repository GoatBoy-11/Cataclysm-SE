#include "catch/catch.hpp"
#include "fx/emitter.h"
#include "fx/present_grade.h"
#include "fx/system.h"
#include "json.h"
#include "options_helpers.h"
#include "sprite_fx.h"
#include "weather/weather.h"

#include <sstream>
#include <string>

namespace
{

auto load_emitter_json( const std::string &json ) -> void
{
    std::istringstream buffer( json );
    JsonIn jsin( buffer );
    JsonObject jo = jsin.get_object();
    fx_emitter::load_fx_emitter( jo, "test" );
}

} // namespace

TEST_CASE( "fx range-equal particle spawn is deterministic", "[fx]" )
{
    fx_emitter::reset();
    cata_fx::reset();
    load_emitter_json( R"({
        "id": "unit_burst",
        "trigger": "hit",
        "lifetime_ms": [ 250, 250 ],
        "speed": [ 4, 4 ],
        "heading_deg": 90,
        "spread_deg": 0,
        "gravity": [ 0, 10 ],
        "size_px": [ 3, 3 ],
        "burst": 2
    })" );

    REQUIRE( fx_emitter_id( "unit_burst" ).is_valid() );
    cata_fx::spawn( fx_spawn{ .id = fx_emitter_id( "unit_burst" ), .x = 1.0f, .y = 2.0f, .z = 0.0f } );
    REQUIRE( cata_fx::particles().size() == 2 );
    CHECK( cata_fx::particles()[0].x == Approx( 1.0f ) );
    CHECK( cata_fx::particles()[0].y == Approx( 2.0f ) );
    CHECK( cata_fx::particles()[0].vy == Approx( 4.0f ) );
    CHECK( cata_fx::particles()[0].gx == Approx( 0.0f ) );
    CHECK( cata_fx::particles()[0].gy == Approx( 10.0f ) );
    CHECK( cata_fx::particles()[0].life_s == Approx( 0.25f ) );

    fx_emitter::reset();
    cata_fx::reset();
}

TEST_CASE( "fx particle dies after lifetime and falls with gravity", "[fx]" )
{
    auto p = fx_particle{
        .x = 0.0f,
        .y = 0.0f,
        .vx = 0.0f,
        .vy = 0.0f,
        .gx = 0.0f,
        .gy = 10.0f,
        .age_s = 0.0f,
        .life_s = 0.5f,
        .size_px = 2.0f
    };
    REQUIRE( cata_fx::advance_particle( p, 0.1f ) );
    CHECK( p.vy == Approx( 1.0f ) );
    CHECK( p.y == Approx( 0.1f ).margin( 0.001f ) );
    CHECK( cata_fx::advance_particle( p, 0.5f ) == false );
}

TEST_CASE( "single spread_deg is a symmetric cone", "[fx]" )
{
    fx_emitter::reset();
    load_emitter_json( R"({
        "id": "cone_spread",
        "trigger": "hit",
        "lifetime_ms": [ 200, 200 ],
        "speed": [ 4, 4 ],
        "heading_deg": 90,
        "spread_deg": 20,
        "size_px": [ 2, 2 ],
        "burst": 1
    })" );

    const auto &em = fx_emitter_id( "cone_spread" ).obj();
    CHECK( em.spread_deg.min == Approx( -20.0f ) );
    CHECK( em.spread_deg.max == Approx( 20.0f ) );

    fx_emitter::reset();
}

TEST_CASE( "fx particle expands when size growth is set", "[fx]" )
{
    auto p = fx_particle{
        .x = 0.0f,
        .y = 0.0f,
        .life_s = 1.0f,
        .size_px = 4.0f,
        .size_growth_px_s = 20.0f,
    };
    REQUIRE( cata_fx::advance_particle( p, 0.25f ) );
    CHECK( p.size_px == Approx( 9.0f ) );
}

TEST_CASE( "acid rain emitter is separate from water rain", "[fx]" )
{
    fx_emitter::reset();
    load_emitter_json( R"({
        "id": "weather_rain",
        "trigger": "weather",
        "weathers": [ "rain", "thunder" ],
        "lifetime_ms": [ 400, 400 ],
        "size_px": [ 2, 2 ],
        "spawn_rate": 10
    })" );
    load_emitter_json( R"({
        "id": "weather_acid_rain",
        "trigger": "weather",
        "weathers": [ "acid_rain" ],
        "lifetime_ms": [ 500, 500 ],
        "size_px": [ 3, 3 ],
        "spawn_rate": 10,
        "color": [ 170, 220, 70, 170 ]
    })" );

    REQUIRE( fx_emitter_id( "weather_rain" ).is_valid() );
    REQUIRE( fx_emitter_id( "weather_acid_rain" ).is_valid() );
    const auto &rain = fx_emitter_id( "weather_rain" ).obj();
    const auto &acid = fx_emitter_id( "weather_acid_rain" ).obj();
    CHECK( rain.weathers.size() == 2 );
    CHECK( acid.weathers.size() == 1 );
    CHECK( acid.weathers[0] == weather_type_id( "acid_rain" ) );
    CHECK( acid.color_r == 170 );
    CHECK( acid.color_g == 220 );

    fx_emitter::reset();
}

TEST_CASE( "hit blood falls with gravity", "[fx]" )
{
    fx_emitter::reset();
    cata_fx::reset();
    load_emitter_json( R"({
        "id": "hit_blood",
        "trigger": "hit",
        "lifetime_ms": [ 200, 200 ],
        "speed": [ 5, 5 ],
        "heading_deg": 90,
        "spread_deg": 0,
        "gravity": [ 0, 14 ],
        "size_px": [ 3, 3 ],
        "burst": 1,
        "blend": "blend",
        "color": [ 92, 10, 14, 210 ]
    })" );

    cata_fx::spawn( fx_spawn{ .id = fx_emitter_id( "hit_blood" ), .x = 0.0f, .y = 0.0f } );
    REQUIRE( cata_fx::particles().size() == 1 );
    CHECK( cata_fx::particles()[0].vy == Approx( 5.0f ) );
    CHECK( cata_fx::particles()[0].gy == Approx( 14.0f ) );
    CHECK( cata_fx::particles()[0].blend == fx_blend::blend );

    fx_emitter::reset();
    cata_fx::reset();
}

TEST_CASE( "fire ember emitter lists fire fields", "[fx]" )
{
    fx_emitter::reset();
    load_emitter_json( R"({
        "id": "fire_embers",
        "trigger": "field",
        "fields": [ "fd_fire", "fd_flame_burst" ],
        "lifetime_ms": [ 400, 400 ],
        "heading_deg": 270,
        "spread_deg": [ -5, 5 ],
        "gravity": [ 0, 0 ],
        "size_px": [ 2, 2 ],
        "spawn_rate": 6,
        "blend": "additive"
    })" );

    REQUIRE( fx_emitter_id( "fire_embers" ).is_valid() );
    const auto &em = fx_emitter_id( "fire_embers" ).obj();
    CHECK( em.trigger == fx_trigger::field );
    REQUIRE( em.fields.size() == 2 );
    CHECK( em.fields[0] == field_type_str_id( "fd_fire" ) );
    CHECK( em.fields[1] == field_type_str_id( "fd_flame_burst" ) );
    CHECK( em.heading_deg == Approx( 270.0f ) );
    CHECK( em.spread_deg.min == Approx( -5.0f ) );
    CHECK( em.spread_deg.max == Approx( 5.0f ) );
    CHECK( em.gravity_y == Approx( 0.0f ) );

    fx_emitter::reset();
}

TEST_CASE( "fire ember spawn is vertical at zero spread sample", "[fx]" )
{
    fx_emitter::reset();
    cata_fx::reset();
    load_emitter_json( R"({
        "id": "fire_embers",
        "trigger": "field",
        "fields": [ "fd_fire" ],
        "lifetime_ms": [ 400, 400 ],
        "speed": [ 3, 3 ],
        "heading_deg": 270,
        "spread_deg": [ 0, 0 ],
        "gravity": [ 0, 0 ],
        "size_px": [ 4, 4 ],
        "spawn_rate": 1
    })" );

    cata_fx::spawn( fx_spawn{ .id = fx_emitter_id( "fire_embers" ), .x = 0.0f, .y = 0.0f, .count = 1 } );
    REQUIRE( cata_fx::particles().size() == 1 );
    CHECK( cata_fx::particles()[0].vx == Approx( 0.0f ).margin( 0.001f ) );
    CHECK( cata_fx::particles()[0].vy == Approx( -3.0f ) );

    fx_emitter::reset();
    cata_fx::reset();
}

TEST_CASE( "fire smoke lists fire and smoke fields and rises", "[fx]" )
{
    fx_emitter::reset();
    load_emitter_json( R"({
        "id": "fire_smoke",
        "trigger": "field",
        "fields": [ "fd_fire", "fd_flame_burst", "fd_smoke" ],
        "lifetime_ms": [ 800, 800 ],
        "speed": [ 1, 1 ],
        "heading_deg": 270,
        "spread_deg": [ -8, 8 ],
        "gravity": [ 0, 0 ],
        "size_px": [ 5, 5 ],
        "spawn_rate": 3,
        "blend": "blend"
    })" );

    REQUIRE( fx_emitter_id( "fire_smoke" ).is_valid() );
    const auto &em = fx_emitter_id( "fire_smoke" ).obj();
    CHECK( em.trigger == fx_trigger::field );
    REQUIRE( em.fields.size() == 3 );
    CHECK( em.fields[2] == field_type_str_id( "fd_smoke" ) );
    CHECK( em.heading_deg == Approx( 270.0f ) );
    CHECK( em.blend == fx_blend::blend );
    CHECK( em.gravity_y == Approx( 0.0f ) );
    CHECK( em.spread_deg.min == Approx( -8.0f ) );

    cata_fx::reset();
    cata_fx::spawn( fx_spawn{ .id = fx_emitter_id( "fire_smoke" ), .x = 0.0f, .y = 0.0f } );
    REQUIRE( cata_fx::particles().size() == 1 );
    CHECK( cata_fx::particles()[0].vy == Approx( -1.0f ).margin( 0.01f ) );

    fx_emitter::reset();
    cata_fx::reset();
}

TEST_CASE( "liquid footstep ripple emitter uses radial burst settings", "[fx]" )
{
    fx_emitter::reset();
    cata_fx::reset();
    load_emitter_json( R"({
        "id": "water_ripple",
        "trigger": "liquid_footstep",
        "lifetime_ms": [ 500, 500 ],
        "speed": [ 2, 2 ],
        "spread_deg": 0,
        "gravity": [ 0, 0 ],
        "size_px": [ 3, 3 ],
        "size_growth_px_s": [ 12, 12 ],
        "burst": 8,
        "blend": "blend",
        "color": [ 210, 230, 255, 90 ]
    })" );

    REQUIRE( fx_emitter_id( "water_ripple" ).is_valid() );
    const auto &em = fx_emitter_id( "water_ripple" ).obj();
    CHECK( em.trigger == fx_trigger::liquid_footstep );
    CHECK( em.burst == 8 );
    CHECK( em.size_growth_px_s.min == Approx( 12.0f ) );

    cata_fx::spawn( fx_spawn{
        .id = fx_emitter_id( "water_ripple" ),
        .x = 1.0f,
        .y = 2.0f,
        .z = 0.0f
    } );
    REQUIRE( cata_fx::particles().size() == 8 );
    CHECK( cata_fx::particles()[0].vx == Approx( 2.0f ).margin( 0.05f ) );
    CHECK( cata_fx::particles()[0].size_growth_px_s == Approx( 12.0f ) );

    fx_emitter::reset();
    cata_fx::reset();
}

TEST_CASE( "cold breath rises and uses the breath trigger", "[fx]" )
{
    fx_emitter::reset();
    load_emitter_json( R"({
        "id": "cold_breath",
        "trigger": "breath",
        "lifetime_ms": [ 400, 400 ],
        "speed": [ 2, 2 ],
        "heading_deg": 270,
        "spread_deg": 0,
        "gravity": [ 0, 1.2 ],
        "size_px": [ 3, 3 ],
        "spawn_rate": 1.4,
        "blend": "blend",
        "color": [ 220, 230, 240, 90 ]
    })" );

    REQUIRE( fx_emitter_id( "cold_breath" ).is_valid() );
    const auto &em = fx_emitter_id( "cold_breath" ).obj();
    CHECK( em.trigger == fx_trigger::breath );
    CHECK( em.heading_deg == Approx( 270.0f ) );
    CHECK( em.color_r == 220 );
    CHECK( em.color_a == 90 );

    cata_fx::reset();
    cata_fx::spawn( fx_spawn{ .id = fx_emitter_id( "cold_breath" ), .x = 0.0f, .y = 0.0f, .count = 1 } );
    REQUIRE( cata_fx::particles().size() == 1 );
    CHECK( cata_fx::particles()[0].vy == Approx( -2.0f ) );

    fx_emitter::reset();
    cata_fx::reset();
}

TEST_CASE( "present grade tints acid weather and clears for clear skies", "[fx]" )
{
    const auto acid = compute_present_grade( present_grade_input{
        .weather = weather_type_id( "acid_rain" ),
    } );
    CHECK( acid.tint_strength == Approx( 0.32f ) );
    CHECK( acid.tint_g == Approx( 1.12f ) );

    const auto clear = compute_present_grade( present_grade_input{
        .weather = weather_type_id( "clear" ),
    } );
    CHECK( clear.tint_strength == Approx( 0.0f ) );
    CHECK( clear.flash_strength == Approx( 0.0f ) );
}

TEST_CASE( "present grade screen flash scales with lightning fraction", "[fx]" )
{
    const auto peak = compute_present_grade( present_grade_input{
        .weather = weather_type_id( "clear" ),
        .lightning_flash_frac = 1.0f,
    } );
    CHECK( peak.flash_strength == Approx( 0.62f ) );

    const auto none = compute_present_grade( present_grade_input{
        .weather = weather_type_id( "clear" ),
        .lightning_flash_frac = 0.0f,
    } );
    CHECK( none.flash_strength == Approx( 0.0f ) );
}

TEST_CASE( "toxic gas emitter lists hazmat fields", "[fx]" )
{
    fx_emitter::reset();
    load_emitter_json( R"({
        "id": "toxic_gas_wisps",
        "trigger": "field",
        "fields": [ "fd_toxic_gas", "fd_nuke_gas" ],
        "lifetime_ms": [ 900, 900 ],
        "heading_deg": 270,
        "size_px": [ 4, 4 ],
        "spawn_rate": 2.5,
        "blend": "blend"
    })" );

    REQUIRE( fx_emitter_id( "toxic_gas_wisps" ).is_valid() );
    const auto &em = fx_emitter_id( "toxic_gas_wisps" ).obj();
    REQUIRE( em.fields.size() == 2 );
    CHECK( em.fields[0] == field_type_str_id( "fd_toxic_gas" ) );
    CHECK( em.fields[1] == field_type_str_id( "fd_nuke_gas" ) );

    fx_emitter::reset();
}

TEST_CASE( "lightning flash bursts once on a rising lightning_active edge", "[fx]" )
{
    const auto animations = override_option( "ANIMATIONS", "true" );
    const auto gfx = override_option( "GRAPHIC_FX", "true" );
    fx_emitter::reset();
    cata_fx::reset();
    load_emitter_json( R"({
        "id": "lightning_flash",
        "trigger": "lightning",
        "lifetime_ms": [ 5000, 5000 ],
        "speed": [ 0, 0 ],
        "heading_deg": 90,
        "spread_deg": 0,
        "size_px": [ 6, 6 ],
        "burst": 5,
        "blend": "additive"
    })" );

    REQUIRE( fx_emitter_id( "lightning_flash" ).is_valid() );
    CHECK( fx_emitter_id( "lightning_flash" ).obj().trigger == fx_trigger::lightning );

    const auto view = fx_view{ .min_x = 0.0f, .min_y = 0.0f, .max_x = 10.0f, .max_y = 10.0f };
    get_weather().lightning_active = false;
    cata_fx::tick( 0.016f, view );
    CHECK( cata_fx::particles().empty() );

    get_weather().lightning_active = true;
    cata_fx::tick( 0.016f, view );
    CHECK( cata_fx::particles().size() == 5 );

    cata_fx::tick( 0.016f, view );
    CHECK( cata_fx::particles().size() == 5 );

    get_weather().lightning_active = false;
    fx_emitter::reset();
    cata_fx::reset();
}

TEST_CASE( "lightning strike starts screen flash fraction", "[fx]" )
{
    const auto animations = override_option( "ANIMATIONS", "true" );
    const auto gfx = override_option( "GRAPHIC_FX", "true" );
    fx_emitter::reset();
    cata_fx::reset();
    load_emitter_json( R"({
        "id": "lightning_flash",
        "trigger": "lightning",
        "lifetime_ms": [ 100, 100 ],
        "size_px": [ 4, 4 ],
        "burst": 1,
        "blend": "additive"
    })" );

    const auto view = fx_view{ .min_x = 0.0f, .min_y = 0.0f, .max_x = 4.0f, .max_y = 4.0f };
    get_weather().lightning_active = false;
    cata_fx::tick( 0.016f, view );
    get_weather().lightning_active = true;
    cata_fx::tick( 0.016f, view );
    CHECK( cata_fx::lightning_screen_flash_frac() == Approx( 1.0f ).margin( 0.05f ) );

    cata_fx::advance_lightning_screen_flash( 0.11f );
    CHECK( cata_fx::lightning_screen_flash_frac() == Approx( 0.5f ).margin( 0.08f ) );

    fx_emitter::reset();
    cata_fx::reset();
}

TEST_CASE( "combat fx option suppresses hit particles", "[fx]" )
{
    const auto animations = override_option( "ANIMATIONS", "true" );
    const auto gfx = override_option( "GRAPHIC_FX", "true" );
    const auto combat = override_option( "FX_COMBAT", "false" );
    fx_emitter::reset();
    cata_fx::reset();
    load_emitter_json( R"({
        "id": "hit_blood",
        "trigger": "hit",
        "lifetime_ms": [ 200, 200 ],
        "speed": [ 2, 2 ],
        "size_px": [ 2, 2 ],
        "burst": 4
    })" );

    CHECK_FALSE( cata_fx::category_enabled( "FX_COMBAT" ) );
    cata_fx::spawn( fx_spawn{ .id = fx_emitter_id( "hit_blood" ), .x = 0.0f, .y = 0.0f } );
    CHECK( cata_fx::particles().empty() );

    fx_emitter::reset();
    cata_fx::reset();
}

TEST_CASE( "glow mesh expands around the dest rect", "[sprite_fx]" )
{
    const auto dest = sprite_fx_rect{ .x = 10.0f, .y = 20.0f, .w = 16.0f, .h = 16.0f };
    const auto empty = build_glow_mesh( dest, {} );
    CHECK( empty.vertices.empty() );

    const auto fx = sprite_fx{ .kind = sprite_fx_kind::glow, .amplitude_px = 4.0f };
    const auto mesh = build_glow_mesh( dest, fx );
    REQUIRE( mesh.vertices.size() == 4 );
    CHECK( mesh.vertices[0].x == Approx( 6.0f ) );
    CHECK( mesh.vertices[0].y == Approx( 16.0f ) );
    CHECK( mesh.vertices[2].x == Approx( 30.0f ) );
    CHECK( mesh.vertices[2].y == Approx( 40.0f ) );
}

TEST_CASE( "distortion mesh wobbles with phase", "[sprite_fx]" )
{
    const auto dest = sprite_fx_rect{ .x = 0.0f, .y = 0.0f, .w = 10.0f, .h = 10.0f };
    auto fx = sprite_fx{ .kind = sprite_fx_kind::distortion, .amplitude_px = 2.0f, .phase = 0.0f, .time = 0.0f };
    const auto mesh_zero = build_distortion_mesh( dest, fx );
    fx.phase = 1.57079632f;
    const auto mesh_peak = build_distortion_mesh( dest, fx );
    REQUIRE( mesh_zero.vertices.size() == 4 );
    REQUIRE( mesh_peak.vertices.size() == 4 );
    CHECK( mesh_zero.vertices[0].x == Approx( dest.x ).margin( 0.001f ) );
    CHECK( mesh_peak.vertices[0].x == Approx( dest.x + 2.0f ).margin( 0.001f ) );
}

TEST_CASE( "build_sprite_fx_mesh dispatches by kind", "[sprite_fx]" )
{
    const auto dest = sprite_fx_rect{ .x = 0.0f, .y = 0.0f, .w = 8.0f, .h = 8.0f };
    CHECK( build_sprite_fx_mesh( dest, {} ).vertices.empty() );
    CHECK_FALSE( build_sprite_fx_mesh( dest, sprite_fx{ .kind = sprite_fx_kind::glow, .amplitude_px = 1.0f } ).vertices.empty() );
    CHECK_FALSE( build_sprite_fx_mesh( dest, sprite_fx{ .kind = sprite_fx_kind::distortion, .amplitude_px = 1.0f } ).vertices.empty() );
}
