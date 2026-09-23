#include "fx/system.h"

#include "avatar.h"
#include "character.h"
#include "coordinates.h"
#include "creature.h"
#include "game.h"
#include "map/field.h"
#include "map/map.h"
#include "map/mapdata.h"
#include "math_defines.h"
#include "monster.h"
#include "mtype.h"
#include "npc.h"
#include "options.h"
#include "rng.h"
#include "units_temperature.h"
#include "weather/weather.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{

constexpr auto k_max_particles = 4096;
constexpr auto k_max_dt_s = 0.1f;

auto s_particles = std::vector<fx_particle> {};
auto s_weather_accum = std::unordered_map<std::string, float> {};
auto s_prev_lightning = false;
auto s_lightning_screen_flash_s = 0.0f;
auto s_liquid_last_xy = std::unordered_map<int64_t, point_bub_ms> {};

constexpr auto k_lightning_screen_flash_duration_s = 0.22f;

auto sample_range( const fx_range &range ) -> float
{
    if( range.max <= range.min ) {
        return range.min;
    }
    return static_cast<float>( rng_float( range.min, range.max ) );
}

auto deg_to_rad( const float deg ) -> float
{
    return deg * static_cast<float>( M_PI ) / 180.0f;
}

auto push_particle( fx_particle p ) -> void
{
    if( static_cast<int>( s_particles.size() ) >= k_max_particles ) {
        const auto drop = std::max( s_particles.size() / 8, static_cast<std::size_t>( 1 ) );
        s_particles.erase( s_particles.begin(),
                           s_particles.begin() + static_cast<std::ptrdiff_t>( drop ) );
    }
    s_particles.push_back( std::move( p ) );
}

auto tile_is_outside( const float x, const float y, const float z ) -> bool
{
    if( g == nullptr ) {
        return true;
    }
    const auto pos = tripoint_bub_ms( static_cast<int>( std::floor( x ) ),
                                      static_cast<int>( std::floor( y ) ),
                                      static_cast<int>( std::floor( z ) ) );
    auto &here = get_map();
    if( !here.inbounds( pos ) ) {
        return true;
    }
    return here.is_outside( pos );
}

auto scatter_in_view( const fx_emitter &em, const fx_view &view, const int count ) -> void
{
    const auto width = view.max_x - view.min_x;
    const auto height = view.max_y - view.min_y;
    for( int i = 0; i < count; ++i ) {
        const auto x = view.min_x + static_cast<float>( rng_float( 0.0, static_cast<double>( width ) ) );
        const auto y = view.min_y + static_cast<float>( rng_float( 0.0, static_cast<double>( height ) ) );
        push_particle( cata_fx::make_particle( em, x, y, view.z ) );
    }
}

auto in_view_tile( const fx_view &view, const tripoint_bub_ms &pos ) -> bool
{
    const auto x = static_cast<float>( pos.x() );
    const auto y = static_cast<float>( pos.y() );
    const auto z = static_cast<float>( pos.z() );
    return z >= view.z && z < view.z + 1.0f &&
           x >= view.min_x && x < view.max_x &&
           y >= view.min_y && y < view.max_y;
}

auto spawn_expected( const fx_emitter &em, const float x, const float y, const float z,
                     float expected ) -> void
{
    while( expected >= 1.0f ) {
        expected -= 1.0f;
        push_particle( cata_fx::make_particle( em, x, y, z ) );
    }
    if( rng_float( 0.0, 1.0 ) < static_cast<double>( expected ) ) {
        push_particle( cata_fx::make_particle( em, x, y, z ) );
    }
}

auto liquid_footstep_track_key( const Creature &who ) -> int64_t
{
    if( const Character *const ch = who.as_character() ) {
        const auto id = ch->getID();
        if( id.is_valid() ) {
            return id.get_value();
        }
    }
    return static_cast<int64_t>( reinterpret_cast<uintptr_t>( &who ) );
}

auto on_liquid_surface( const map &here, const Creature &who, const tripoint_bub_ms &pos ) -> bool
{
    if( !here.inbounds( pos ) || !here.has_flag( TFLAG_SWIMMABLE, pos ) ) {
        return false;
    }
    if( who.is_underwater() ) {
        return false;
    }
    if( const Character *const ch = who.as_character() ) {
        if( ch->in_vehicle ) {
            return false;
        }
    }
    return true;
}

