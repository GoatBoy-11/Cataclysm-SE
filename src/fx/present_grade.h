#pragma once

#include "type_id.h"

struct present_grade {
    float color_scale = 1.0f;
    float tint_strength = 0.0f;
    float tint_r = 1.0f;
    float tint_g = 1.0f;
    float tint_b = 1.0f;
    float flash_strength = 0.0f;
    float flash_r = 1.0f;
    float flash_g = 1.0f;
    float flash_b = 1.0f;
};

struct present_grade_input {
    weather_type_id weather;
    /// Remaining screen flash, 0..1, from {@link cata_fx::lightning_screen_flash_frac}.
    float lightning_flash_frac = 0.0f;
};

/// CPU-side grade for the present fragment shader. No GPU when all strengths are zero.
auto compute_present_grade( const present_grade_input &input ) -> present_grade;
