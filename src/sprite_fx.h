#pragma once

#include <optional>
#include <string_view>
#include <vector>

/// Kind of per-sprite cosmetic deformation. Additional kinds can share the mesh path.
enum class sprite_fx_kind : int {
    none,
    sway,
    glow,
    distortion,
};

struct sprite_fx {
    sprite_fx_kind kind = sprite_fx_kind::none;
    float amplitude_px = 0.0f;
    float phase = 0.0f;
    float time = 0.0f;
};

struct sprite_fx_rect {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

struct sprite_fx_vertex {
    float x = 0.0f;
    float y = 0.0f;
    /// Horizontal UV in sprite-local 0..1 space (not atlas space).
    float u = 0.0f;
    /// Vertical UV in sprite-local 0..1 space; 0 is the top of the sprite.
    float v = 0.0f;
};

struct sprite_fx_mesh {
    std::vector<sprite_fx_vertex> vertices;
    std::vector<int> indices;
};

struct plant_sway_weather {
    int windspeed_mph = 0;
    /// 0 = none … 4 = heavy, matching precip_class.
    int precip_rank = 0;
    bool sheltered = false;
};

struct plant_sway_params {
    int elapsed_ms = 0;
    plant_sway_weather weather{};
    int x = 0;
    int y = 0;
    bool tree = false;
    bool young = false;
    bool shrub = false;
};

/// Pixel amplitude for a plant tile. TREE wins over YOUNG over SHRUB.
auto plant_sway_amplitude( bool tree, bool young, bool shrub ) -> std::optional<float>;

/// Per-tile phase so a forest does not lockstep. Range is about 0..2π.
auto plant_sway_phase( int x, int y ) -> float;

/// Continuous sway clock from elapsed milliseconds. 4.2 rad/s is about a 1.5s cycle.
auto plant_sway_time( int elapsed_ms ) -> float;

/// Still air stays quiet; gales and heavy precip lean harder. Shelter damps both.
auto plant_sway_weather_scale( const plant_sway_weather &weather ) -> float;

/// Frame budget in ms for a Plant sway quality id ("off", "8", "30", "60").
auto plant_sway_frame_budget_ms( std::string_view quality ) -> std::optional<int>;

auto make_plant_sway_fx( const plant_sway_params &params ) -> sprite_fx;

struct sprite_distortion_params {
    int elapsed_ms = 0;
    int x = 0;
    int y = 0;
    float amplitude_px = 1.6f;
};

/// Halo size from monster luminance. Zero luminance is no glow; bright sources clamp.
auto glow_amplitude_from_luminance( float luminance ) -> float;

/// Expanded-quad glow. Empty kind when amplitude is not positive.
auto make_glow_fx( float amplitude_px ) -> sprite_fx;

/// Heat-shimmer wobble. Phase is per-tile so a fire line does not lockstep.
auto make_distortion_fx( const sprite_distortion_params &params ) -> sprite_fx;

/// Fine horizontal strips. The crown leads and the base stays planted, with a
/// smooth cantilever bend so the trunk does not slice into rigid bands.
/// Empty when fx.kind is none.
auto build_sway_mesh( const sprite_fx_rect &dest, const sprite_fx &fx ) -> sprite_fx_mesh;

/// Expanded quad used as a cheap halo. Empty when fx.kind is not glow.
auto build_glow_mesh( const sprite_fx_rect &dest, const sprite_fx &fx ) -> sprite_fx_mesh;

/// Wobbling quad. Empty when fx.kind is not distortion.
auto build_distortion_mesh( const sprite_fx_rect &dest, const sprite_fx &fx ) -> sprite_fx_mesh;

/// Dispatch to the mesh builder for fx.kind.
auto build_sprite_fx_mesh( const sprite_fx_rect &dest, const sprite_fx &fx ) -> sprite_fx_mesh;