auto spawn_radial_burst( const fx_emitter &em, const float x, const float y, const float z ) -> void
{
    const auto count = std::max( em.burst, 1 );
    for( int i = 0; i < count; ++i ) {
        const auto heading = 360.0f * static_cast<float>( i ) / static_cast<float>( count );
        const auto speed = sample_range( em.speed );
        const auto rad = deg_to_rad( heading + sample_range( em.spread_deg ) );
        const auto life_ms = std::max( sample_range( em.lifetime_ms ), 1.0f );
        push_particle( fx_particle{
            .x = x,
            .y = y,
            .z = z,
            .vx = std::cos( rad ) * speed,
            .vy = std::sin( rad ) * speed,
            .gx = em.gravity_x,
            .gy = em.gravity_y,
            .age_s = 0.0f,
            .life_s = life_ms * 0.001f,
            .size_px = std::max( sample_range( em.size_px ), 1.0f ),
            .size_growth_px_s = sample_range( em.size_growth_px_s ),
            .blend = em.blend,
            .r = em.color_r,
            .g = em.color_g,
            .b = em.color_b,
            .a = em.color_a,
        } );
    }
}

auto try_liquid_footstep( const fx_emitter &em, const Creature &who, const fx_view &view,
                          map &here ) -> void
{
    if( who.is_dead_state() ) {
        s_liquid_last_xy.erase( liquid_footstep_track_key( who ) );
        return;
    }
    const auto pos = who.bub_pos();
    if( !in_view_tile( view, pos ) ) {
        return;
    }
    if( !on_liquid_surface( here, who, pos ) ) {
        s_liquid_last_xy.erase( liquid_footstep_track_key( who ) );
        return;
    }
    const auto key = liquid_footstep_track_key( who );
    const auto xy = pos.xy();
    const auto prev_it = s_liquid_last_xy.find( key );
    if( prev_it != s_liquid_last_xy.end() && prev_it->second != xy ) {
        // Draw adds half a tile, so +0.42 in y sits on the bottom of the sprite.
        spawn_radial_burst( em, static_cast<float>( pos.x() ),
                            static_cast<float>( pos.y() ) + 0.42f, static_cast<float>( pos.z() ) );
    }
    s_liquid_last_xy[key] = xy;
}

auto try_breath( const fx_emitter &em, const Creature &who, const fx_view &view,
                 const float dt_s ) -> void
{
    if( who.is_dead_state() || who.is_underwater() ) {
        return;
    }
    const auto pos = who.bub_pos();
    if( !in_view_tile( view, pos ) ) {
        return;
    }
    if( get_weather().get_temperature( who.abs_pos() ) > 0_c ) {
        return;
    }
    // Tile origin is the sprite center after the draw offset; nudge up toward the mouth.
    spawn_expected( em, static_cast<float>( pos.x() ),
                    static_cast<float>( pos.y() ) - 0.15f, view.z, em.spawn_rate * dt_s );
}

} // namespace

