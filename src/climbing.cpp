#include "climbing.h"

#include <cstdint>
#include <utility>

#include "cata_utility.h"
#include "character.h"
#include "creature_tracker.h"
#include "cata_unreachable.h"
#include "debug.h"
#include "enum_conversions.h"
#include "generic_factory.h"
#include "json.h"
#include "map.h"
#include "type_id.h"
#include "type_id_implement.h"

static const climbing_aid_id climbing_aid_default( "default" );

template<>
struct enum_traits<climbing_aid::category> {
    static constexpr climbing_aid::category last = climbing_aid::category::last;
};

namespace
{
generic_factory<climbing_aid> climbing_aid_factory( "climbing_aid" );
} // namespace

static climbing_aid::lookup climbing_lookup;
static const climbing_aid *climbing_aid_default_ptr = nullptr;

IMPLEMENT_STRING_AND_INT_IDS( climbing_aid, climbing_aid_factory )

auto climbing_aid::get_all() -> const std::vector<climbing_aid> &
{
    return climbing_aid_factory.get_all();
}

auto climbing_aid::load_climbing_aid( const JsonObject &jo, const std::string &src ) -> void
{
    climbing_aid_factory.load( jo, src );
}

auto climbing_aid::finalize() -> void
{
}

auto climbing_aid::finalize_all() -> void
{
    climbing_aid_factory.finalize();
    climbing_aid_default_ptr = nullptr;
    climbing_lookup.clear();
    climbing_lookup.resize( static_cast<size_t>( category::last ) );

    for( const climbing_aid &aid : get_all() ) {
        const int category_index = static_cast<int>( aid.base_condition.cat );
        if( category_index >= static_cast<int>( climbing_lookup.size() ) ) {
            debugmsg( "Climbing aid %s has invalid condition type.", aid.id.str() );
            continue;
        }
        climbing_lookup[ category_index ].emplace( aid.base_condition.flag, &aid );

        if( aid.id.str() == "default" ) {
            climbing_aid_default_ptr = &aid;
        }
    }

    if( !climbing_aid_default_ptr ) {
        static climbing_aid def;
        def.id = climbing_aid_default;
        def.slip_chance_mod = 0;
        def.base_condition.cat = category::special;
        def.down.menu_text = to_translation( "Climb down by lowering yourself from the ledge." );
        def.down.menu_hotkey = 'c';
        def.down.confirm_text = to_translation( "Climb down the ledge?" );
        def.down.msg_after = to_translation( "You lower yourself from the ledge." );
        def.down.max_height = 1;
        def.was_loaded = false;
        def.down.was_loaded = true;
        climbing_aid_default_ptr = &def;
    }
}

auto climbing_aid::check_consistency() -> void
{
    if( !climbing_aid_default || !climbing_aid_default->was_loaded ) {
        debugmsg( "Default climbing aid was not defined." );
    }
}

auto climbing_aid::reset() -> void
{
    climbing_aid_factory.reset();
    climbing_lookup.clear();
    climbing_aid_default_ptr = nullptr;
}

auto climbing_aid::load( const JsonObject &jo, std::string_view ) -> void
{
    optional( jo, was_loaded, "slip_chance_mod", slip_chance_mod );
    mandatory( jo, was_loaded, "down", down );
    mandatory( jo, was_loaded, "condition", base_condition );

    was_loaded = true;
}

namespace io
{
template<>
std::string enum_to_string<climbing_aid::category>( climbing_aid::category data )
{
    switch( data ) {
        case climbing_aid::category::special:
            return "special";
        case climbing_aid::category::ter_furn:
            return "ter_furn";
        case climbing_aid::category::veh:
            return "veh";
        case climbing_aid::category::item:
            return "item";
        case climbing_aid::category::character:
            return "character";
        case climbing_aid::category::trait:
            return "trait";
        case climbing_aid::category::last:
            break;
    }
    debugmsg( "Invalid climbing aid condition category" );
    abort();
}
} // namespace io

auto climbing_aid::condition::category_string() const noexcept -> std::string
{
    return io::enum_to_string( cat );
}

auto climbing_aid::condition::deserialize( const JsonObject &jo ) -> void
{
    mandatory( jo, false, "type", cat );
    mandatory( jo, false, "flag", flag );
    switch( cat ) {
        case category::item:
            mandatory( jo, false, "uses", uses_item );
            break;
        case category::ter_furn:
            optional( jo, false, "range", range );
            break;
        default:
            break;
    }
}

auto climbing_aid::down_t::deserialize( const JsonObject &jo ) -> void
{
    optional( jo, true, "max_height", max_height );
    optional( jo, true, "easy_climb_back_up", easy_climb_back_up );
    optional( jo, true, "allow_remaining_height", allow_remaining_height );

    if( enabled() ) {
        optional( jo, true, "cost", cost );
        optional( jo, true, "deploy_furn", deploy_furn );

        mandatory( jo, was_loaded, "menu_text", menu_text );
        mandatory( jo, was_loaded, "confirm_text", confirm_text );
        std::string menu_hotkey_str;
        if( deploys_furniture() ) {
            mandatory( jo, was_loaded, "menu_cant", menu_cant );
            mandatory( jo, was_loaded, "menu_hotkey", menu_hotkey_str );
            if( menu_hotkey_str.length() != 1 ) {
                jo.throw_error( "failed to read mandatory member \"menu_hotkey\"" );
            }
        } else {
            optional( jo, was_loaded, "menu_cant", menu_cant );
            optional( jo, was_loaded, "menu_hotkey", menu_hotkey_str );
            if( menu_hotkey_str.length() > 1 ) {
                jo.throw_error( "failed to read optional member \"menu_hotkey\"" );
            }
        }
        if( !menu_hotkey_str.empty() ) {
            menu_hotkey = static_cast<std::uint8_t>( menu_hotkey_str[ 0 ] );
        }

        optional( jo, true, "msg_before", msg_before );
        optional( jo, true, "msg_after", msg_after );
    }
}

