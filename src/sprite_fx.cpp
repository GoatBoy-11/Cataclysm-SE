#include "sprite_fx.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string_view>

namespace
{

constexpr auto k_strips = 24;
constexpr auto k_tree_amp = 1.5f;
constexpr auto k_young_amp = 1.0f;
constexpr auto k_shrub_amp = 0.55f;
constexpr auto k_sway_radians_per_second = 4.2f;
constexpr auto k_still_scale = 0.35f;
constexpr auto k_gale_wind_extra = 1.4f;
constexpr auto k_heavy_precip_extra = 0.4f;
constexpr auto k_gale_windspeed = 40.0f;
constexpr auto k_shelter_still_factor = 0.5f;
constexpr auto k_shelter_gust_factor = 0.15f;

/// Cantilever pinned at the base. v is 0 at the crown and 1 at the roots.
auto sway_dx( const sprite_fx &fx, const float v ) -> float
{
    const auto from_base = 1.0f - v;
    const auto bend = ( 3.0f * from_base * from_base - from_base * from_base * from_base ) * 0.5f;
    return fx.amplitude_px * std::sin( fx.time + fx.phase ) * bend;
}

} // namespace

auto plant_sway_amplitude( const bool tree, const bool young, const bool shrub )
-> std::optional<float>
{
    if( tree ) {
        return k_tree_amp;
    }
    if( young ) {
        return k_young_amp;
    }
    if( shrub ) {
        return k_shrub_amp;
    }
    return std::nullopt;
}

auto plant_sway_phase( const int x, const int y ) -> float
{
    const auto seed = static_cast<unsigned>( x ) + static_cast<unsigned>( y ) * 65536u;
    return static_cast<float>( seed % 6283u ) * 0.001f;
}

auto plant_sway_time( const int elapsed_ms ) -> float
{
    return static_cast<float>( elapsed_ms ) * 0.001f * k_sway_radians_per_second;
}

auto plant_sway_weather_scale( const plant_sway_weather &weather ) -> float
{
    const auto wind_t = std::clamp( static_cast<float>( weather.windspeed_mph ) / k_gale_windspeed,
                                    0.0f, 1.0f );
    const auto precip_t = std::clamp( static_cast<float>( weather.precip_rank ) / 4.0f, 0.0f, 1.0f );
    const auto gust = wind_t * k_gale_wind_extra + precip_t * k_heavy_precip_extra;
    if( weather.sheltered ) {
        return k_still_scale * k_shelter_still_factor + gust * k_shelter_gust_factor;
    }
    return k_still_scale + gust;
}

auto plant_sway_frame_budget_ms( const std::string_view quality ) -> std::optional<int>
{
    if( quality == "8" ) {
        return 125;
    }
    if( quality == "30" ) {
        return 33;
    }
    if( quality == "60" ) {
        return 16;
    }
    return std::nullopt;
}

auto make_plant_sway_fx( const plant_sway_params &params ) -> sprite_fx
{
    const auto amp = plant_sway_amplitude( params.tree, params.young, params.shrub );
    if( !amp ) {
        return {};
    }
    return sprite_fx{
        .kind = sprite_fx_kind::sway,
        .amplitude_px = *amp * plant_sway_weather_scale( params.weather ),
        .phase = plant_sway_phase( params.x, params.y ),
        .time = plant_sway_time( params.elapsed_ms ),
    };
}

auto glow_amplitude_from_luminance( const float luminance ) -> float
{
    if( luminance <= 0.0f ) {
        return 0.0f;
    }
    return std::clamp( 2.0f + std::log2( std::max( luminance, 1.0f ) ) * 0.55f, 2.0f, 6.0f );
}

auto make_glow_fx( const float amplitude_px ) -> sprite_fx
{
    if( amplitude_px <= 0.0f ) {
        return {};
    }
    return sprite_fx{
        .kind = sprite_fx_kind::glow,
        .amplitude_px = amplitude_px,
    };
}

auto make_distortion_fx( const sprite_distortion_params &params ) -> sprite_fx
{
    if( params.amplitude_px <= 0.0f ) {
        return {};
    }
    return sprite_fx{
        .kind = sprite_fx_kind::distortion,
        .amplitude_px = params.amplitude_px,
        .phase = plant_sway_phase( params.x, params.y ),
        .time = plant_sway_time( params.elapsed_ms ) * 0.55f,
    };
}

