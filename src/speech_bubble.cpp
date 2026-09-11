#include "speech_bubble.h"

#include <algorithm>
#include <vector>

#include "avatar.h"
#include "cached_options.h"
#include "catacharset.h"
#include "character.h"
#include "game.h"
#include "npc.h"
#include "options.h"
#include "output.h"

namespace speech_bubbles
{
namespace
{

auto bubbles() -> std::vector<speech_bubble> &
{
    static std::vector<speech_bubble> list;
    return list;
}

auto is_same_speaker( const speech_bubble &bubble, const Character &speaker ) -> bool
{
    if( speaker.is_avatar() ) {
        return bubble.speaker_is_avatar;
    }
    return !bubble.speaker_is_avatar && bubble.speaker_id == speaker.getID();
}

} // namespace

auto duration_for( const std::string &text ) -> std::chrono::milliseconds
{
    const auto width = std::max( 0, utf8_width( text ) );
    const auto extra_ms = ( width * 1000 ) / chars_per_second;
    const auto total = min_visible + std::chrono::milliseconds( extra_ms );
    return std::min( total, max_visible );
}

auto wrap_text( const std::string &text ) -> std::vector<std::string>
{
    auto lines = foldstring( text, wrap_width );
    if( static_cast<int>( lines.size() ) <= max_lines ) {
        return lines;
    }
    lines.resize( max_lines );
    auto &last = lines.back();
    const utf8_wrapper wrapped( last );
    if( static_cast<int>( wrapped.display_width() ) >= wrap_width ) {
        last = wrapped.shorten( wrap_width );
    } else {
        last += "...";
    }
    return lines;
}

auto add( const Character &speaker, const std::string &text,
          std::chrono::steady_clock::time_point now ) -> void
{
    if( text.empty() ) {
        return;
    }

    auto &list = bubbles();
    std::erase_if( list, [&]( const speech_bubble & bubble ) {
        return is_same_speaker( bubble, speaker );
    } );

    list.push_back( speech_bubble{
        .speaker_id = speaker.getID(),
        .speaker_is_avatar = speaker.is_avatar(),
        .text = text,
        .born = now,
        .duration = duration_for( text )
    } );
}

auto try_add( const Character &speaker, const std::string &text ) -> void
{
    if( text.empty() ) {
        return;
    }
    if( !use_tiles ) {
        return;
    }
    if( !get_option<bool>( "SPEECH_BUBBLES" ) ) {
        return;
    }
    if( !speaker.is_avatar() && !get_avatar().sees( speaker ) ) {
        return;
    }
    add( speaker, text );
}

auto cull( std::chrono::steady_clock::time_point now ) -> void
{
    std::erase_if( bubbles(), [now]( const speech_bubble & bubble ) {
        return now >= bubble.born + bubble.duration;
    } );
}

auto clear() -> void
{
    bubbles().clear();
}

auto active() -> bool
{
    cull();
    return !bubbles().empty();
}

auto entries() -> const std::vector<speech_bubble> &
{
    return bubbles();
}

auto speaker_location( const speech_bubble &bubble ) -> std::optional<tripoint_bub_ms>
{
    if( !g ) {
        return std::nullopt;
    }
    if( bubble.speaker_is_avatar ) {
        return g->u.bub_pos();
    }
    for( npc &guy : g->all_npcs() ) {
        if( guy.getID() == bubble.speaker_id ) {
            return guy.bub_pos();
        }
    }
    npc *const found = g->find_npc( bubble.speaker_id );
    if( found == nullptr ) {
        return std::nullopt;
    }
    return found->bub_pos();
}

} // namespace speech_bubbles
