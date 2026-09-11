#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "character_id.h"
#include "coordinates.h"

class Character;

/// Overhead speech balloons for quoted overworld speech. Tiles-only drawing;
/// this module is the spawn/lifetime list. Toggle: options Graphics / SPEECH_BUBBLES.
namespace speech_bubbles
{

constexpr int wrap_width = 24;
constexpr int max_lines = 4;
constexpr int chars_per_second = 16;
constexpr auto min_visible = std::chrono::milliseconds( 2500 );
constexpr auto max_visible = std::chrono::milliseconds( 8000 );
constexpr auto fade_duration = std::chrono::milliseconds( 400 );

struct speech_bubble {
    character_id speaker_id;
    bool speaker_is_avatar = false;
    std::string text;
    std::chrono::steady_clock::time_point born;
    std::chrono::milliseconds duration{ 0 };
};

/// Reading-time for a balloon: 2.5s + display_width/16s, capped at 8s.
auto duration_for( const std::string &text ) -> std::chrono::milliseconds;

/// Fold to wrap_width columns and clip to max_lines with an ellipsis.
auto wrap_text( const std::string &text ) -> std::vector<std::string>;

/// Insert, replacing any existing balloon from the same speaker. Skips empty text.
auto add( const Character &speaker, const std::string &text,
          std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now() ) -> void;

/// add() gated on SPEECH_BUBBLES, use_tiles, and the player both seeing and being
/// able to hear the speaker.
auto try_add( const Character &speaker, const std::string &text ) -> void;

/// Alpha multiplier in [0,1]: solid until the last fade_duration, then linear to zero.
auto fade_for( const speech_bubble &bubble,
               std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now() ) -> float;

auto cull( std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now() ) -> void;
auto clear() -> void;
/// Culls expired balloons, then reports whether any remain.
auto active() -> bool;

auto entries() -> const std::vector<speech_bubble> &;

auto speaker_location( const speech_bubble &bubble ) -> std::optional<tripoint_bub_ms>;

} // namespace speech_bubbles
