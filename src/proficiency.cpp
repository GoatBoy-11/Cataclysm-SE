#include "proficiency.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <utility>

#include "calendar.h"
#include "debug.h"
#include "enum_conversions.h"
#include "generic_factory.h"
#include "json.h"
#include "string_id.h"
#include "translations.h"
#include "type_id.h"
#include "type_id_implement.h"

namespace
{
generic_factory<proficiency> proficiency_factory( "proficiency" );
generic_factory<proficiency_category> proficiency_category_factory( "proficiency_category" );
} // namespace

IMPLEMENT_STRING_ID( proficiency, proficiency_factory )
IMPLEMENT_STRING_ID( proficiency_category, proficiency_category_factory )

namespace io
{
template<>
std::string enum_to_string<proficiency_bonus_type>( const proficiency_bonus_type data )
{
    switch( data ) {
        case proficiency_bonus_type::strength:
            return "strength";
        case proficiency_bonus_type::dexterity:
            return "dexterity";
        case proficiency_bonus_type::intelligence:
            return "intelligence";
        case proficiency_bonus_type::perception:
            return "perception";
        case proficiency_bonus_type::stamina:
            return "stamina";
        case proficiency_bonus_type::last:
            break;
    }
    debugmsg( "Invalid proficiency_bonus_type" );
    abort();
}
} // namespace io

void proficiency_bonus::deserialize( const JsonObject &jo )
{
    mandatory( jo, false, "type", type );
    mandatory( jo, false, "value", value );
}

void proficiency::load_proficiencies( const JsonObject &jo, const std::string &src )
{
    proficiency_factory.load( jo, src );
}

void proficiency_category::load_proficiency_categories( const JsonObject &jo,
        const std::string &src )
{
    proficiency_category_factory.load( jo, src );
}

void proficiency::reset()
{
    proficiency_factory.reset();
}

void proficiency_category::reset()
{
    proficiency_category_factory.reset();
}

void proficiency::load( const JsonObject &jo, const std::string & )
{
    mandatory( jo, was_loaded, "name", _name );
    mandatory( jo, was_loaded, "description", _description );
    mandatory( jo, was_loaded, "can_learn", _can_learn );
    mandatory( jo, was_loaded, "category", _category );

    // These need their defaults spelled out: the three-argument optional() assigns a
    // value-initialized member when the key is absent, rather than leaving the
    // in-class initializer alone.  40 proficiencies omit the two multipliers and two
    // omit the learning time.
    optional( jo, was_loaded, "default_time_multiplier", _default_time_multiplier, 2.0f );
    optional( jo, was_loaded, "default_skill_penalty", _default_skill_penalty, 1.0f );
    optional( jo, was_loaded, "default_weakpoint_bonus", _default_weakpoint_bonus, 0.0f );
    optional( jo, was_loaded, "default_weakpoint_penalty", _default_weakpoint_penalty, 0.0f );
    optional( jo, was_loaded, "time_to_learn", _time_to_learn, 9999_hours );
    optional( jo, was_loaded, "required_proficiencies", _required );
    optional( jo, was_loaded, "ignore_focus", _ignore_focus );
    optional( jo, was_loaded, "teachable", _teachable, true );

    optional( jo, was_loaded, "bonuses", _bonuses );
}

void proficiency_category::load( const JsonObject &jo, const std::string & )
{
    mandatory( jo, was_loaded, "name", _name );
    mandatory( jo, was_loaded, "description", _description );
}

const std::vector<proficiency> &proficiency::get_all()
{
    return proficiency_factory.get_all();
}

const std::vector<proficiency_category> &proficiency_category::get_all()
{
    return proficiency_category_factory.get_all();
}

void proficiency::check_consistency()
{
    proficiency_factory.check();
}

void proficiency::check() const
{
    if( !_category.is_empty() && !_category.is_valid() ) {
        debugmsg( "proficiency %s has unknown category %s", id.str(), _category.str() );
    }
    for( const proficiency_id &req : _required ) {
        if( !req.is_valid() ) {
            debugmsg( "proficiency %s requires unknown proficiency %s", id.str(), req.str() );
        } else if( req == id ) {
            debugmsg( "proficiency %s requires itself", id.str() );
        }
    }
    // Only meaningful for proficiencies you can actually train; the rest are granted.
    if( _can_learn && _time_to_learn <= 0_seconds ) {
        debugmsg( "proficiency %s is learnable but takes no time to learn", id.str() );
    }
}