namespace cata_fx
{

auto enabled() -> bool
{
#if !defined(TILES)
    return false;
#endif
    auto &opts = get_options();
    if( opts.has_option( "ANIMATIONS" ) && !get_option<bool>( "ANIMATIONS" ) ) {
        return false;
    }
    if( !opts.has_option( "GRAPHIC_FX" ) ) {
        return true;
    }
    return get_option<bool>( "GRAPHIC_FX" );
}

auto category_enabled( const std::string_view option_id ) -> bool
{
    auto &opts = get_options();
    const auto name = std::string( option_id );
    if( !opts.has_option( name ) ) {
        return true;
    }
    return get_option<bool>( name );
}

auto emitter_enabled( const fx_emitter &em ) -> bool
{
    switch( em.trigger ) {
        case fx_trigger::weather:
            return category_enabled( "FX_WEATHER" );
        case fx_trigger::muzzle:
        case fx_trigger::hit:
        case fx_trigger::explosion:
            return category_enabled( "FX_COMBAT" );
        case fx_trigger::breath:
            return category_enabled( "FX_BREATH" );
        case fx_trigger::lightning:
            return category_enabled( "FX_LIGHTNING" );
        case fx_trigger::liquid_footstep:
            return category_enabled( "FX_RIPPLES" );
        case fx_trigger::field: {
            const auto &id = em.id.str();
            if( id == "fire_embers" || id == "fire_smoke" || id == "incendiary_sparks" ) {
                return category_enabled( "FX_FIRE" );
            }
            return category_enabled( "FX_FIELDS" );
        }
        default:
            return true;
    }
}

auto reset() -> void
{
    s_particles.clear();
    s_weather_accum.clear();
    s_prev_lightning = false;
    s_lightning_screen_flash_s = 0.0f;
    s_liquid_last_xy.clear();
}

auto lightning_screen_flash_frac() -> float
{
    if( s_lightning_screen_flash_s <= 0.0f ) {
        return 0.0f;
    }
    return std::clamp( s_lightning_screen_flash_s / k_lightning_screen_flash_duration_s, 0.0f, 1.0f );
}

auto advance_lightning_screen_flash( const float dt_s ) -> void
{
    if( s_lightning_screen_flash_s <= 0.0f || dt_s <= 0.0f ) {
        return;
    }
    s_lightning_screen_flash_s = std::max( 0.0f, s_lightning_screen_flash_s - dt_s );
}

auto make_particle( const fx_emitter &em, const float x, const float y, const float z )
-> fx_particle
{
    const auto heading = em.heading_deg + sample_range( em.spread_deg );
    const auto speed = sample_range( em.speed );
    const auto rad = deg_to_rad( heading );
    const auto life_ms = std::max( sample_range( em.lifetime_ms ), 1.0f );
    return fx_particle{
        .x = x,
        .y = y,
        .z = z,
        .vx = std::cos( rad ) * speed,
        .vy = std::sin( rad ) * speed,
        .gx = em.gravity_x,
        .gy = em.gravity_y,
        .age_s = 0.0f,
        .life_s = life_ms * 0.001f,
        .size_px = std::max( sample_range( em.size_px ), 1.0f ),
        .size_growth_px_s = sample_range( em.size_growth_px_s ),
        .blend = em.blend,
        .r = em.color_r,
        .g = em.color_g,
        .b = em.color_b,
        .a = em.color_a,
    };
}

auto advance_particle( fx_particle &p, const float dt_s ) -> bool
{
    p.age_s += dt_s;
    if( p.age_s >= p.life_s ) {
        return false;
    }
    p.vx += p.gx * dt_s;
    p.vy += p.gy * dt_s;
    p.x += p.vx * dt_s;
    p.y += p.vy * dt_s;
    if( p.size_growth_px_s != 0.0f ) {
        p.size_px = std::max( p.size_px + p.size_growth_px_s * dt_s, 1.0f );
    }
    return true;
}

auto particles() -> const std::vector<fx_particle> &
{
    return s_particles;
}

auto spawn( const fx_spawn &opts ) -> void
{
    if( !enabled() || !opts.id.is_valid() ) {
        return;
    }
    const auto &em = opts.id.obj();
    if( !emitter_enabled( em ) ) {
        return;
    }
    if( em.trigger == fx_trigger::liquid_footstep && em.burst > 0 ) {
        spawn_radial_burst( em, opts.x, opts.y, opts.z );
        return;
    }
    const auto count = opts.count >= 0 ? opts.count : std::max( em.burst, 1 );
    for( int i = 0; i < count; ++i ) {
        push_particle( make_particle( em, opts.x, opts.y, opts.z ) );
    }
}

auto tick( const float dt_s, const fx_view &view ) -> void
{
    if( !enabled() || dt_s <= 0.0f ) {
        return;
    }

    namespace ranges = std::ranges;
    auto still_alive = std::vector<fx_particle> {};
    still_alive.reserve( s_particles.size() );
    for( auto &p : s_particles ) {
        if( advance_particle( p, dt_s ) &&
            !( p.outdoor_only && !tile_is_outside( p.x, p.y, p.z ) ) ) {
            still_alive.push_back( p );
        }
    }
    s_particles = std::move( still_alive );

    if( view.max_x <= view.min_x || view.max_y <= view.min_y ) {
        return;
    }

    using namespace std::views;
    const auto width = view.max_x - view.min_x;
    const auto height = view.max_y - view.min_y;
    const auto weather = get_weather().weather_id;

    for( const auto &em : fx_emitter::get_all() ) {
        if( em.trigger != fx_trigger::weather || em.spawn_rate <= 0.0f || !emitter_enabled( em ) ) {
            continue;
        }
        if( !weather.is_valid() ) {
            continue;
        }
        const auto matches = ranges::any_of( em.weathers, [&]( const weather_type_id & id ) {
            return id == weather;
        } );
        if( !matches ) {
            continue;
        }
        auto &accum = s_weather_accum[em.id.str()];
        accum += em.spawn_rate * dt_s;
        while( accum >= 1.0f ) {
            accum -= 1.0f;
            const auto x = view.min_x + static_cast<float>( rng_float( 0.0, static_cast<double>( width ) ) );
            const auto y = view.min_y + static_cast<float>( rng_float( 0.0, static_cast<double>( height ) ) );
            if( !tile_is_outside( x, y, view.z ) ) {
                continue;
            }
            auto particle = make_particle( em, x, y, view.z );
            particle.outdoor_only = true;
            push_particle( std::move( particle ) );
        }
    }

    const auto lightning_now = get_weather().lightning_active;
    if( lightning_now && !s_prev_lightning && category_enabled( "FX_LIGHTNING" ) ) {
        s_lightning_screen_flash_s = k_lightning_screen_flash_duration_s;
        for( const auto &em : fx_emitter::get_all() ) {
            if( em.trigger != fx_trigger::lightning || em.burst <= 0 ) {
                continue;
            }
            scatter_in_view( em, view, em.burst );
        }
    }
    s_prev_lightning = lightning_now;

    if( g == nullptr ) {
        return;
    }

    auto field_emitters = std::vector<const fx_emitter *> {};
    auto breath_emitters = std::vector<const fx_emitter *> {};
    auto liquid_emitters = std::vector<const fx_emitter *> {};
    for( const auto &em : fx_emitter::get_all() ) {
        if( !emitter_enabled( em ) ) {
            continue;
        }
        if( em.trigger == fx_trigger::field && em.spawn_rate > 0.0f && !em.fields.empty() ) {
            field_emitters.push_back( &em );
        } else if( em.trigger == fx_trigger::breath && em.spawn_rate > 0.0f ) {
            breath_emitters.push_back( &em );
        } else if( em.trigger == fx_trigger::liquid_footstep && em.burst > 0 ) {
            liquid_emitters.push_back( &em );
        }
    }

    if( !field_emitters.empty() ) {
        auto &here = get_map();
        const auto min_x = static_cast<int>( std::floor( view.min_x ) );
        const auto min_y = static_cast<int>( std::floor( view.min_y ) );
        const auto max_x = static_cast<int>( std::ceil( view.max_x ) );
        const auto max_y = static_cast<int>( std::ceil( view.max_y ) );
        const auto z = static_cast<int>( view.z );

        for( const auto y : iota( min_y, max_y ) ) {
            for( const auto x : iota( min_x, max_x ) ) {
                const auto pos = tripoint_bub_ms( x, y, z );
                if( !here.inbounds( pos ) || !here.has_field_at( pos ) ) {
                    continue;
                }
                const auto &tile_fields = here.get_field( pos );
                for( const auto *em : field_emitters ) {
                    auto intensity = 0;
                    for( const auto &fid : em->fields ) {
                        if( !fid.is_valid() ) {
                            continue;
                        }
                        const auto *entry = tile_fields.find_field( fid.id() );
                        if( entry != nullptr ) {
                            intensity = std::max( intensity, entry->get_field_intensity() );
                        }
                    }
                    if( intensity <= 0 ) {
                        continue;
                    }
                    // Tile origin: draw adds half a tile, so this sits on the sprite.
                    spawn_expected( *em, static_cast<float>( x ),
                                    static_cast<float>( y ), view.z,
                                    em->spawn_rate * dt_s * static_cast<float>( intensity ) );
                }
            }
        }
    }

    if( breath_emitters.empty() && liquid_emitters.empty() ) {
        return;
    }

    // One sample: cold breath follows the air the avatar is standing in.
    const auto air_is_cold = !breath_emitters.empty() &&
                              get_weather().get_temperature( g->u.abs_pos() ) <= 0_c;
    if( s_liquid_last_xy.size() > 2048 ) {
        s_liquid_last_xy.clear();
    }

    auto &here = get_map();
    auto visit = [&]( const Creature & who, const bool breathes ) {
        if( air_is_cold && breathes ) {
            for( const auto *em : breath_emitters ) {
                try_breath( *em, who, view, dt_s );
            }
        }
        if( !liquid_emitters.empty() ) {
            for( const auto *em : liquid_emitters ) {
                try_liquid_footstep( *em, who, view, here );
            }
        }
    };

    visit( g->u, true );
    for( const auto &who : g->all_npcs() ) {
        visit( who, true );
    }
    for( const auto &mon : g->all_monsters() ) {
        visit( mon, !mon.has_flag( MF_NO_BREATHE ) );
    }
}

auto tick_realtime( const fx_view &view ) -> void
{
    static auto last = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    const auto dt = std::chrono::duration<float>( now - last ).count();
    last = now;
    tick( std::clamp( dt, 0.0f, k_max_dt_s ), view );
}

} // namespace cata_fx
