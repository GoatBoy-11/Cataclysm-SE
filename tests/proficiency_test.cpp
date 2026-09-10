#include "catch/catch.hpp"

#include "calendar.h"
#include "json.h"
#include "avatar.h"
#include "character.h"
#include "profession.h"
#include "proficiency.h"
#include "recipe.h"
#include "type_id.h"

#include <sstream>
#include <string>

// A short chain from the shipped data: the "pro" proficiency requires the
// "familiar" one, which has no prerequisites of its own.
static const proficiency_id prof_familiar( "prof_auto_rifles_familiar" );
static const proficiency_id prof_pro( "prof_auto_rifles_pro" );

TEST_CASE("proficiency data loads", "[proficiency]") {
    REQUIRE(!proficiency::get_all().empty());

    CHECK(prof_familiar.is_valid());
    CHECK(prof_pro.is_valid());
    CHECK(prof_pro->required_proficiencies().count(prof_familiar) == 1);
}

TEST_CASE("proficiency defaults survive absent json keys", "[proficiency]") {
    // The three-argument form of optional() value-initializes the member when the
    // key is missing, which silently zeroed these for every proficiency that omits
    // them. Two omit time_to_learn and forty omit the multipliers.
    for (const proficiency &prof : proficiency::get_all()) {
        INFO("proficiency: " << prof.prof_id().str());
        CHECK(prof.time_to_learn() > 0_seconds);
        CHECK(prof.default_time_multiplier() > 0.0f);
    }

    // prof_aircraft_mechanic states no learning time and cannot be learned.
    const proficiency_id aircraft("prof_aircraft_mechanic");
    REQUIRE(aircraft.is_valid());
    CHECK(!aircraft->can_learn());
    CHECK(aircraft->time_to_learn() == 9999_hours);
}

TEST_CASE("practising a proficiency eventually learns it", "[proficiency]") {
    proficiency_set profs;
    REQUIRE(!profs.has_learned(prof_familiar));

    const time_duration needed = prof_familiar->time_to_learn();
    REQUIRE(needed > 0_seconds);

    // Short of the requirement it is in progress, not known.
    CHECK(!profs.practice(prof_familiar, needed - 1_seconds));
    CHECK(!profs.has_learned(prof_familiar));
    CHECK(profs.has_practiced(prof_familiar));
    CHECK(profs.pct_practiced(prof_familiar) > 0.0f);
    CHECK(profs.pct_practiced(prof_familiar) < 1.0f);

    // Crossing it moves the proficiency from learning to known.
    CHECK(profs.practice(prof_familiar, 1_seconds));
    CHECK(profs.has_learned(prof_familiar));
    CHECK(!profs.has_practiced(prof_familiar));
    CHECK(profs.pct_practiced(prof_familiar) == 1.0f);
}

TEST_CASE("proficiency prerequisites gate learning", "[proficiency]") {
    proficiency_set profs;

    CHECK(!profs.has_prereqs(prof_pro));

    // Practice is refused outright while the prerequisite is missing.
    CHECK(!profs.practice(prof_pro, prof_pro->time_to_learn()));
    CHECK(!profs.has_learned(prof_pro));

    // A direct learn without the prerequisite is a no-op.
    profs.learn(prof_pro);
    CHECK(!profs.has_learned(prof_pro));

    // Recursively, it picks up the prerequisite too.
    profs.learn(prof_pro, true);
    CHECK(profs.has_learned(prof_pro));
    CHECK(profs.has_learned(prof_familiar));
}

TEST_CASE("losing a proficiency loses what depended on it", "[proficiency]") {
    proficiency_set profs;
    profs.learn(prof_pro, true);
    REQUIRE(profs.has_learned(prof_familiar));
    REQUIRE(profs.has_learned(prof_pro));

    profs.remove(prof_familiar);

    CHECK(!profs.has_learned(prof_familiar));
    CHECK(!profs.has_learned(prof_pro));
}

TEST_CASE("proficiency set survives a save round trip", "[proficiency]") {
    proficiency_set before;
    before.learn(prof_familiar);
    before.practice(prof_pro, 5_minutes);
    REQUIRE(before.has_learned(prof_familiar));
    REQUIRE(before.has_practiced(prof_pro));

    std::ostringstream os;
    JsonOut jsout(os);
    before.serialize(jsout);

    std::istringstream is(os.str());
    JsonIn jsin(is);
    JsonObject jo = jsin.get_object();
    proficiency_set after;
    after.deserialize(jo);

    CHECK(after.has_learned(prof_familiar));
    CHECK(after.has_practiced(prof_pro));
    CHECK(after.pct_practiced_time(prof_pro) == 5_minutes);
}

TEST_CASE("a save with no proficiencies loads as an empty set", "[proficiency]") {
    // Saves written before this port carry no "proficiencies" member at all.
    std::istringstream is("{}");
    JsonIn jsin(is);
    JsonObject jo = jsin.get_object();

    proficiency_set profs;
    profs.deserialize(jo);

    CHECK(profs.known_profs().empty());
    CHECK(profs.learning_profs().empty());
}