bool proficiency::can_learn() const
{
    return _can_learn;
}

bool proficiency::ignore_focus() const
{
    return _ignore_focus;
}

bool proficiency::is_teachable() const
{
    return _teachable;
}

proficiency_id proficiency::prof_id() const
{
    return id;
}

proficiency_category_id proficiency::prof_category() const
{
    return _category;
}

std::string proficiency::name() const
{
    return _name.translated();
}

std::string proficiency::description() const
{
    return _description.translated();
}

float proficiency::default_time_multiplier() const
{
    return _default_time_multiplier;
}

float proficiency::default_skill_penalty() const
{
    return _default_skill_penalty;
}

time_duration proficiency::time_to_learn() const
{
    return _time_to_learn;
}

std::set<proficiency_id> proficiency::required_proficiencies() const
{
    return _required;
}

std::vector<proficiency_bonus> proficiency::get_bonuses( const std::string &category ) const
{
    const auto it = _bonuses.find( category );
    if( it == _bonuses.end() ) {
        return {};
    }
    return it->second;
}

std::optional<float> proficiency::bonus_for( const std::string &category,
        const proficiency_bonus_type type ) const
{
    for( const proficiency_bonus &bonus : get_bonuses( category ) ) {
        if( bonus.type == type ) {
            return bonus.value;
        }
    }
    return std::nullopt;
}

std::string proficiency_category::name() const
{
    return _name.translated();
}

std::string proficiency_category::description() const
{
    return _description.translated();
}

learning_proficiency &proficiency_set::fetch_learning( const proficiency_id &target )
{
    for( learning_proficiency &cursor : learning ) {
        if( cursor.id == target ) {
            return cursor;
        }
    }

    // This should _never_ happen
    debugmsg( "Uh-oh!  Requested proficiency that character does not know"
              " - expect crash or undefined behavior" );
    return learning[0];
}

void proficiency_set::clear()
{
    known.clear();
    learning.clear();
}

std::vector<display_proficiency> proficiency_set::display() const
{
    // Sorted by whether or not you know them, and then alphabetically
    std::vector<std::pair<std::string, proficiency_id>> sorted_known;
    std::vector<std::pair<std::string, proficiency_id>> sorted_learning;

    sorted_known.reserve( known.size() );
    for( const proficiency_id &cur : known ) {
        sorted_known.emplace_back( cur->name(), cur );
    }

    sorted_learning.reserve( learning.size() );
    for( const learning_proficiency &cur : learning ) {
        sorted_learning.emplace_back( cur.id->name(), cur.id );
    }

    std::sort( sorted_known.begin(), sorted_known.end(), localized_compare );
    std::sort( sorted_learning.begin(), sorted_learning.end(), localized_compare );

    std::vector<display_proficiency> ret;
    ret.reserve( sorted_known.size() + sorted_learning.size() );

    for( const std::pair<std::string, proficiency_id> &cur : sorted_known ) {
        display_proficiency disp;
        disp.id = cur.second;
        disp.color = c_white;
        disp.practice = 1.0f;
        disp.spent = cur.second->time_to_learn();
        disp.known = true;
        ret.push_back( disp );
    }

    for( const std::pair<std::string, proficiency_id> &cur : sorted_learning ) {
        display_proficiency disp;
        disp.id = cur.second;
        disp.color = c_light_gray;
        time_duration practiced = 0_seconds;
        for( const learning_proficiency &cursor : learning ) {
            if( cursor.id == cur.second ) {
                practiced = cursor.practiced;
                break;
            }
        }
        disp.spent = practiced;
        disp.practice = practiced / cur.second->time_to_learn();
        disp.known = false;
        ret.push_back( disp );
    }

    return ret;
}

bool proficiency_set::practice( const proficiency_id &practicing, const time_duration &amount,
                                const float remainder, const std::optional<time_duration> &max )
{
    if( has_learned( practicing ) || !practicing->can_learn() || !has_prereqs( practicing ) ) {
        return false;
    }
    if( !has_practiced( practicing ) ) {
        learning.emplace_back( practicing, 0_seconds );
    }

    learning_proficiency &current = fetch_learning( practicing );

    if( max && current.practiced > *max ) {
        return false;
    }

    current.practiced += amount;
    current.remainder += remainder;
    if( current.remainder > 1.0f ) {
        current.practiced += 1_seconds;
        current.remainder -= 1.0f;
    }

    if( current.practiced >= practicing->time_to_learn() ) {
        std::erase_if( learning, [&practicing]( const learning_proficiency & it ) {
            return it.id == practicing;
        } );
        learn( practicing );

        return true;
    }

    return false;
}

