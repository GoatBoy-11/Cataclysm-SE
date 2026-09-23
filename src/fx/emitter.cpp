#include "fx/emitter.h"

#include "debug.h"
#include "enum_conversions.h"
#include "generic_factory.h"
#include "json.h"
#include "string_id.h"
#include "type_id_implement.h"

#include <algorithm>
#include <cstdint>
#include <ranges>
#include <string>
#include <vector>

namespace
{
generic_factory<fx_emitter> fx_emitter_factory( "fx_emitter" );

auto read_range( const JsonObject &jo, const std::string &name, fx_range &out ) -> void
{
    if( jo.has_array( name ) ) {
        const auto arr = jo.get_array( name );
        if( arr.size() == 0 ) {
            return;
        }
        out.min = arr.get_float( 0 );
        out.max = arr.size() > 1 ? arr.get_float( 1 ) : out.min;
        return;
    }
    if( jo.has_number( name ) ) {
        out.min = out.max = jo.get_float( name );
    }
}

auto clamp_u8( const int value ) -> std::uint8_t
{
    return static_cast<std::uint8_t>( std::clamp( value, 0, 255 ) );
}

} // namespace

IMPLEMENT_STRING_ID( fx_emitter, fx_emitter_factory )

namespace io
{
template<>
auto enum_to_string<fx_trigger>( const fx_trigger data ) -> std::string
{
    switch( data ) {
        case fx_trigger::none:
            return "none";
        case fx_trigger::weather:
            return "weather";
        case fx_trigger::field:
            return "field";
        case fx_trigger::muzzle:
            return "muzzle";
        case fx_trigger::hit:
            return "hit";
        case fx_trigger::explosion:
            return "explosion";
        case fx_trigger::breath:
            return "breath";
        case fx_trigger::lightning:
            return "lightning";
        case fx_trigger::liquid_footstep:
            return "liquid_footstep";
        case fx_trigger::last:
            break;
    }
    debugmsg( "Invalid fx_trigger" );
    abort();
}

template<>
auto enum_to_string<fx_blend>( const fx_blend data ) -> std::string
{
    switch( data ) {
        case fx_blend::blend:
            return "blend";
        case fx_blend::additive:
            return "additive";
        case fx_blend::last:
            break;
    }
    debugmsg( "Invalid fx_blend" );
    abort();
}
} // namespace io

void fx_emitter::reset()
{
    fx_emitter_factory.reset();
}

void fx_emitter::load_fx_emitter( const JsonObject &jo, const std::string &src )
{
    fx_emitter_factory.load( jo, src );
}

void fx_emitter::finalize_all()
{
    fx_emitter_factory.finalize();
}

void fx_emitter::check_consistency()
{
    fx_emitter_factory.check();
}

auto fx_emitter::get_all() -> const std::vector<fx_emitter> &
{
    return fx_emitter_factory.get_all();
}

void fx_emitter::load( const JsonObject &jo, const std::string & )
{
    optional( jo, was_loaded, "trigger", trigger, fx_trigger::none );
    optional( jo, was_loaded, "texture", texture );
    optional( jo, was_loaded, "heading_deg", heading_deg, 90.0f );
    optional( jo, was_loaded, "spawn_rate", spawn_rate, 0.0f );
    optional( jo, was_loaded, "burst", burst, 0 );
    optional( jo, was_loaded, "blend", blend, fx_blend::blend );

    read_range( jo, "lifetime_ms", lifetime_ms );
    read_range( jo, "speed", speed );
    // A lone number is a cone half-width. [min, max] stays an explicit range.
    if( jo.has_number( "spread_deg" ) ) {
        const auto half = std::abs( jo.get_float( "spread_deg" ) );
        spread_deg.min = -half;
        spread_deg.max = half;
    } else {
        read_range( jo, "spread_deg", spread_deg );
    }
    read_range( jo, "size_px", size_px );
    read_range( jo, "size_growth_px_s", size_growth_px_s );

    if( jo.has_array( "gravity" ) ) {
        const auto arr = jo.get_array( "gravity" );
        if( arr.size() > 0 ) {
            gravity_x = arr.get_float( 0 );
        }
        if( arr.size() > 1 ) {
            gravity_y = arr.get_float( 1 );
        }
    } else {
        optional( jo, was_loaded, "gravity_x", gravity_x, 0.0f );
        optional( jo, was_loaded, "gravity_y", gravity_y, 0.0f );
    }

    if( jo.has_array( "color" ) ) {
        const auto arr = jo.get_array( "color" );
        if( arr.size() >= 3 ) {
            color_r = clamp_u8( arr.get_int( 0 ) );
            color_g = clamp_u8( arr.get_int( 1 ) );
            color_b = clamp_u8( arr.get_int( 2 ) );
        }
        if( arr.size() >= 4 ) {
            color_a = clamp_u8( arr.get_int( 3 ) );
        }
    }

    weathers.clear();
    if( jo.has_array( "weathers" ) ) {
        using namespace std::views;
        const auto names = jo.get_string_array( "weathers" );
        weathers = names | transform( []( const std::string & name ) {
            return weather_type_id( name );
        } ) | std::ranges::to<std::vector<weather_type_id>>();
    }

    fields.clear();
    if( jo.has_array( "fields" ) ) {
        using namespace std::views;
        const auto names = jo.get_string_array( "fields" );
        fields = names | transform( []( const std::string & name ) {
            return field_type_str_id( name );
        } ) | std::ranges::to<std::vector<field_type_str_id>>();
    }
}

void fx_emitter::check() const
{
    if( lifetime_ms.min <= 0.0f || lifetime_ms.max < lifetime_ms.min ) {
        debugmsg( "fx_emitter %s has invalid lifetime_ms", id.str() );
    }
    if( size_px.min <= 0.0f || size_px.max < size_px.min ) {
        debugmsg( "fx_emitter %s has invalid size_px", id.str() );
    }
    if( trigger == fx_trigger::weather && weathers.empty() ) {
        debugmsg( "fx_emitter %s uses weather trigger but lists no weathers", id.str() );
    }
    if( trigger == fx_trigger::field && fields.empty() ) {
        debugmsg( "fx_emitter %s uses field trigger but lists no fields", id.str() );
    }
    if( trigger == fx_trigger::breath && spawn_rate <= 0.0f ) {
        debugmsg( "fx_emitter %s uses breath trigger but has no spawn_rate", id.str() );
    }
    if( trigger == fx_trigger::lightning && burst <= 0 ) {
        debugmsg( "fx_emitter %s uses lightning trigger but has no burst", id.str() );
    }
    if( trigger == fx_trigger::liquid_footstep && burst <= 0 ) {
        debugmsg( "fx_emitter %s uses liquid_footstep trigger but has no burst", id.str() );
    }
    namespace ranges = std::ranges;
    ranges::for_each( weathers, [this]( const weather_type_id & weather ) {
        if( !weather.is_valid() ) {
            debugmsg( "fx_emitter %s references unknown weather %s", id.str(), weather.str() );
        }
    } );
    ranges::for_each( fields, [this]( const field_type_str_id & field ) {
        if( !field.is_valid() ) {
            debugmsg( "fx_emitter %s references unknown field %s", id.str(), field.str() );
        }
    } );
}
