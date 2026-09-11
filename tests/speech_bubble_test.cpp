#include <chrono>
#include <string>

#include "avatar.h"
#include "cached_options.h"
#include "calendar.h"
#include "catch/catch.hpp"
#include "cata_utility.h"
#include "map_helpers.h"
#include "npc.h"
#include "options_helpers.h"
#include "player_helpers.h"
#include "point.h"
#include "speech_bubble.h"
#include "state_helpers.h"
#include "type_id.h"

TEST_CASE("speech bubble duration uses a floor plus reading time", "[speech_bubble]") {
    using ms = std::chrono::milliseconds;

    CHECK(speech_bubbles::duration_for("Bye.") == ms(2500 + 4 * 1000 / 16));
    CHECK(speech_bubbles::duration_for(std::string(12, 'x')) == ms(2500 + 12 * 1000 / 16));
    CHECK(speech_bubbles::duration_for(std::string(36, 'x')) == ms(2500 + 36 * 1000 / 16));
    CHECK(speech_bubbles::duration_for(std::string(200, 'x')) == ms(8000));
}

TEST_CASE("speech bubble wrap clips long text", "[speech_bubble]") {
    const auto short_lines = speech_bubbles::wrap_text("Look sharp!");
    REQUIRE_FALSE(short_lines.empty());
    CHECK(short_lines.size() == 1);

    const auto chunk = std::string("lorem ipsum dolor sit amet ");
    const auto long_text = chunk + chunk + chunk + chunk + chunk + chunk + chunk + chunk;
    const auto long_lines = speech_bubbles::wrap_text(long_text);
    REQUIRE(long_lines.size() == static_cast<size_t>(speech_bubbles::max_lines));
    CHECK(long_lines.back().find("...") != std::string::npos);
}

TEST_CASE("speech bubble list replaces the same speaker and culls by clock", "[speech_bubble]") {
    clear_all_state();
    auto& you = get_avatar();
    const auto t0 = std::chrono::steady_clock::now();

    speech_bubbles::add(you, "Hello", t0);
    REQUIRE(speech_bubbles::entries().size() == 1);
    CHECK(speech_bubbles::entries().front().text == "Hello");

    speech_bubbles::add(you, "Hello again", t0);
    REQUIRE(speech_bubbles::entries().size() == 1);
    CHECK(speech_bubbles::entries().front().text == "Hello again");

    speech_bubbles::cull(t0 + std::chrono::seconds(1));
    CHECK(speech_bubbles::entries().size() == 1);

    speech_bubbles::cull(t0 + std::chrono::seconds(20));
    CHECK(speech_bubbles::entries().empty());
}

TEST_CASE("quoted yells spawn bubbles and wordless yells do not", "[speech_bubble]") {
    clear_all_state();
    auto restore_tiles = restore_on_out_of_scope<bool>(use_tiles);
    use_tiles = true;
    override_option enabled("SPEECH_BUBBLES", "true");

    auto& you = get_avatar();
    you.shout("Stay close!");
    REQUIRE(speech_bubbles::entries().size() == 1);
    CHECK(speech_bubbles::entries().front().text == "Stay close!");
    CHECK(speech_bubbles::entries().front().speaker_is_avatar);

    speech_bubbles::clear();
    you.shout();
    CHECK(speech_bubbles::entries().empty());
}

TEST_CASE("speech bubbles honour the graphics option", "[speech_bubble]") {
    clear_all_state();
    auto restore_tiles = restore_on_out_of_scope<bool>(use_tiles);
    use_tiles = true;
    override_option disabled("SPEECH_BUBBLES", "false");

    get_avatar().shout("This should not appear");
    CHECK(speech_bubbles::entries().empty());
}

TEST_CASE("npc say spawns a bubble when the speaker is seen", "[speech_bubble]") {
    clear_all_state();
    set_time(calendar::turn_zero + 12_hours);
    auto restore_tiles = restore_on_out_of_scope<bool>(use_tiles);
    use_tiles = true;
    override_option enabled("SPEECH_BUBBLES", "true");

    auto& you = get_avatar();
    npc& guy = spawn_npc(you.bub_pos() + point_east, "test_talker");
    REQUIRE(you.sees(guy));

    guy.say("Look sharp!");
    REQUIRE(speech_bubbles::entries().size() == 1);
    CHECK(speech_bubbles::entries().front().text == "Look sharp!");
    CHECK_FALSE(speech_bubbles::entries().front().speaker_is_avatar);
}

TEST_CASE("a bubble fades only in its last moments", "[speech_bubble]") {
    using ms = std::chrono::milliseconds;
    const auto t0 = std::chrono::steady_clock::now();

    speech_bubbles::speech_bubble bubble;
    bubble.text = "Hello";
    bubble.born = t0;
    bubble.duration = ms(3000);

    // Solid for all but the tail.
    CHECK(speech_bubbles::fade_for(bubble, t0) == Approx(1.0f));
    CHECK(speech_bubbles::fade_for(bubble, t0 + ms(2000)) == Approx(1.0f));

    // Then linear to nothing.  A ratio taken from mismatched units landed near a
    // million here, which is undefined behaviour once cast to an alpha byte.
    const float half = speech_bubbles::fade_for(bubble, t0 + ms(3000) - speech_bubbles::fade_duration / 2);
    CHECK(half > 0.0f);
    CHECK(half < 1.0f);
    CHECK(half == Approx(0.5f).margin(0.01f));

    CHECK(speech_bubbles::fade_for(bubble, t0 + ms(3000)) == Approx(0.0f));
    CHECK(speech_bubbles::fade_for(bubble, t0 + ms(9000)) == Approx(0.0f));
}

TEST_CASE("a deaf player is not shown what they cannot hear", "[speech_bubble]") {
    clear_all_state();
    set_time(calendar::turn_zero + 12_hours);
    auto restore_tiles = restore_on_out_of_scope<bool>(use_tiles);
    use_tiles = true;
    override_option enabled("SPEECH_BUBBLES", "true");

    auto& you = get_avatar();
    npc& guy = spawn_npc(you.bub_pos() + point_east, "test_talker");
    REQUIRE(you.sees(guy));

    you.add_effect(efftype_id("deaf"), 10_minutes);
    REQUIRE(you.is_deaf());

    guy.say("Look sharp!");
    CHECK(speech_bubbles::entries().empty());

    // Their own words are still their own to know.
    you.shout("Stay close!");
    CHECK(speech_bubbles::entries().size() == 1);
}