TEST_CASE("professions grant proficiencies", "[proficiency]") {
    // Imported from the matching CDDA professions, so the grant lists are theirs.
    const string_id<profession> smith("blacksmith");
    REQUIRE(smith.is_valid());

    const std::vector<proficiency_id> &granted = smith->proficiencies();
    REQUIRE(!granted.empty());

    bool has_blacksmithing = false;
    for (const proficiency_id &p : granted) {
        INFO("granted: " << p.str());
        CHECK(p.is_valid());
        if (p == proficiency_id("prof_blacksmithing")) {
            has_blacksmithing = true;
        }
    }
    CHECK(has_blacksmithing);
}

TEST_CASE("recipe proficiencies penalise a character who lacks them", "[proficiency]") {
    const recipe_id rid("arrow_small_game_fletched");
    REQUIRE(rid.is_valid());
    const recipe &rec = *rid;
    REQUIRE(!rec.proficiencies.empty());

    const std::vector<proficiency_id> used = rec.used_proficiencies();
    REQUIRE(!used.empty());

    Character &you = get_avatar();
    for (const proficiency_id &p : used) {
        you.lose_proficiency(p, true);
    }

    // Missing them costs time and effective skill.
    CHECK(rec.proficiency_time_maluses(you) > 1.0f);
    CHECK(rec.proficiency_skill_maluses(you) > 0.0f);

    for (const proficiency_id &p : used) {
        you.add_proficiency(p, true);
    }

    // Knowing them costs nothing at all.
    CHECK(rec.proficiency_time_maluses(you) == Approx(1.0f));
    CHECK(rec.proficiency_skill_maluses(you) == Approx(0.0f));

    // Nothing here is required, so the recipe is never gated.
    CHECK(rec.character_has_required_proficiencies(you));

    for (const proficiency_id &p : used) {
        you.lose_proficiency(p, true);
    }
}

TEST_CASE("a character practises a proficiency until they know it", "[proficiency]") {
    Character &you = get_avatar();
    you.lose_proficiency(prof_familiar, true);
    REQUIRE(!you.has_proficiency(prof_familiar));

    const time_duration needed = prof_familiar->time_to_learn();

    // Part-way there is progress, not knowledge.
    CHECK(!you.practice_proficiency(prof_familiar, needed / 2));
    CHECK(!you.has_proficiency(prof_familiar));
    CHECK(you.get_proficiency_practice(prof_familiar) > 0.0f);

    // Finishing the required time learns it, and it then shows in the display.
    CHECK(you.practice_proficiency(prof_familiar, needed));
    CHECK(you.has_proficiency(prof_familiar));
    CHECK(you.get_proficiency_practice(prof_familiar) == Approx(1.0f));

    bool listed = false;
    for (const display_proficiency &d : you.display_proficiencies()) {
        if (d.id == prof_familiar) {
            listed = true;
            CHECK(d.known);
        }
    }
    CHECK(listed);

    you.lose_proficiency(prof_familiar, true);
}

TEST_CASE("weapon proficiencies make a familiar weapon cheaper to swing", "[proficiency]") {
    const proficiency_id knives_familiar("prof_knives_familiar");
    REQUIRE(knives_familiar.is_valid());

    Character &you = get_avatar();
    you.lose_proficiency(knives_familiar, true);

    detached_ptr<item> knife = item::spawn("knife_combat");
    REQUIRE(knife);

    const int untrained = you.get_melee_stamina_cost(*knife);
    you.add_proficiency(knives_familiar, true);
    const int trained = you.get_melee_stamina_cost(*knife);

    // The bonus lives on the proficiency, reached through the weapon's category.
    CHECK(trained < untrained);

    you.lose_proficiency(knives_familiar, true);
    CHECK(you.get_melee_stamina_cost(*knife) == untrained);
}

TEST_CASE("books soften a proficiency you lack", "[proficiency]") {
    // No books means no mitigation at all.
    book_proficiency_bonuses none;
    CHECK(none.time_factor(prof_familiar) == Approx(0.0f));
    CHECK(none.fail_factor(prof_familiar) == Approx(0.0f));

    book_proficiency_bonus half;
    half.id = prof_familiar;
    half.time_factor = 0.5f;
    half.fail_factor = 0.5f;
    half.include_prereqs = false;

    book_proficiency_bonuses one;
    one.add(half);
    const float one_book = one.time_factor(prof_familiar);
    CHECK(one_book > 0.0f);
    CHECK(one_book < 1.0f);

    // A second book helps more, but the mitigation never reaches whole.
    book_proficiency_bonuses two;
    two.add(half);
    two.add(half);
    const float two_books = two.time_factor(prof_familiar);
    CHECK(two_books > one_book);
    CHECK(two_books < 1.0f);

    // A book about one proficiency says nothing about another.
    CHECK(one.time_factor(prof_pro) == Approx(0.0f));
}
