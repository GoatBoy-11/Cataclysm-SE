#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "catalua_type_operators.h"
#include "enum_traits.h"
#include "type_id.h"

class JsonObject;

/// Inclusive [min, max] sample range. Equal endpoints are deterministic.
struct fx_range {
    float min = 0.0f;
    float max = 0.0f;
};

enum class fx_trigger : int {
    none,
    weather,
    field,
    muzzle,
    hit,
    explosion,
    breath,
    lightning,
    liquid_footstep,
    last
};

enum class fx_blend : int {
    blend,
    additive,
    last
};

template<>
struct enum_traits<fx_trigger> {
    static constexpr auto last = fx_trigger::last;
};

template<>
struct enum_traits<fx_blend> {
    static constexpr auto last = fx_blend::last;
};

/// JSON-driven particle emitter. Simulation is renderer-agnostic.
struct fx_emitter {
    fx_emitter_id id;
    bool was_loaded = false;

    fx_trigger trigger = fx_trigger::none;
    std::vector<weather_type_id> weathers;
    std::vector<field_type_str_id> fields;
    std::string texture;
    fx_range lifetime_ms{ 400.0f, 800.0f };
    fx_range speed{ 4.0f, 8.0f };
    fx_range spread_deg{ -8.0f, 8.0f };
    float heading_deg = 90.0f;
    float gravity_x = 0.0f;
    float gravity_y = 0.0f;
    fx_range size_px{ 3.0f, 5.0f };
    fx_range size_growth_px_s{ 0.0f, 0.0f };
    float spawn_rate = 0.0f;
    int burst = 0;
    fx_blend blend = fx_blend::blend;
    std::uint8_t color_r = 255;
    std::uint8_t color_g = 255;
    std::uint8_t color_b = 255;
    std::uint8_t color_a = 200;

    static void reset();
    static void load_fx_emitter( const JsonObject &jo, const std::string &src );
    static void finalize_all();
    static void check_consistency();
    static auto get_all() -> const std::vector<fx_emitter> &;

    void load( const JsonObject &jo, const std::string &src );
    void check() const;

    LUA_TYPE_OPS( fx_emitter, id );
};