auto build_sway_mesh( const sprite_fx_rect &dest, const sprite_fx &fx ) -> sprite_fx_mesh
{
    if( fx.kind != sprite_fx_kind::sway || dest.w <= 0.0f || dest.h <= 0.0f ) {
        return {};
    }

    auto mesh = sprite_fx_mesh{};
    const auto row_count = k_strips + 1;
    mesh.vertices.reserve( static_cast<std::size_t>( row_count ) * 2 );
    mesh.indices.reserve( static_cast<std::size_t>( k_strips ) * 6 );

    for( int row = 0; row < row_count; ++row ) {
        const auto v = static_cast<float>( row ) / static_cast<float>( k_strips );
        const auto dx = sway_dx( fx, v );
        const auto y = dest.y + dest.h * v;
        mesh.vertices.push_back( sprite_fx_vertex{
            .x = dest.x + dx,
            .y = y,
            .u = 0.0f,
            .v = v,
        } );
        mesh.vertices.push_back( sprite_fx_vertex{
            .x = dest.x + dest.w + dx,
            .y = y,
            .u = 1.0f,
            .v = v,
        } );
    }

    for( int strip = 0; strip < k_strips; ++strip ) {
        const auto top_left = strip * 2;
        const auto top_right = top_left + 1;
        const auto bot_left = top_left + 2;
        const auto bot_right = top_left + 3;
        mesh.indices.push_back( top_left );
        mesh.indices.push_back( top_right );
        mesh.indices.push_back( bot_left );
        mesh.indices.push_back( top_right );
        mesh.indices.push_back( bot_right );
        mesh.indices.push_back( bot_left );
    }

    return mesh;
}

namespace
{

auto make_quad_mesh( const sprite_fx_rect &dest, const float dx0, const float dy0,
                     const float dx1, const float dy1, const float dx2, const float dy2,
                     const float dx3, const float dy3 ) -> sprite_fx_mesh
{
    auto mesh = sprite_fx_mesh{};
    mesh.vertices = {
        { .x = dest.x + dx0, .y = dest.y + dy0, .u = 0.0f, .v = 0.0f },
        { .x = dest.x + dest.w + dx1, .y = dest.y + dy1, .u = 1.0f, .v = 0.0f },
        { .x = dest.x + dest.w + dx2, .y = dest.y + dest.h + dy2, .u = 1.0f, .v = 1.0f },
        { .x = dest.x + dx3, .y = dest.y + dest.h + dy3, .u = 0.0f, .v = 1.0f },
    };
    mesh.indices = { 0, 1, 2, 0, 2, 3 };
    return mesh;
}

} // namespace

auto build_glow_mesh( const sprite_fx_rect &dest, const sprite_fx &fx ) -> sprite_fx_mesh
{
    if( fx.kind != sprite_fx_kind::glow || dest.w <= 0.0f || dest.h <= 0.0f ) {
        return {};
    }
    const auto pad = std::max( fx.amplitude_px, 0.0f );
    const auto grown = sprite_fx_rect{
        .x = dest.x - pad,
        .y = dest.y - pad,
        .w = dest.w + pad * 2.0f,
        .h = dest.h + pad * 2.0f
    };
    return make_quad_mesh( grown, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f );
}

auto build_distortion_mesh( const sprite_fx_rect &dest, const sprite_fx &fx ) -> sprite_fx_mesh
{
    if( fx.kind != sprite_fx_kind::distortion || dest.w <= 0.0f || dest.h <= 0.0f ) {
        return {};
    }
    const auto wobble = fx.amplitude_px * std::sin( fx.time + fx.phase );
    const auto wobble_y = fx.amplitude_px * std::cos( fx.time + fx.phase ) * 0.35f;
    return make_quad_mesh( dest, wobble, wobble_y, -wobble, wobble_y, wobble, -wobble_y, -wobble,
                           -wobble_y );
}

auto build_sprite_fx_mesh( const sprite_fx_rect &dest, const sprite_fx &fx ) -> sprite_fx_mesh
{
    switch( fx.kind ) {
        case sprite_fx_kind::sway:
            return build_sway_mesh( dest, fx );
        case sprite_fx_kind::glow:
            return build_glow_mesh( dest, fx );
        case sprite_fx_kind::distortion:
            return build_distortion_mesh( dest, fx );
        case sprite_fx_kind::none:
            break;
    }
    return {};
}
