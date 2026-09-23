#include "fx/present_grade.h"

#include <algorithm>
#include <cmath>

namespace
{

auto is_acid_weather( const weather_type_id &weather ) -> bool
{
    return weather == weather_type_id( "acid_drizzle" ) || weather == weather_type_id( "acid_rain" );
}

} // namespace

auto compute_present_grade( const present_grade_input &input ) -> present_grade
{
    auto grade = present_grade{};

    if( input.weather.is_valid() && is_acid_weather( input.weather ) ) {
        grade.tint_r = 1.05f;
        grade.tint_g = 1.12f;
        grade.tint_b = 0.88f;
        grade.tint_strength = 0.32f;
    }

    const auto flash_frac = std::clamp( input.lightning_flash_frac, 0.0f, 1.0f );
    if( flash_frac > 0.0f ) {
        grade.flash_r = 0.92f;
        grade.flash_g = 0.96f;
        grade.flash_b = 1.0f;
        grade.flash_strength = flash_frac * 0.62f;
    }

    return grade;
}
