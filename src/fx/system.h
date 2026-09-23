#pragma once

#include "fx/emitter.h"

#include <cstdint>
#include <string_view>
#include <vector>

/// One live particle in bub-map tile space (fractional).
struct fx_particle {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float vx = 0.0f;
    float vy = 0.0f;
    float gx = 0.0f;
    float gy = 0.0f;
    float age_s = 0.0f;
    float life_s = 1.0f;
    float size_px = 3.0f;
    float size_growth_px_s = 0.0f;
    fx_blend blend = fx_blend::blend;
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
    std::uint8_t a = 255;
    /// Drop the particle once it enters an indoor tile. Used by precipitation.
    bool outdoor_only = false;
};

struct fx_spawn {
    fx_emitter_id id;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    int count = -1;
};

struct fx_view {
    float min_x = 0.0f;
    float min_y = 0.0f;
    float max_x = 0.0f;
    float max_y = 0.0f;
    float z = 0.0f;
};

namespace cata_fx
{

/// False on curses, when ANIMATIONS is off, or when GRAPHIC_FX is off.
auto enabled() -> bool;

/// Sub-toggle on the FX options page. True when the option is missing, so tests stay on.
auto category_enabled( std::string_view option_id ) -> bool;

auto reset() -> void;

/// Advance particles by dt seconds and spawn weather, lightning, field, and breath emitters.
auto tick( float dt_s, const fx_view &view ) -> void;

/// Wall-clock tick clamped to 0.1s so alt-tab cannot flood the pool.
auto tick_realtime( const fx_view &view ) -> void;

auto spawn( const fx_spawn &opts ) -> void;

/// Sample one particle from an emitter without going through the world pool.
auto make_particle( const fx_emitter &em, float x, float y, float z ) -> fx_particle;

/// Integrate one particle. Returns false when it should be removed.
auto advance_particle( fx_particle &p, float dt_s ) -> bool;

auto particles() -> const std::vector<fx_particle> &;

/// Screen flash decay after a lightning strike, 0..1 for the present shader.
auto lightning_screen_flash_frac() -> float;

/// Decay the screen flash timer (call once per present frame).
auto advance_lightning_screen_flash( float dt_s ) -> void;

} // namespace cata_fx