void proficiency_set::set_time_practiced( const proficiency_id &practicing,
        const time_duration &amount )
{
    if( amount >= practicing->time_to_learn() ) {
        std::erase_if( learning, [&practicing]( const learning_proficiency & it ) {
            return it.id == practicing;
        } );
        learn( practicing );
        return;
    } else if( known.count( practicing ) ) {
        remove( practicing );
    }
    if( !has_practiced( practicing ) ) {
        learning.emplace_back( practicing, 0_seconds );
    }
    learning_proficiency &current = fetch_learning( practicing );
    current.practiced = amount;
}

void proficiency_set::learn( const proficiency_id &learned, const bool recursive )
{
    for( const proficiency_id &req : learned->required_proficiencies() ) {
        if( !has_learned( req ) && !recursive ) {
            return;
        } else if( recursive ) {
            learn( req, recursive );
        }
    }
    known.insert( learned );
}

/** All the proficiencies in @p subjects that require one of @p requirements. */
static std::set<proficiency_id> proficiencies_requiring(
    const std::set<proficiency_id> &requirements,
    const cata::flat_set<proficiency_id> &subjects )
{
    std::set<proficiency_id> ret;
    for( const proficiency_id &candidate : subjects ) {
        for( const proficiency_id &selector : requirements ) {
            if( candidate->required_proficiencies().count( selector ) ) {
                ret.insert( candidate );
                break;
            }
        }
    }
    return ret;
}

void proficiency_set::remove( const proficiency_id &lost )
{
    std::erase_if( learning, [&lost]( const learning_proficiency & it ) {
        return it.id == lost;
    } );

    // No unintended side effects
    if( !known.count( lost ) ) {
        return;
    }

    // Anything that required the lost proficiency goes too, and anything that
    // required those, until the set stops growing.
    std::set<proficiency_id> to_remove;
    to_remove.insert( lost );
    size_t cached_size = 0;
    while( to_remove.size() != cached_size ) {
        cached_size = to_remove.size();
        std::set<proficiency_id> additional = proficiencies_requiring( to_remove, known );
        to_remove.insert( additional.begin(), additional.end() );
    }

    for( const proficiency_id &gone : to_remove ) {
        known.erase( gone );
    }
}

void proficiency_set::direct_learn( const proficiency_id &learned )
{
    std::erase_if( learning, [&learned]( const learning_proficiency & it ) {
        return it.id == learned;
    } );

    known.insert( learned );
}

void proficiency_set::direct_remove( const proficiency_id &lost )
{
    known.erase( lost );
}

bool proficiency_set::has_learned( const proficiency_id &query ) const
{
    return known.count( query );
}

bool proficiency_set::has_practiced( const proficiency_id &query ) const
{
    for( const learning_proficiency &cursor : learning ) {
        if( cursor.id == query ) {
            return true;
        }
    }
    return false;
}

bool proficiency_set::has_prereqs( const proficiency_id &query ) const
{
    for( const proficiency_id &req : query->required_proficiencies() ) {
        if( !has_learned( req ) ) {
            return false;
        }
    }
    return true;
}

float proficiency_set::pct_practiced( const proficiency_id &query ) const
{
    for( const learning_proficiency &prof : learning ) {
        if( prof.id == query ) {
            return prof.practiced / query->time_to_learn();
        }
    }
    if( has_learned( query ) ) {
        return 1.0f;
    }
    return 0.0f;
}

time_duration proficiency_set::pct_practiced_time( const proficiency_id &query ) const
{
    for( const learning_proficiency &prof : learning ) {
        if( prof.id == query ) {
            return prof.practiced;
        }
    }
    if( has_learned( query ) ) {
        return query->time_to_learn();
    }
    return 0_seconds;
}

time_duration proficiency_set::training_time_needed( const proficiency_id &query ) const
{
    for( const learning_proficiency &prof : learning ) {
        if( prof.id == query ) {
            return query->time_to_learn() - prof.practiced;
        }
    }
    return query->time_to_learn();
}

