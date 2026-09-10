#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "calendar.h"
#include "color.h"
#include "flat_set.h"
#include "translations.h"
#include "type_id.h"

class Character;
class JsonObject;
class JsonOut;
struct display_proficiency;
struct learning_proficiency;
template<typename E> struct enum_traits;
template<typename T> class generic_factory;

/**
 * Proficiencies are knowledge a character has independently of skill level:
 * knowing *how* a thing is done rather than being good at doing it.  Ported
 * from Cataclysm: Dark Days Ahead, which Bright Nights never carried.
 *
 * A proficiency is either known or being learned.  Practising accumulates time
 * against @ref proficiency::time_to_learn, and a proficiency cannot be learned
 * until its prerequisites are.
 */
enum class proficiency_bonus_type : int {
    strength,
    dexterity,
    intelligence,
    perception,
    stamina,
    last
};

template<>
struct enum_traits<proficiency_bonus_type> {
    static constexpr proficiency_bonus_type last = proficiency_bonus_type::last;
};

struct proficiency_bonus {
    proficiency_bonus_type type = proficiency_bonus_type::last;
    float value = 0.0f;

    void deserialize( const JsonObject &jo );
};

struct proficiency_category {
    proficiency_category_id id;
    translation _name;
    translation _description;
    bool was_loaded = false;

    static void load_proficiency_categories( const JsonObject &jo, const std::string &src );
    static void reset();
    void load( const JsonObject &jo, const std::string &src );
    static const std::vector<proficiency_category> &get_all();

    std::string name() const;
    std::string description() const;
};

class proficiency
{
        friend class generic_factory<proficiency>;

        bool _can_learn = false;
        bool _ignore_focus = false;
        bool _teachable = true;

        proficiency_category_id _category;

        translation _name;
        translation _description;

        float _default_time_multiplier = 2.0f;
        float _default_skill_penalty = 1.0f;

        // Parsed for data compatibility with the upstream JSON.  Bright Nights has
        // no weakpoint system, so nothing reads these yet.
        float _default_weakpoint_bonus = 0.0f;
        float _default_weakpoint_penalty = 0.0f;

        time_duration _time_to_learn = 9999_hours;
        std::set<proficiency_id> _required;

        std::map<std::string, std::vector<proficiency_bonus>> _bonuses;

    public:
        proficiency_id id;
        bool was_loaded = false;

        static void load_proficiencies( const JsonObject &jo, const std::string &src );
        static void reset();
        void load( const JsonObject &jo, const std::string &src );

        static const std::vector<proficiency> &get_all();
        static void check_consistency();
        void check() const;

        bool can_learn() const;
        bool ignore_focus() const;
        bool is_teachable() const;
        proficiency_id prof_id() const;
        proficiency_category_id prof_category() const;
        std::string name() const;
        std::string description() const;

        float default_time_multiplier() const;
        float default_skill_penalty() const;

        time_duration time_to_learn() const;
        std::set<proficiency_id> required_proficiencies() const;

        std::vector<proficiency_bonus> get_bonuses( const std::string &category ) const;
        std::optional<float> bonus_for( const std::string &category,
                                        proficiency_bonus_type type ) const;
};

/** A proficiency a character is part-way through learning. */
struct learning_proficiency {
    proficiency_id id;

    /** How long this proficiency has been practised for. */
    time_duration practiced;
    /** Sub-second remainder, so very brief practice is not truncated away. */
    float remainder = 0.0f;

    learning_proficiency() = default;
    learning_proficiency( const proficiency_id &id, const time_duration &practiced )
        : id( id ), practiced( practiced ) {}

    void serialize( JsonOut &jsout ) const;
    void deserialize( const JsonObject &jo );
};

/** One row of the proficiency display: what it is and how far along we are. */
struct display_proficiency {
    proficiency_id id;
    nc_color color = c_white;
    float practice = 0.0f;
    time_duration spent = 0_turns;
    bool known = false;
};

/** The proficiencies a character knows, and the ones they are learning. */
class proficiency_set
{
        cata::flat_set<proficiency_id> known;
        std::vector<learning_proficiency> learning;

        learning_proficiency &fetch_learning( const proficiency_id &target );

    public:
        void clear();

        std::vector<display_proficiency> display() const;

        /**
         * Practise @p practicing for @p amount of time.
         * @param max caps how much practice this source can ever contribute.
         * @return true if the proficiency became known as a result.
         */
        bool practice( const proficiency_id &practicing, const time_duration &amount,
                       float remainder = 0.0f,
                       const std::optional<time_duration> &max = std::nullopt );
        void learn( const proficiency_id &learned, bool recursive = false );
        void remove( const proficiency_id &lost );

        /** Learn or remove ignoring prerequisites.  For debugging. */
        void direct_learn( const proficiency_id &learned );
        void direct_remove( const proficiency_id &lost );

        bool has_learned( const proficiency_id &query ) const;
        bool has_practiced( const proficiency_id &query ) const;
        bool has_prereqs( const proficiency_id &query ) const;

        float pct_practiced( const proficiency_id &query ) const;
        time_duration pct_practiced_time( const proficiency_id &query ) const;
        void set_time_practiced( const proficiency_id &practicing, const time_duration &amount );
        time_duration training_time_needed( const proficiency_id &query ) const;

        std::vector<proficiency_id> known_profs() const;
        std::vector<proficiency_id> learning_profs() const;

        float get_proficiency_bonus( const std::string &category,
                                     proficiency_bonus_type prof_bonus ) const;

        void serialize( JsonOut &jsout ) const;
        void deserialize( const JsonObject &jsobj );
};

/** Browse the proficiencies @p u knows and is learning.  Read-only. */
void show_proficiencies_window( const Character &u );