auto climbing_aid::climb_cost::deserialize( const JsonObject &jo ) -> void
{
    optional( jo, true, "kcal", kcal );
    optional( jo, true, "thirst", thirst );
    optional( jo, true, "damage", damage );
    optional( jo, true, "pain", pain );
}

template<typename Lambda>
static auto for_each_aid_condition( const climbing_aid::condition_list &conditions,
                                    const Lambda &func ) -> void
{
    for( const climbing_aid::condition &cond : conditions ) {
        if( static_cast<int>( cond.cat ) < static_cast<int>( climbing_lookup.size() ) ) {
            auto &cat = climbing_lookup[ static_cast<int>( cond.cat ) ];
            const auto range = cat.equal_range( cond.flag );
            for( auto i = range.first; i != range.second; ++i ) {
                func( cond, *i->second );
            }
        }
    }
}

auto climbing_aid::list( const condition_list &conditions ) -> aid_list
{
    aid_list list;

    const auto add_deployables = [&list]( const condition &, const climbing_aid & aid ) {
        if( aid.down.deploys_furniture() ) {
            list.push_back( &aid );
        }
    };
    for_each_aid_condition( conditions, add_deployables );

    list.push_back( &get_safest( conditions, true ) );

    return list;
}

auto climbing_aid::list_all( const condition_list &conditions ) -> aid_list
{
    aid_list list;

    const auto add_climbing_aids = [&list]( const condition &, const climbing_aid & aid ) {
        list.push_back( &aid );
    };
    for_each_aid_condition( conditions, add_climbing_aids );

    list.push_back( climbing_aid_default_ptr );

    return list;
}

auto climbing_aid::get_safest( const condition_list &conditions, const bool no_deploy ) -> const climbing_aid &
{
    const climbing_aid *choice = climbing_aid_default_ptr;

    const auto choose_safest = [&choice, no_deploy]( const condition &, const climbing_aid & aid ) {
        if( !no_deploy || aid.down.deploy_furn.is_empty() ) {
            if( aid.slip_chance_mod < choice->slip_chance_mod ) {
                choice = &aid;
            }
        }
    };
    for_each_aid_condition( conditions, choose_safest );

    return *choice;
}

auto climbing_aid::get_default() -> const climbing_aid &
{
    return *climbing_aid_default_ptr;
}

template<typename Test>
static auto detect_conditions_sub( climbing_aid::condition_list &list,
                                   const climbing_aid::category category, const Test &test ) -> void
{
    std::string empty;
    const std::string *flag_already_tested = &empty;
    for( const auto &[ flag, aid ] : climbing_lookup[ static_cast<int>( category ) ] ) {
        if( flag == *flag_already_tested ) {
            continue;
        }

        climbing_aid::condition cond = { category, flag };
        if( test( cond ) ) {
            list.emplace_back( cond );
        }

        flag_already_tested = &flag;
    }
}

auto climbing_aid::detect_conditions( Character &you,
                                        const tripoint_bub_ms &examp ) -> condition_list
{
    condition_list list;

    map &here = get_map();

    const fall_scan fall( examp );

    const auto detect_you_character_flag = [&you]( condition & cond ) {
        return you.has_trait_flag( trait_flag_str_id( cond.flag ) );
    };
    const auto detect_you_trait = [&you]( condition & cond ) {
        return you.has_trait( trait_id( cond.flag ) );
    };
    const auto detect_item = [&you]( condition & cond ) {
        cond.uses_item = you.amount_of( itype_id( cond.flag ) );
        return cond.uses_item > 0;
    };
    const auto detect_ter_furn_flag = [&here, &fall]( condition & cond ) {
        const tripoint_bub_ms pos = fall.pos_furniture_or_floor();
        cond.range = fall.pos_top().z() - pos.z();
        return here.has_flag( cond.flag, pos );
    };
    const auto detect_vehicle = [&fall]( condition & cond ) {
        cond.range = 1;
        return fall.veh_just_below();
    };

    detect_conditions_sub( list, category::character, detect_you_character_flag );
    detect_conditions_sub( list, category::trait, detect_you_trait );
    detect_conditions_sub( list, category::item, detect_item );
    detect_conditions_sub( list, category::ter_furn, detect_ter_furn_flag );
    detect_conditions_sub( list, category::veh, detect_vehicle );

    return list;
}

climbing_aid::fall_scan::fall_scan( const tripoint_bub_ms &examp )
{
    map &here = get_map();
    Creature_tracker &creatures = here.get_mapbuffer().creature_tracker();

    this->examp = examp;
    height = 0;
    height_until_furniture = 0;
    height_until_creature = 0;
    height_until_vehicle = 0;

    tripoint_bub_ms bottom( examp );
    tripoint_bub_ms just_below( bottom + tripoint_rel_ms::below() );

    bool hit_furn = false;
    bool hit_crea = false;
    bool hit_veh = false;

    for( tripoint_bub_ms lower = just_below; here.valid_move( bottom, lower, false, true ); ) {
        if( !hit_furn ) {
            if( here.has_furn( lower ) ) {
                hit_furn = true;
            } else {
                ++height_until_furniture;
            }
        }
        if( !hit_veh ) {
            if( here.veh_at( lower ) ) {
                hit_veh = true;
            } else {
                ++height_until_vehicle;
            }
        }
        if( !hit_crea ) {
            ++height_until_creature;
            if( creatures.find( lower ) ) {
                hit_crea = true;
            } else {
                ++height_until_creature;
            }
        }
        ++height;
        bottom.z()--;
        lower.z()--;
    }
}