std::vector<proficiency_id> proficiency_set::known_profs() const
{
    std::vector<proficiency_id> ret;
    ret.reserve( known.size() );
    for( const proficiency_id &knows : known ) {
        ret.push_back( knows );
    }
    return ret;
}

std::vector<proficiency_id> proficiency_set::learning_profs() const
{
    std::vector<proficiency_id> ret;
    ret.reserve( learning.size() );
    for( const learning_proficiency &subject : learning ) {
        ret.push_back( subject.id );
    }
    return ret;
}

float proficiency_set::get_proficiency_bonus( const std::string &category,
        const proficiency_bonus_type prof_bonus ) const
{
    float stat_bonus = 0.0f;

    for( const proficiency_id &knows : known ) {
        for( const proficiency_bonus &bonus : knows->get_bonuses( category ) ) {
            if( bonus.type == prof_bonus ) {
                stat_bonus += bonus.value;
            }
        }
    }
    return stat_bonus;
}

void proficiency_set::serialize( JsonOut &jsout ) const
{
    jsout.start_object();

    jsout.member( "known", known );
    jsout.member( "learning", learning );

    jsout.end_object();
}

void proficiency_set::deserialize( const JsonObject &jsobj )
{
    jsobj.read( "known", known );
    jsobj.read( "learning", learning );
}

void book_proficiency_bonus::deserialize( const JsonObject &jo )
{
    mandatory( jo, was_loaded, "proficiency", id );
    optional( jo, was_loaded, "fail_factor", fail_factor, 0.5f );
    optional( jo, was_loaded, "time_factor", time_factor, 0.5f );
    optional( jo, was_loaded, "include_prereqs", include_prereqs, true );
    if( fail_factor < 0.0f || fail_factor >= 1.0f ) {
        jo.throw_error( "fail_factor must be in range [0,1)" );
    }
    if( time_factor < 0.0f || time_factor >= 1.0f ) {
        jo.throw_error( "time_factor must be in range [0,1)" );
    }
}

void book_proficiency_bonuses::add( const book_proficiency_bonus &bonus )
{
    std::set<proficiency_id> seen;
    add( bonus, seen );
}

void book_proficiency_bonuses::add( const book_proficiency_bonus &bonus,
                                    std::set<proficiency_id> &already_included )
{
    bonuses.push_back( bonus );
    if( !bonus.include_prereqs || !bonus.id.is_valid() ) {
        return;
    }
    // A book that covers a proficiency implicitly covers what it is built on.
    for( const proficiency_id &prereq : bonus.id->required_proficiencies() ) {
        if( already_included.insert( prereq ).second ) {
            book_proficiency_bonus inherited = bonus;
            inherited.id = prereq;
            add( inherited, already_included );
        }
    }
}

book_proficiency_bonuses &book_proficiency_bonuses::operator+=(
    const book_proficiency_bonuses &rhs )
{
    for( const book_proficiency_bonus &bonus : rhs.bonuses ) {
        add( bonus );
    }
    return *this;
}

/** Combine factors so two half-helpful books beat one, without ever reaching 1. */
static float combine_factors( const std::vector<book_proficiency_bonus> &bonuses,
                              const proficiency_id &id, const bool want_fail )
{
    double sum = 0.0;
    for( const book_proficiency_bonus &bonus : bonuses ) {
        if( id != bonus.id ) {
            continue;
        }
        const double f = want_fail ? bonus.fail_factor : bonus.time_factor;
        sum += std::pow( std::log( 1.0 - f ), 2 );
    }
    return static_cast<float>( 1.0 - std::exp( -std::sqrt( sum ) ) );
}

float book_proficiency_bonuses::fail_factor( const proficiency_id &id ) const
{
    return combine_factors( bonuses, id, true );
}

float book_proficiency_bonuses::time_factor( const proficiency_id &id ) const
{
    return combine_factors( bonuses, id, false );
}

void learning_proficiency::serialize( JsonOut &jsout ) const
{
    jsout.start_object();

    jsout.member( "id", id );
    jsout.member( "practiced", practiced );
    jsout.member( "remainder", remainder );

    jsout.end_object();
}

void learning_proficiency::deserialize( const JsonObject &jo )
{
    jo.read( "id", id );
    jo.read( "practiced", practiced );
    jo.read( "remainder", remainder, 0.0f );
}
