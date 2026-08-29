#pragma once

#include <concepts>
#include <cstdint>
#include <limits>

#include "units_def.h"

namespace units
{

class length_in_millimeter_tag
{
};

using length = quantity<std::int64_t, length_in_millimeter_tag>;

const length length_min = units::length( std::numeric_limits<units::length::value_type>::min(),
                          units::length::unit_type{} );

const length length_max = units::length( std::numeric_limits<units::length::value_type>::max(),
                          units::length::unit_type{} );

template<typename value_type>
constexpr quantity<value_type, length_in_millimeter_tag> from_millimeter(
    const value_type v )
{
    return quantity<value_type, length_in_millimeter_tag>( v, length_in_millimeter_tag{} );
}

template<std::integral value_type>
constexpr length from_centimeter( const value_type v )
{
    return from_millimeter( static_cast<length::value_type>( v ) * 10 );
}

template<std::integral value_type>
constexpr length from_meter( const value_type v )
{
    return from_millimeter( static_cast<length::value_type>( v ) * 1000 );
}

template<typename value_type>
constexpr value_type to_millimeter( const quantity<value_type, length_in_millimeter_tag> &v )
{
    return v / from_millimeter<value_type>( 1 );
}

constexpr double to_centimeter( const length &v )
{
    return v.value() / 10.0;
}

constexpr double to_meter( const length &v )
{
    return v.value() / 1000.0;
}

} // namespace units

// Implicitly converted to length, which has int64_t as value_type!
constexpr units::length operator""_mm( const unsigned long long v )
{
    return units::from_millimeter( v );
}

constexpr units::quantity<double, units::length_in_millimeter_tag> operator""_mm(
    const long double v )
{
    return units::from_millimeter( v );
}

// Implicitly converted to length, which has int64_t as value_type!
constexpr units::length operator""_cm( const unsigned long long v )
{
    return units::from_millimeter( v * 10 );
}

constexpr units::quantity<double, units::length_in_millimeter_tag> operator""_cm(
    const long double v )
{
    return units::from_millimeter( v * 10 );
}
