#include "catch/catch.hpp"

#include <sstream>
#include <utility>

#include "detached_ptr.h"
#include "flag.h"
#include "item.h"
#include "item_pocket.h"
#include "item_factory.h"
#include "item_group.h"
#include "npc.h"
#include "pickup_token.h"
#include "pickup.h"
#include "map.h"
#include "game.h"
#include "avatar.h"
#include "avatar_functions.h"
#include "iteminfo_query.h"
#include "itype.h"
#include "json.h"
#include "options_helpers.h"
#include "relic.h"
#include "state_helpers.h"
#include "ret_val.h"
#include "type_id.h"
#include "units.h"

TEST_CASE( "empty_pocket_reports_empty_and_full_remaining_volume", "[item][pocket]" )
{
    detached_ptr<item> holder = item::spawn( "backpack" );
    pocket_data data;
    data.type = pocket_type::CONTAINER;
    data.max_contains_volume = 2500_ml;

    item_pocket pocket( holder.get(), &data );

    CHECK( pocket.empty() );
    CHECK( pocket.contents_volume() == 0_ml );
    CHECK( pocket.remaining_volume() == 2500_ml );
}

TEST_CASE( "pocket_accepts_and_holds_an_item_that_fits", "[item][pocket]" )
{
    detached_ptr<item> holder = item::spawn( "backpack" );
    pocket_data data;
    data.type = pocket_type::CONTAINER;
    data.max_contains_volume = 2500_ml;
    item_pocket pocket( holder.get(), &data );

    detached_ptr<item> sugar = item::spawn( "sugar" );
    const units::volume sugar_volume = sugar->volume();
    REQUIRE( pocket.can_contain( *sugar ).success() );

    pocket.insert( std::move( sugar ) );

    CHECK_FALSE( pocket.empty() );
    CHECK( pocket.all_items_top().size() == 1 );
    CHECK( pocket.contents_volume() == sugar_volume );
}

TEST_CASE( "pocket_rejects_an_item_that_is_too_large", "[item][pocket]" )
{
    detached_ptr<item> holder = item::spawn( "backpack" );
    pocket_data data;
    data.type = pocket_type::CONTAINER;
    data.max_contains_volume = 1_ml;
    item_pocket pocket( holder.get(), &data );

    detached_ptr<item> sugar = item::spawn( "sugar" );
    ret_val<item_pocket::contain_code> res = pocket.can_contain( *sugar );

    CHECK_FALSE( res.success() );
    CHECK( res.value() == item_pocket::contain_code::ERR_TOO_BIG );
}

TEST_CASE( "removing_an_item_empties_the_pocket", "[item][pocket]" )
{
    detached_ptr<item> holder = item::spawn( "backpack" );
    pocket_data data;
    data.type = pocket_type::CONTAINER;
    data.max_contains_volume = 2500_ml;
    item_pocket pocket( holder.get(), &data );

    detached_ptr<item> sugar = item::spawn( "sugar" );
    item *raw = sugar.get();
    pocket.insert( std::move( sugar ) );
    REQUIRE_FALSE( pocket.empty() );

    detached_ptr<item> taken = pocket.remove( raw );

    CHECK( taken );
    CHECK( pocket.empty() );
    CHECK( pocket.contents_volume() == 0_ml );
}

TEST_CASE( "item_contents_round_trips_through_a_single_pocket", "[item][pocket][contents]" )
{
    detached_ptr<item> backpack = item::spawn( "backpack" );
    detached_ptr<item> sugar = item::spawn( "sugar" );
    item *raw = sugar.get();

    REQUIRE( backpack->contents.empty() );

    ret_val<bool> inserted = backpack->contents.insert_item( std::move( sugar ) );
    REQUIRE( inserted.success() );

    CHECK_FALSE( backpack->contents.empty() );
    CHECK( backpack->contents.all_items_top().size() == 1 );
    CHECK( backpack->contents.all_items_top().front() == raw );
    CHECK( backpack->contents.num_item_stacks() == 1 );

    detached_ptr<item> taken = backpack->contents.remove_top( raw );

    CHECK( taken );
    CHECK( backpack->contents.empty() );
}

TEST_CASE( "an_uncurated_item_exposes_exactly_one_synthesized_pocket",
           "[item][pocket][contents]" )
{
    // hoodie rather than backpack: backpack now carries curated pocket_data.
    detached_ptr<item> hoodie = item::spawn( "hoodie" );

    CHECK( hoodie->contents.get_pockets().size() == 1 );
    CHECK( hoodie->contents.get_pockets().front().definition().type == pocket_type::CONTAINER );
}

TEST_CASE( "a_container_gains_one_pocket_sized_from_its_legacy_storage",
           "[item][pocket][synthesis]" )
{
    detached_ptr<item> bottle = item::spawn( "bottle_plastic" );
    const std::vector<item_pocket> &pockets = bottle->contents.get_pockets();

    REQUIRE( pockets.size() == 1 );
    CHECK( pockets.front().definition().type == pocket_type::CONTAINER );
    CHECK( pockets.front().definition().watertight );
    CHECK( pockets.front().remaining_volume() == bottle->type->container->contains );
}

TEST_CASE( "worn_storage_synthesizes_a_non_rigid_pocket", "[item][pocket][synthesis]" )
{
    // hoodie rather than backpack: backpack now carries curated pocket_data.
    detached_ptr<item> hoodie = item::spawn( "hoodie" );
    const std::vector<item_pocket> &pockets = hoodie->contents.get_pockets();

    REQUIRE( pockets.size() == 1 );
    CHECK_FALSE( pockets.front().definition().rigid );
    CHECK( pockets.front().remaining_volume() == hoodie->type->armor->storage );
}

TEST_CASE( "pocket_contents_survive_a_serialization_round_trip", "[item][pocket][save]" )
{
    detached_ptr<item> backpack = item::spawn( "backpack" );
    backpack->contents.insert_item( item::spawn( "sugar" ) );

    std::ostringstream os;
    JsonOut jo( os );
    backpack->contents.serialize( jo );

    detached_ptr<item> restored = item::spawn( "backpack" );
    std::istringstream is( os.str() );
    JsonIn ji( is );
    restored->contents.deserialize( ji );

    CHECK( restored->contents.all_items_top().size() == 1 );
    CHECK( restored->contents.all_items_top().front()->typeId() == itype_id( "sugar" ) );
}

TEST_CASE( "a_backpack_keeps_its_capacity_across_a_round_trip", "[item][pocket][save]" )
{
    // A worn non-rigid container with no capacity trips a debugmsg in
    // get_encumber_when_containing and drops everything it held.
    detached_ptr<item> backpack = item::spawn( "backpack" );
    backpack->contents.insert_item( item::spawn( "sugar" ) );
    const units::volume before = backpack->get_total_capacity();
    REQUIRE( before > 0_ml );

    std::ostringstream os;
    JsonOut jo( os );
    backpack->contents.serialize( jo );

    detached_ptr<item> restored = item::spawn( "backpack" );
    std::istringstream is( os.str() );
    JsonIn ji( is );
    restored->contents.deserialize( ji );

    INFO( "capacity before " << units::to_milliliter( before ) << " ml, after "
          << units::to_milliliter( restored->get_total_capacity() ) << " ml" );
    CHECK( restored->get_total_capacity() == before );
}

TEST_CASE( "a_backpack_keeps_its_capacity_through_item_serialization",
           "[item][pocket][save]" )
{
    detached_ptr<item> backpack = item::spawn( "backpack" );
    backpack->contents.insert_item( item::spawn( "sugar" ) );
    const units::volume before = backpack->get_total_capacity();
    REQUIRE( before > 0_ml );

    std::ostringstream os;
    JsonOut jo( os );
    backpack->serialize( jo );

    std::istringstream is( os.str() );
    JsonIn ji( is );
    detached_ptr<item> restored = item::spawn( "backpack" );
    restored->deserialize( ji );

    INFO( "pockets after: " << restored->contents.get_pockets().size() );
    INFO( "capacity before " << units::to_milliliter( before ) << " ml, after "
          << units::to_milliliter( restored->get_total_capacity() ) << " ml" );
    CHECK( restored->get_total_capacity() == before );
}

TEST_CASE( "an_item_loaded_from_a_save_keeps_its_pockets", "[item][pocket][save]" )
{
    // item::spawn( JsonIn& ) builds a null item and then deserializes into it.
    // item_contents takes its pockets from the type it is constructed with, and
    // convert() swaps the type without rebuilding them, so a loaded container
    // can end up with the single 0 ml fallback pocket instead of its own.
    const units::volume expected = item::spawn( "backpack" )->get_total_capacity();
    REQUIRE( expected > 0_ml );

    std::istringstream is( R"({"typeid":"backpack"})" );
    JsonIn ji( is );
    detached_ptr<item> loaded = item::spawn( ji );
    REQUIRE( loaded );

    INFO( "pockets: " << loaded->contents.get_pockets().size()
          << ", capacity " << units::to_milliliter( loaded->get_total_capacity() ) << " ml" );
    CHECK( loaded->get_total_capacity() == expected );
}

TEST_CASE( "a_collapsed_pocket_stays_collapsed_across_a_save", "[item][pocket][save]" )
{
    detached_ptr<item> backpack = item::spawn( "backpack" );
    REQUIRE( backpack->contents.get_pockets().size() > 1 );
    backpack->contents.get_pockets().front().get_settings().set_collapse( true );

    std::ostringstream os;
    JsonOut jo( os );
    backpack->serialize( jo );

    std::istringstream is( os.str() );
    JsonIn ji( is );
    detached_ptr<item> restored = item::spawn( ji );
    REQUIRE( restored );
    REQUIRE( restored->contents.get_pockets().size() ==
             backpack->contents.get_pockets().size() );

    CHECK( restored->contents.get_pockets().front().get_settings().is_collapsed() );
}

TEST_CASE( "serialized_contents_use_the_pocket_format", "[item][pocket][save]" )
{
    detached_ptr<item> backpack = item::spawn( "backpack" );
    backpack->contents.insert_item( item::spawn( "sugar" ) );

    std::ostringstream os;
    JsonOut jo( os );
    backpack->contents.serialize( jo );

    CHECK( os.str().find( "\"pockets\"" ) != std::string::npos );
    CHECK( os.str().find( "\"pocket_type\"" ) != std::string::npos );
}

TEST_CASE( "a_legacy_flat_contents_array_loads_into_the_first_pocket", "[item][pocket][save]" )
{
    // The pre-pocket item_contents format: a bare "items" array. The plan named
    // this key "contents", but that is the *item*-level ancient array, which
    // item::deserialize already handles before item_contents ever sees it.
    const std::string legacy = R"({ "items": [ { "typeid": "sugar" } ] })";

    detached_ptr<item> backpack = item::spawn( "backpack" );
    std::istringstream is( legacy );
    JsonIn ji( is );
    backpack->contents.deserialize( ji );

    CHECK( backpack->contents.all_items_top().size() == 1 );
    CHECK( backpack->contents.get_pockets().front().all_items_top().size() == 1 );
}

// A whole item as a pre-pocket save wrote it: "contents" is an object holding a
// flat "items" array. Exercises item::deserialize's branch into the legacy path.
TEST_CASE( "an_item_saved_before_pockets_keeps_its_contents", "[item][pocket][save]" )
{
    const std::string legacy =
        R"([ { "typeid": "backpack", "contents": { "items": [ { "typeid": "sugar" } ] } } ])";
    std::istringstream is( legacy );
    JsonIn ji( is );

    std::vector<detached_ptr<item>> loaded;
    REQUIRE( ji.read( loaded ) );

    REQUIRE( loaded.size() == 1 );
    REQUIRE( loaded.front()->typeId() == itype_id( "backpack" ) );
    CHECK( loaded.front()->contents.all_items_top().size() == 1 );
    CHECK( loaded.front()->contents.all_items_top().front()->typeId() == itype_id( "sugar" ) );
}

TEST_CASE( "authored_pockets_appear_in_item_info_and_synthesized_do_not",
           "[item][pocket][info]" )
{
    const auto info_text = []( const item & it ) {
        std::string joined;
        for( const iteminfo &entry : it.info() ) {
            joined += entry.sName;
            joined += "\n";
        }
        return joined;
    };

    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    const std::string bag_info = info_text( *bag );
    CHECK( bag_info.find( "2 pockets" ) != std::string::npos );
    CHECK( bag_info.find( "watertight" ) != std::string::npos );

    // A hoodie's single synthesized pocket is already described by the legacy
    // storage line; a pocket section would only duplicate it. Match the section
    // header, not the bare word: the hoodie's flavour text mentions a pocket.
    detached_ptr<item> hoodie = item::spawn( "hoodie" );
    CHECK( info_text( *hoodie ).find( "This item has" ) == std::string::npos );
}

// NOTE: an end-to-end g->save()/g->load() test was tried here and removed. A
// successful load mid-suite replaces the world and avatar, and the surrounding
// tests do not survive it (146 failures and a Lua panic). The plan's manual
// save-and-reload check stays manual.

// ---------------------------------------------------------------------------
// Special pocket synthesis: MAGAZINE, MAGAZINE_WELL, MOD, CORPSE
// ---------------------------------------------------------------------------

static bool has_pocket( const item &it, const pocket_type type )
{
    for( const item_pocket &pocket : it.contents.get_pockets() ) {
        if( pocket.definition().type == type ) {
            return true;
        }
    }
    return false;
}

TEST_CASE( "a_magazine_gains_a_magazine_pocket", "[item][pocket][synthesis]" )
{
    detached_ptr<item> mag = item::spawn( "glockmag" );
    REQUIRE( mag->type->magazine );
    CHECK( has_pocket( *mag, pocket_type::MAGAZINE ) );
}

TEST_CASE( "a_gun_taking_magazines_gains_a_magazine_well", "[item][pocket][synthesis]" )
{
    detached_ptr<item> gun = item::spawn( "glock_19" );
    REQUIRE_FALSE( gun->type->magazines.empty() );
    CHECK( has_pocket( *gun, pocket_type::MAGAZINE_WELL ) );
}

TEST_CASE( "a_gun_with_mod_locations_gains_a_mod_pocket", "[item][pocket][synthesis]" )
{
    detached_ptr<item> gun = item::spawn( "glock_19" );
    REQUIRE( gun->type->gun );
    REQUIRE_FALSE( gun->type->gun->valid_mod_locations.empty() );
    CHECK( has_pocket( *gun, pocket_type::MOD ) );
}

TEST_CASE( "a_gun_with_an_internal_clip_gains_a_magazine_pocket",
           "[item][pocket][synthesis]" )
{
    // Tube-fed and lever guns load rounds directly rather than taking a
    // detachable magazine, so they need somewhere to hold them.
    detached_ptr<item> shotgun = item::spawn( "mossberg_500" );
    REQUIRE( shotgun->type->gun );
    REQUIRE( shotgun->type->gun->clip > 0 );
    REQUIRE( shotgun->type->magazines.empty() );

    CHECK( has_pocket( *shotgun, pocket_type::MAGAZINE ) );
    CHECK_FALSE( has_pocket( *shotgun, pocket_type::MAGAZINE_WELL ) );
}

TEST_CASE( "a_gun_taking_detachable_magazines_gains_no_magazine_pocket",
           "[item][pocket][synthesis]" )
{
    // The rounds live in the magazine, which lives in the well.
    detached_ptr<item> gun = item::spawn( "glock_19" );
    CHECK( has_pocket( *gun, pocket_type::MAGAZINE_WELL ) );
    CHECK_FALSE( has_pocket( *gun, pocket_type::MAGAZINE ) );
}

TEST_CASE( "an_item_with_no_storage_gains_no_special_pockets", "[item][pocket][synthesis]" )
{
    detached_ptr<item> rock = item::spawn( "test_rock" );
    CHECK_FALSE( has_pocket( *rock, pocket_type::MAGAZINE ) );
    CHECK_FALSE( has_pocket( *rock, pocket_type::MAGAZINE_WELL ) );
    CHECK_FALSE( has_pocket( *rock, pocket_type::MOD ) );
    CHECK_FALSE( has_pocket( *rock, pocket_type::CORPSE ) );
}

TEST_CASE( "a_gun_can_still_hold_its_mods_and_magazine", "[item][pocket][synthesis]" )
{
    // The load-bearing check for special pockets: adding pockets must not stop a
    // gun holding what it held before.
    detached_ptr<item> gun = item::spawn( "glock_19" );
    REQUIRE( gun->contents.get_pockets().size() > 1 );

    ret_val<bool> put_mag = gun->contents.insert_item( item::spawn( "glockmag" ) );
    CHECK( put_mag.success() );
    CHECK( gun->contents.all_items_top().size() == 1 );
}

TEST_CASE( "all_items_top_stays_stable_across_calls_on_a_multi_pocket_item",
           "[item][pocket][contents]" )
{
    // With several pockets the returned reference comes from a cache. Rebuilding
    // it on every call would invalidate a reference the caller still holds.
    detached_ptr<item> gun = item::spawn( "glock_19" );
    REQUIRE( gun->contents.get_pockets().size() > 1 );
    gun->contents.insert_item( item::spawn( "glockmag" ) );

    const std::vector<item *> &first = gun->contents.all_items_top();
    REQUIRE( first.size() == 1 );
    item *before = first.front();

    // A second call must not clear the vector the first reference points at.
    const std::vector<item *> &second = gun->contents.all_items_top();
    REQUIRE( second.size() == 1 );

    CHECK( first.size() == 1 );
    CHECK( first.front() == before );
}

// ---------------------------------------------------------------------------
// Pocket favorites: per-pocket player preferences
// ---------------------------------------------------------------------------

TEST_CASE( "untouched_favorite_settings_are_null", "[item][pocket][favorites]" )
{
    // Null settings serialize to nothing, which keeps saves from growing by an
    // empty object per pocket per item.
    pocket_favorite_settings settings;
    CHECK( settings.is_null() );

    settings.set_priority( 1 );
    CHECK_FALSE( settings.is_null() );

    settings.clear();
    CHECK( settings.is_null() );
}

TEST_CASE( "an_item_whitelist_admits_only_what_it_names", "[item][pocket][favorites]" )
{
    pocket_favorite_settings settings;
    settings.whitelist_item( itype_id( "sugar" ) );

    detached_ptr<item> sugar = item::spawn( "sugar" );
    detached_ptr<item> rock = item::spawn( "test_rock" );
    CHECK( settings.accepts_item( *sugar ) );
    CHECK_FALSE( settings.accepts_item( *rock ) );
}

TEST_CASE( "an_item_blacklist_excludes_only_what_it_names", "[item][pocket][favorites]" )
{
    pocket_favorite_settings settings;
    settings.blacklist_item( itype_id( "sugar" ) );

    detached_ptr<item> sugar = item::spawn( "sugar" );
    detached_ptr<item> rock = item::spawn( "test_rock" );
    CHECK_FALSE( settings.accepts_item( *sugar ) );
    CHECK( settings.accepts_item( *rock ) );
}

TEST_CASE( "whitelisting_an_item_clears_it_from_the_blacklist",
           "[item][pocket][favorites]" )
{
    pocket_favorite_settings settings;
    settings.blacklist_item( itype_id( "sugar" ) );
    settings.whitelist_item( itype_id( "sugar" ) );

    CHECK( settings.get_item_blacklist().empty() );
    detached_ptr<item> sugar = item::spawn( "sugar" );
    CHECK( settings.accepts_item( *sugar ) );
}

TEST_CASE( "an_item_whitelist_beside_a_category_blacklist_is_an_exception_to_it",
           "[item][pocket][favorites]" )
{
    // CDDA's subtlest rule: a lone item whitelist means "only these", but paired
    // with a category blacklist it means "these despite the blacklist", and
    // everything untouched by either still gets in.
    detached_ptr<item> sugar = item::spawn( "sugar" );
    detached_ptr<item> rock = item::spawn( "test_rock" );

    pocket_favorite_settings alone;
    alone.whitelist_item( sugar->typeId() );
    CHECK_FALSE( alone.accepts_item( *rock ) );

    pocket_favorite_settings paired;
    paired.whitelist_item( sugar->typeId() );
    paired.blacklist_category( item_category_id( "clothing" ) );
    CHECK( paired.accepts_item( *rock ) );
    CHECK( paired.accepts_item( *sugar ) );
}

TEST_CASE( "a_disabled_pocket_accepts_nothing", "[item][pocket][favorites]" )
{
    pocket_favorite_settings settings;
    settings.set_disabled( true );

    detached_ptr<item> sugar = item::spawn( "sugar" );
    CHECK_FALSE( settings.accepts_item( *sugar ) );
}

TEST_CASE( "a_container_is_judged_by_its_contents", "[item][pocket][favorites]" )
{
    // Putting a bag of blacklisted things into a pocket should be refused just
    // as the loose things would be.
    pocket_favorite_settings settings;
    settings.blacklist_item( itype_id( "sugar" ) );

    detached_ptr<item> bag = item::spawn( "bag_plastic" );
    bag->contents.insert_item( item::spawn( "sugar" ) );
    REQUIRE_FALSE( bag->contents.empty() );

    CHECK_FALSE( settings.accepts_item( *bag ) );
}

// ---------------------------------------------------------------------------
// Item length
// ---------------------------------------------------------------------------

TEST_CASE( "an_undeclared_length_is_derived_from_volume", "[item][pocket][length]" )
{
    // CDDA's rule: the edge of a cube with the item's volume.
    CHECK( units::default_length_from_volume<int>( 1000_ml ) == 10_cm );
    CHECK( units::default_length_from_volume<int>( 8000_ml ) == 20_cm );

    // And it reaches real items: a 250 ml rock is about 6 cm on a side.
    detached_ptr<item> rock = item::spawn( "test_rock" );
    CHECK( rock->type->longest_side > 0_mm );
}

TEST_CASE( "a_stackable_item_is_measured_per_unit", "[item][pocket][length]" )
{
    // A box of ammo must not be treated as one object the size of the whole
    // stack; CDDA divides by stack size before deriving length.
    detached_ptr<item> round = item::spawn( "9mm" );
    REQUIRE( round->type->stack_size > 1 );
    CHECK( round->type->longest_side <
           units::default_length_from_volume<int>( round->type->volume ) );
}

TEST_CASE( "a_pocket_refuses_an_item_that_is_too_long", "[item][pocket][length]" )
{
    detached_ptr<item> holder = item::spawn( "hoodie" );
    pocket_data data;
    data.type = pocket_type::CONTAINER;
    data.max_contains_volume = 100_liter;
    data.max_item_length = 5_cm;
    item_pocket shallow( holder.get(), &data );

    // Roomy by volume, but far too short for a crowbar.
    detached_ptr<item> crowbar = item::spawn( "crowbar" );
    REQUIRE( crowbar->length() > 5_cm );
    const ret_val<item_pocket::contain_code> res = shallow.can_contain( *crowbar );
    CHECK_FALSE( res.success() );
    CHECK( res.value() == item_pocket::contain_code::ERR_TOO_BIG );
}

TEST_CASE( "soft_items_ignore_length_limits", "[item][pocket][length]" )
{
    // Cloth squashes, so CDDA gives soft items no length at all.
    detached_ptr<item> rag = item::spawn( "rag" );
    if( rag->is_soft() ) {
        CHECK( rag->length() == 0_mm );
    }
}

TEST_CASE( "classic_mode_ignores_length_limits", "[item][pocket][length][classic]" )
{
    detached_ptr<item> holder = item::spawn( "hoodie" );
    pocket_data data;
    data.type = pocket_type::CONTAINER;
    data.max_contains_volume = 100_liter;
    data.max_item_length = 5_cm;
    item_pocket shallow( holder.get(), &data );
    detached_ptr<item> crowbar = item::spawn( "crowbar" );
    REQUIRE_FALSE( shallow.can_contain( *crowbar ).success() );

    override_option classic( "POCKET_SYSTEM", "classic" );
    CHECK( shallow.can_contain( *crowbar ).success() );
}

// ---------------------------------------------------------------------------
// Flag restrictions and holsters
// ---------------------------------------------------------------------------

TEST_CASE( "a_flag_restricted_pocket_accepts_only_flagged_items",
           "[item][pocket][restrict][flags]" )
{
    // The baldric's first pocket takes SHEATH_SWORD, its belt loops BELT_CLIP.
    detached_ptr<item> baldric = item::spawn( "baldric" );
    const std::vector<item_pocket> &pockets = baldric->contents.get_pockets();
    REQUIRE( pockets.size() >= 2 );
    REQUIRE_FALSE( pockets[1].definition().flag_restriction.empty() );

    // A baton carries BELT_CLIP and fits the loop's 2 L / 1750 g limits; sugar
    // carries no relevant flag. (A 2h_flail_steel also has BELT_CLIP but weighs
    // 1800 g, so CDDA's weight cap refuses it - correctly.)
    detached_ptr<item> baton = item::spawn( "baton" );
    CHECK( pockets[1].can_contain( *baton ).success() );

    detached_ptr<item> sugar = item::spawn( "sugar" );
    sugar->charges = 1;
    CHECK_FALSE( pockets[1].can_contain( *sugar ).success() );
}

TEST_CASE( "a_holster_pocket_holds_exactly_one_item", "[item][pocket][restrict][holster]" )
{
    // The backpack's side pouches are holsters: one bottle each.
    detached_ptr<item> backpack = item::spawn( "backpack" );
    const std::vector<item_pocket> &pockets = backpack->contents.get_pockets();
    REQUIRE( pockets.size() >= 2 );
    REQUIRE( pockets[1].definition().holster );

    item_pocket &pouch = backpack->contents.get_pockets()[1];
    detached_ptr<item> first = item::spawn( "bottle_plastic" );
    REQUIRE( pouch.can_contain( *first ).success() );
    pouch.insert( std::move( first ) );

    detached_ptr<item> second = item::spawn( "bottle_plastic" );
    const ret_val<item_pocket::contain_code> res = pouch.can_contain( *second );
    CHECK_FALSE( res.success() );
    CHECK( res.value() == item_pocket::contain_code::ERR_NO_SPACE );
}

TEST_CASE( "no_pocket_is_left_with_zero_capacity", "[item][pocket][audit]" )
{
    // The first import produced authored 0 ml pockets whose CDDA capacity lived
    // in fields that do not translate; the re-import drops such pockets.
    std::vector<std::string> offenders;
    for( const itype *def : item_controller->all() ) {
        // TEST_DATA carries one deliberately degenerate pocket, to prove such a
        // pocket is described nowhere. The audit is about shipped content.
        if( def->get_id().str().starts_with( "test_" ) ) {
            continue;
        }
        for( const pocket_data &pocket : def->pockets ) {
            if( !pocket.synthesized && pocket.type == pocket_type::CONTAINER &&
                pocket.max_contains_volume == 0_ml &&
                pocket.ammo_restriction.empty() ) {
                offenders.push_back( def->get_id().str() );
            }
        }
    }
    CAPTURE( offenders );
    CHECK( offenders.empty() );
}

// ---------------------------------------------------------------------------
// Enforcement: insertion can refuse, and nothing is destroyed by refusal
// ---------------------------------------------------------------------------

TEST_CASE( "insert_item_refuses_what_no_pocket_accepts", "[item][pocket][enforce]" )
{
    // A magazine's only pocket is ammo-restricted, so sugar has nowhere to go.
    detached_ptr<item> mag = item::spawn( "glockmag" );
    detached_ptr<item> sugar = item::spawn( "sugar" );
    sugar->charges = 1;

    const ret_val<bool> res = mag->contents.insert_item( std::move( sugar ) );

    CHECK_FALSE( res.success() );
    // The refused item was not consumed and not destroyed.
    // NOLINTNEXTLINE(bugprone-use-after-move)
    REQUIRE( sugar );
    CHECK( sugar->typeId() == itype_id( "sugar" ) );
    CHECK( mag->contents.empty() );
}

TEST_CASE( "put_in_hands_back_a_refused_item", "[item][pocket][enforce]" )
{
    detached_ptr<item> mag = item::spawn( "glockmag" );
    detached_ptr<item> sugar = item::spawn( "sugar" );
    sugar->charges = 1;

    detached_ptr<item> refused = mag->put_in( std::move( sugar ) );

    REQUIRE( refused );
    CHECK( refused->typeId() == itype_id( "sugar" ) );
    CHECK( mag->contents.empty() );
}

// The machinery that must never drop an item - save migration, copy
// construction - forces its way in rather than accepting a refusal.
TEST_CASE( "a_forced_insertion_never_loses_an_item", "[item][pocket][enforce]" )
{
    detached_ptr<item> mag = item::spawn( "glockmag" );
    detached_ptr<item> sugar = item::spawn( "sugar" );
    sugar->charges = 1;
    REQUIRE_FALSE( mag->contents.get_pockets().front().can_contain( *sugar ).success() );

    mag->contents.insert_item_forced( std::move( sugar ) );

    // Into pocket 0 rather than rejected or destroyed.
    CHECK( mag->contents.all_items_top().size() == 1 );
}

TEST_CASE( "classic_mode_accepts_what_full_mode_refuses", "[item][pocket][enforce][classic]" )
{
    // Classic relaxes the type rules, so the same insertion succeeds: this is
    // the first point where the two modes actually behave differently.
    override_option classic( "POCKET_SYSTEM", "classic" );

    detached_ptr<item> mag = item::spawn( "glockmag" );
    detached_ptr<item> sugar = item::spawn( "sugar" );
    sugar->charges = 1;

    CHECK( mag->contents.insert_item( std::move( sugar ) ).success() );
}

// ---------------------------------------------------------------------------
// Classic mode: pooling storage back into one compartment
// ---------------------------------------------------------------------------

TEST_CASE( "classic_mode_keeps_the_same_pockets", "[item][pocket][classic]" )
{
    // Synthesis is identical in both modes: what changes is behaviour, not the
    // pocket set. Keeping the pockets is what makes save data byte-identical
    // between modes.
    const size_t full_count = item::spawn( "pants_cargo" )->contents.get_pockets().size();
    REQUIRE( full_count > 1 );

    override_option classic( "POCKET_SYSTEM", "classic" );
    CHECK( item::spawn( "pants_cargo" )->contents.get_pockets().size() == full_count );
}

TEST_CASE( "classic_mode_ignores_type_restrictions", "[item][pocket][classic]" )
{
    detached_ptr<item> mag = item::spawn( "glockmag" );
    detached_ptr<item> sugar = item::spawn( "sugar" );
    sugar->charges = 1;
    const item_pocket &pocket = mag->contents.get_pockets().front();

    // Full mode refuses it: a magazine takes ammo and nothing else.
    REQUIRE_FALSE( pocket.can_contain( *sugar ).success() );

    override_option classic( "POCKET_SYSTEM", "classic" );
    CHECK( pocket.can_contain( *sugar ).success() );
}

TEST_CASE( "classic_mode_still_respects_volume", "[item][pocket][classic]" )
{
    // Relaxed does not mean unlimited; volume and weight still apply.
    detached_ptr<item> holder = item::spawn( "hoodie" );
    pocket_data data;
    data.type = pocket_type::CONTAINER;
    data.max_contains_volume = 1_ml;
    item_pocket tiny( holder.get(), &data );

    override_option classic( "POCKET_SYSTEM", "classic" );
    detached_ptr<item> sugar = item::spawn( "sugar" );
    const ret_val<item_pocket::contain_code> res = tiny.can_contain( *sugar );
    CHECK_FALSE( res.success() );
    CHECK( res.value() == item_pocket::contain_code::ERR_TOO_BIG );
}

TEST_CASE( "classic_mode_hides_the_pocket_info_section", "[item][pocket][classic]" )
{
    const auto info_text = []( const item & it ) {
        std::string joined;
        for( const iteminfo &entry : it.info() ) {
            joined += entry.sName;
            joined += "\n";
        }
        return joined;
    };

    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    REQUIRE( info_text( *bag ).find( "This item has" ) != std::string::npos );

    override_option classic( "POCKET_SYSTEM", "classic" );
    CHECK( info_text( *bag ).find( "This item has" ) == std::string::npos );
}

TEST_CASE( "classic_mode_best_pocket_is_first_fit", "[item][pocket][classic]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    REQUIRE( bag->contents.get_pockets().size() == 2 );
    detached_ptr<item> sugar = item::spawn( "sugar" );
    sugar->charges = 1;

    // Full mode prefers the tighter 100 ml pocket over the 4 L one.
    REQUIRE( bag->contents.best_pocket( *sugar )->definition().max_contains_volume == 100_ml );

    override_option classic( "POCKET_SYSTEM", "classic" );
    // First-fit takes whichever comes first, ranking ignored.
    CHECK( bag->contents.best_pocket( *sugar ) == &bag->contents.get_pockets().front() );
}

TEST_CASE( "a_full_mode_save_loads_without_losing_items_in_classic_mode",
           "[item][pocket][classic][save]" )
{
    // Write contents spread across a curated item's pockets. Placed pocket by
    // pocket on purpose: best_pocket() would otherwise pile small items into the
    // same compartment, and stackable ones would merge into a single entry.
    detached_ptr<item> pants = item::spawn( "pants_cargo" );
    REQUIRE( pants->contents.get_pockets().size() >= 3 );
    for( int i = 0; i < 3; i++ ) {
        detached_ptr<item> sugar = item::spawn( "sugar" );
        sugar->charges = 1;
        pants->contents.get_pockets()[i].insert( std::move( sugar ) );
    }
    REQUIRE( pants->contents.all_items_top().size() == 3 );

    std::ostringstream os;
    JsonOut jo( os );
    pants->contents.serialize( jo );

    // ...then read them back into an item with only one pocket, as classic mode
    // would build it. Nothing may be dropped on the floor.
    detached_ptr<item> holder = item::spawn( "hoodie" );
    REQUIRE( holder->contents.get_pockets().size() == 1 );
    std::istringstream is( os.str() );
    JsonIn ji( is );
    holder->contents.deserialize( ji );

    CHECK( holder->contents.all_items_top().size() == 3 );
}

// ---------------------------------------------------------------------------
// Phase 3: restrictions
// ---------------------------------------------------------------------------

static bool any_pocket_accepts( const item &container, const item &content )
{
    for( const item_pocket &pocket : container.contents.get_pockets() ) {
        if( pocket.can_contain( content ).success() ) {
            return true;
        }
    }
    return false;
}

TEST_CASE( "a_magazine_pocket_only_accepts_its_own_ammo", "[item][pocket][restrict]" )
{
    detached_ptr<item> mag = item::spawn( "glockmag" );
    const std::vector<item_pocket> &pockets = mag->contents.get_pockets();
    REQUIRE( pockets.size() == 1 );
    REQUIRE_FALSE( pockets.front().definition().ammo_restriction.empty() );

    detached_ptr<item> nine_mm = item::spawn( "9mm" );
    // A spawned stack is far bigger than a magazine; ask about a single round.
    nine_mm->charges = 1;
    CHECK( pockets.front().can_contain( *nine_mm ).success() );

    detached_ptr<item> forty_five = item::spawn( "45_acp" );
    forty_five->charges = 1;
    const ret_val<item_pocket::contain_code> wrong =
        pockets.front().can_contain( *forty_five );
    CHECK_FALSE( wrong.success() );
    CHECK( wrong.value() == item_pocket::contain_code::ERR_AMMO );
}

TEST_CASE( "a_magazine_pocket_rejects_things_that_are_not_ammo",
           "[item][pocket][restrict]" )
{
    detached_ptr<item> mag = item::spawn( "glockmag" );
    detached_ptr<item> sugar = item::spawn( "sugar" );

    const ret_val<item_pocket::contain_code> res =
        mag->contents.get_pockets().front().can_contain( *sugar );
    CHECK_FALSE( res.success() );
    CHECK( res.value() == item_pocket::contain_code::ERR_AMMO );
}

TEST_CASE( "a_magazine_pocket_respects_its_round_capacity", "[item][pocket][restrict]" )
{
    detached_ptr<item> mag = item::spawn( "glockmag" );
    const item_pocket &pocket = mag->contents.get_pockets().front();
    const int capacity = pocket.definition().ammo_restriction.begin()->second;
    REQUIRE( capacity > 0 );

    detached_ptr<item> rounds = item::spawn( "9mm" );
    rounds->charges = capacity;
    CHECK( pocket.can_contain( *rounds ).success() );

    detached_ptr<item> too_many = item::spawn( "9mm" );
    too_many->charges = capacity + 1;
    const ret_val<item_pocket::contain_code> res = pocket.can_contain( *too_many );
    CHECK_FALSE( res.success() );
    CHECK( res.value() == item_pocket::contain_code::ERR_AMMO );
}

TEST_CASE( "an_internal_clip_gun_restricts_its_magazine_pocket_to_its_ammo",
           "[item][pocket][restrict]" )
{
    detached_ptr<item> shotgun = item::spawn( "mossberg_500" );
    const item_pocket *magazine = nullptr;
    for( const item_pocket &pocket : shotgun->contents.get_pockets() ) {
        if( pocket.definition().type == pocket_type::MAGAZINE ) {
            magazine = &pocket;
        }
    }
    REQUIRE( magazine != nullptr );
    CHECK_FALSE( magazine->definition().ammo_restriction.empty() );

    detached_ptr<item> sugar = item::spawn( "sugar" );
    CHECK_FALSE( magazine->can_contain( *sugar ).success() );
}

TEST_CASE( "a_magazine_well_only_accepts_magazines_the_gun_takes",
           "[item][pocket][restrict]" )
{
    detached_ptr<item> gun = item::spawn( "glock_19" );
    const item_pocket *well = nullptr;
    for( const item_pocket &pocket : gun->contents.get_pockets() ) {
        if( pocket.definition().type == pocket_type::MAGAZINE_WELL ) {
            well = &pocket;
        }
    }
    REQUIRE( well != nullptr );
    REQUIRE_FALSE( well->definition().item_restriction.empty() );

    detached_ptr<item> glockmag = item::spawn( "glockmag" );
    CHECK( well->can_contain( *glockmag ).success() );

    detached_ptr<item> akmag = item::spawn( "akmag30" );
    const ret_val<item_pocket::contain_code> wrong = well->can_contain( *akmag );
    CHECK_FALSE( wrong.success() );
    CHECK( wrong.value() == item_pocket::contain_code::ERR_ITEM );

    // And it is not a general-purpose pocket either.
    detached_ptr<item> sugar = item::spawn( "sugar" );
    CHECK_FALSE( well->can_contain( *sugar ).success() );
}

TEST_CASE( "a_mod_pocket_only_accepts_mods_the_gun_can_take", "[item][pocket][restrict]" )
{
    detached_ptr<item> gun = item::spawn( "glock_19" );
    const item_pocket *mod_pocket = nullptr;
    for( const item_pocket &pocket : gun->contents.get_pockets() ) {
        if( pocket.definition().type == pocket_type::MOD ) {
            mod_pocket = &pocket;
        }
    }
    REQUIRE( mod_pocket != nullptr );
    REQUIRE_FALSE( mod_pocket->definition().mod_restriction.empty() );

    detached_ptr<item> sugar = item::spawn( "sugar" );
    const ret_val<item_pocket::contain_code> res = mod_pocket->can_contain( *sugar );
    CHECK_FALSE( res.success() );
    CHECK( res.value() == item_pocket::contain_code::ERR_MOD );
}

TEST_CASE( "every_gun_has_a_pocket_for_its_default_mods", "[item][pocket][audit]" )
{
    // Default mods are installed on spawn, so if a gun's MOD pocket would refuse
    // them, enforcement breaks that gun outright.
    std::vector<std::string> failures;
    int checked = 0;
    for( const itype *def : item_controller->all() ) {
        if( !def->gun || def->gun->default_mods.empty() ) {
            continue;
        }
        for( const itype_id &mod_id : def->gun->default_mods ) {
            detached_ptr<item> gun = item::spawn( def->get_id() );
            gun->contents.clear_items();
            detached_ptr<item> mod = item::spawn( mod_id );
            checked++;
            if( !any_pocket_accepts( *gun, *mod ) ) {
                failures.push_back( def->get_id().str() + " <- " + mod_id.str() );
            }
        }
    }
    // Guard against a vacuous pass; the data declares default_mods ~23 times.
    REQUIRE( checked > 10 );
    CAPTURE( checked );
    CAPTURE( failures );
    CHECK( failures.empty() );
}

TEST_CASE( "a_magazine_goes_to_the_well_not_the_cargo_pocket", "[item][pocket][restrict]" )
{
    // sparkledogsuit has both a MAGAZINE_WELL and a roomy CONTAINER pocket, so it
    // is the case where first-fit and best_pocket could disagree.
    detached_ptr<item> suit = item::spawn( "sparkledogsuit" );
    REQUIRE( suit->contents.get_pockets().size() == 2 );

    detached_ptr<item> cell = item::spawn( "light_battery_cell" );
    item_pocket *chosen = suit->contents.best_pocket( *cell );
    REQUIRE( chosen != nullptr );
    CHECK( chosen->definition().type == pocket_type::MAGAZINE_WELL );

    // And something that is not a battery still finds the storage pocket.
    detached_ptr<item> sugar = item::spawn( "sugar" );
    item_pocket *for_sugar = suit->contents.best_pocket( *sugar );
    REQUIRE( for_sugar != nullptr );
    CHECK( for_sugar->definition().type == pocket_type::CONTAINER );
}

TEST_CASE( "best_pocket_prefers_the_tighter_of_two_storage_pockets",
           "[item][pocket][restrict]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    REQUIRE( bag->contents.get_pockets().size() == 2 );

    // A single unit of sugar fits both the 100 ml and the 4 L pocket; the small
    // one should win so the large one stays free. A whole spawned stack is 250 ml
    // and would only fit the large pocket, which would prove nothing.
    detached_ptr<item> sugar = item::spawn( "sugar" );
    sugar->charges = 1;
    REQUIRE( sugar->volume() <= 100_ml );

    item_pocket *chosen = bag->contents.best_pocket( *sugar );
    REQUIRE( chosen != nullptr );
    CHECK( chosen->definition().max_contains_volume == 100_ml );
}

// ---------------------------------------------------------------------------
// Exhaustive enforcement audit: does every item have a pocket for what it holds?
// These are the gate for turning can_contain() enforcement back on.
// ---------------------------------------------------------------------------

TEST_CASE( "every_gun_has_a_pocket_for_its_default_magazine", "[item][pocket][audit]" )
{
    std::vector<std::string> failures;
    int checked = 0;
    for( const itype *def : item_controller->all() ) {
        if( def->magazines.empty() ) {
            continue;
        }
        for( const auto &entry : def->magazine_default ) {
            if( entry.second.is_null() ) {
                continue;
            }
            detached_ptr<item> gun = item::spawn( def->get_id() );
            detached_ptr<item> mag = item::spawn( entry.second );
            checked++;
            if( !any_pocket_accepts( *gun, *mag ) ) {
                failures.push_back( def->get_id().str() + " <- " + entry.second.str() );
            }
        }
    }
    // Guard against a vacuous pass: an empty sweep would report no failures too.
    REQUIRE( checked > 50 );
    CAPTURE( checked );
    CAPTURE( failures );
    CHECK( failures.empty() );
}

TEST_CASE( "every_magazine_has_a_pocket_for_its_default_ammo", "[item][pocket][audit]" )
{
    std::vector<std::string> failures;
    int checked = 0;
    for( const itype *def : item_controller->all() ) {
        if( !def->magazine || def->magazine->default_ammo.is_null() ) {
            continue;
        }
        detached_ptr<item> mag = item::spawn( def->get_id() );
        // Magazines with a "count" spawn already loaded (disposable cells, ammo
        // belts, weld tanks), and a full magazine rightly refuses more.
        mag->contents.clear_items();
        detached_ptr<item> ammo = item::spawn( def->magazine->default_ammo );
        // One round: the question is whether this ammo *type* belongs here.
        // Spawning gives a full stack, which a magazine may rightly refuse, and
        // real reloading splits to a fitting quantity first (item::reload).
        ammo->charges = 1;
        checked++;
        if( !any_pocket_accepts( *mag, *ammo ) ) {
            failures.push_back( def->get_id().str() + " <- " + def->magazine->default_ammo.str() );
        }
    }
    // Guard against a vacuous pass: an empty sweep would report no failures too.
    REQUIRE( checked > 50 );
    CAPTURE( checked );
    CAPTURE( failures );
    CHECK( failures.empty() );
}

TEST_CASE( "the_insertion_audit_stays_empty_for_ordinary_insertions",
           "[item][pocket][audit]" )
{
    clear_pocket_audit();

    detached_ptr<item> backpack = item::spawn( "backpack" );
    backpack->contents.insert_item( item::spawn( "sugar" ) );

    detached_ptr<item> gun = item::spawn( "glock_19" );
    gun->contents.insert_item( item::spawn( "glockmag" ) );

    detached_ptr<item> shotgun = item::spawn( "mossberg_500" );
    detached_ptr<item> shells = item::spawn( "shot_00" );
    // One shell: a spawned box holds more than the tube does, and real reloading
    // splits to a fitting quantity before inserting.
    shells->charges = 1;
    shotgun->contents.insert_item( std::move( shells ) );

    const std::string report = pocket_audit_report();
    INFO( report );
    CHECK( report.find( "No misses recorded" ) != std::string::npos );

    clear_pocket_audit();
}

TEST_CASE( "the_pocket_coverage_report_describes_loaded_items", "[item][pocket][coverage]" )
{
    const std::string report = pocket_coverage_report();

    REQUIRE_FALSE( report.empty() );
    CHECK( report.find( "Pocket coverage" ) != std::string::npos );
    // A synthesized container and an authored multi-pocket item both appear.
    CHECK( report.find( "backpack" ) != std::string::npos );
    CHECK( report.find( "test_two_pocket_bag" ) != std::string::npos );
    // Both provenances are represented.
    CHECK( report.find( "synthesized" ) != std::string::npos );
    CHECK( report.find( "authored" ) != std::string::npos );
}

TEST_CASE( "synthesized_and_authored_pockets_are_labelled_correctly",
           "[item][pocket][coverage]" )
{
    detached_ptr<item> hoodie = item::spawn( "hoodie" );
    REQUIRE( hoodie->contents.get_pockets().size() == 1 );
    CHECK( hoodie->contents.get_pockets().front().definition().synthesized );

    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    REQUIRE( bag->contents.get_pockets().size() == 2 );
    CHECK_FALSE( bag->contents.get_pockets().front().definition().synthesized );
}

// ---------------------------------------------------------------------------
// Phase 2: pocket_data loaded from JSON
// ---------------------------------------------------------------------------

TEST_CASE( "pocket_data_reads_every_field_from_json", "[item][pocket][json]" )
{
    const std::string text =
        R"({ "pocket_type": "MAGAZINE_WELL", "max_contains_volume": "2 L",
             "max_contains_weight": "3 kg", "max_item_length": "30 cm",
             "rigid": true, "watertight": true, "sealed": true,
             "spoil_multiplier": 0.5, "moves": 250 })";
    std::istringstream is( text );
    JsonIn jsin( is );

    pocket_data data;
    data.deserialize( jsin );

    CHECK( data.type == pocket_type::MAGAZINE_WELL );
    CHECK( data.max_contains_volume == 2_liter );
    CHECK( data.max_contains_weight == units::from_kilogram( 3 ) );
    CHECK( data.max_item_length == 30_cm );
    CHECK( data.rigid );
    CHECK( data.watertight );
    CHECK( data.sealed );
    CHECK( data.spoil_multiplier == Approx( 0.5f ) );
    CHECK( data.moves == 250 );
}

TEST_CASE( "omitted_pocket_data_fields_keep_their_defaults", "[item][pocket][json]" )
{
    const std::string text = R"({ "max_contains_volume": "1 L" })";
    std::istringstream is( text );
    JsonIn jsin( is );

    pocket_data data;
    data.deserialize( jsin );

    CHECK( data.type == pocket_type::CONTAINER );
    CHECK( data.max_contains_volume == 1_liter );
    CHECK( data.max_contains_weight == 0_gram );
    CHECK_FALSE( data.rigid );
    CHECK_FALSE( data.watertight );
    CHECK( data.moves == 100 );
}

TEST_CASE( "an_item_declaring_pocket_data_gets_those_pockets", "[item][pocket][json]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    const std::vector<item_pocket> &pockets = bag->contents.get_pockets();

    REQUIRE( pockets.size() == 2 );
    CHECK( pockets[0].definition().max_contains_volume == 100_ml );
    CHECK( pockets[0].definition().max_item_length == 30_cm );
    CHECK( pockets[1].definition().max_contains_volume == 4_liter );
    CHECK( pockets[1].definition().watertight );
    CHECK( pockets[1].definition().moves == 200 );
}

TEST_CASE( "authored_pocket_data_suppresses_legacy_synthesis", "[item][pocket][json]" )
{
    // An uncurated item still gets exactly one synthesized pocket.
    detached_ptr<item> hoodie = item::spawn( "hoodie" );
    CHECK( hoodie->contents.get_pockets().size() == 1 );

    // The authored item keeps its two, rather than gaining a synthesized third.
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    CHECK( bag->contents.get_pockets().size() == 2 );
}

TEST_CASE( "an_item_too_big_for_the_small_pocket_lands_in_the_large_one",
           "[item][pocket][json]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );

    // 250 ml, so it cannot fit the 100 ml pocket but fits the 4 L one.
    detached_ptr<item> rock = item::spawn( "test_rock" );
    const units::volume rock_volume = rock->volume();
    REQUIRE( rock_volume > 100_ml );

    bag->contents.insert_item( std::move( rock ) );

    CHECK( bag->contents.get_pockets()[0].empty() );
    CHECK( bag->contents.get_pockets()[1].all_items_top().size() == 1 );
}

TEST_CASE( "multi_pocket_contents_survive_a_serialization_round_trip",
           "[item][pocket][json][save]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    bag->contents.insert_item( item::spawn( "test_rock" ) );
    REQUIRE( bag->contents.all_items_top().size() == 1 );

    std::ostringstream os;
    JsonOut jo( os );
    bag->contents.serialize( jo );

    detached_ptr<item> restored = item::spawn( "test_two_pocket_bag" );
    std::istringstream is( os.str() );
    JsonIn ji( is );
    restored->contents.deserialize( ji );

    REQUIRE( restored->contents.get_pockets().size() == 2 );
    CHECK( restored->contents.all_items_top().size() == 1 );
    // Still in the second pocket, not collapsed into the first.
    CHECK( restored->contents.get_pockets()[0].empty() );
    CHECK( restored->contents.get_pockets()[1].all_items_top().size() == 1 );
}

// Task 2: settings reach the save file and come back.

TEST_CASE( "pocket_settings_survive_a_serialization_round_trip",
           "[item][pocket][save][favorites]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    pocket_favorite_settings &settings = bag->contents.get_pockets()[1].get_settings();
    settings.set_priority( 7 );
    settings.whitelist_item( itype_id( "test_rock" ) );
    settings.blacklist_category( item_category_id( "food" ) );
    settings.set_disabled( true );

    std::ostringstream os;
    JsonOut jo( os );
    bag->contents.serialize( jo );

    detached_ptr<item> restored = item::spawn( "test_two_pocket_bag" );
    std::istringstream is( os.str() );
    JsonIn ji( is );
    restored->contents.deserialize( ji );

    REQUIRE( restored->contents.get_pockets().size() == 2 );
    const pocket_favorite_settings &back = restored->contents.get_pockets()[1].get_settings();
    CHECK( back.priority() == 7 );
    CHECK( back.get_item_whitelist().count( itype_id( "test_rock" ) ) == 1 );
    CHECK( back.get_category_blacklist().count( item_category_id( "food" ) ) == 1 );
    CHECK( back.is_disabled() );
    // The untouched pocket stays untouched.
    CHECK( restored->contents.get_pockets()[0].get_settings().is_null() );
}

// An empty pocket that has been organised must still reach the save, even though
// empty contents are normally left out of it entirely.
TEST_CASE( "settings_on_an_empty_pocket_still_save", "[item][pocket][save][favorites]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    REQUIRE( bag->contents.empty() );
    bag->contents.get_pockets()[0].get_settings().set_priority( 3 );

    std::ostringstream os;
    JsonOut jo( os );
    bag->contents.serialize( jo );
    REQUIRE_FALSE( os.str().empty() );

    detached_ptr<item> restored = item::spawn( "test_two_pocket_bag" );
    std::istringstream is( os.str() );
    JsonIn ji( is );
    restored->contents.deserialize( ji );

    CHECK( restored->contents.get_pockets()[0].get_settings().priority() == 3 );
}

TEST_CASE( "an_unorganised_item_writes_no_settings", "[item][pocket][save][favorites]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    bag->contents.insert_item( item::spawn( "test_rock" ) );

    std::ostringstream os;
    JsonOut jo( os );
    bag->contents.serialize( jo );

    CHECK( os.str().find( "settings" ) == std::string::npos );
}

// A save written before this phase has pocket objects with no settings member.
TEST_CASE( "a_save_without_settings_loads_untouched", "[item][pocket][save][favorites]" )
{
    const std::string old_format =
        R"({ "pockets": [ { "pocket_type": 0, "contents": [] },
                          { "pocket_type": 0, "contents": [ { "typeid": "test_rock" } ] } ] })";

    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    std::istringstream is( old_format );
    JsonIn ji( is );
    bag->contents.deserialize( ji );

    CHECK( bag->contents.all_items_top().size() == 1 );
    CHECK( bag->contents.get_pockets()[0].get_settings().is_null() );
    CHECK( bag->contents.get_pockets()[1].get_settings().is_null() );
}

// ---------------------------------------------------------------------------
// best_pocket() and player organisation
// ---------------------------------------------------------------------------

// Both pockets fit the ear plugs, so without settings the tighter one wins and
// every test below is a departure from that baseline.
TEST_CASE( "without_settings_best_pocket_still_prefers_the_tightest_fit",
           "[item][pocket][favorites]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    REQUIRE( bag->contents.insert_item( item::spawn( "test_ear_plugs" ) ).success() );

    CHECK( bag->contents.get_pockets()[0].all_items_top().size() == 1 );
}

TEST_CASE( "priority_beats_the_tightest_fit", "[item][pocket][favorites]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    bag->contents.get_pockets()[1].get_settings().set_priority( 1 );

    REQUIRE( bag->contents.insert_item( item::spawn( "test_ear_plugs" ) ).success() );

    CHECK( bag->contents.get_pockets()[0].empty() );
    CHECK( bag->contents.get_pockets()[1].all_items_top().size() == 1 );
}

TEST_CASE( "a_blacklisted_pocket_is_skipped", "[item][pocket][favorites]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    bag->contents.get_pockets()[0].get_settings().blacklist_item( itype_id( "test_ear_plugs" ) );

    REQUIRE( bag->contents.insert_item( item::spawn( "test_ear_plugs" ) ).success() );

    CHECK( bag->contents.get_pockets()[0].empty() );
    CHECK( bag->contents.get_pockets()[1].all_items_top().size() == 1 );
}

TEST_CASE( "a_whitelisted_pocket_wins_over_a_tighter_one", "[item][pocket][favorites]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    bag->contents.get_pockets()[1].get_settings().whitelist_item( itype_id( "test_ear_plugs" ) );

    REQUIRE( bag->contents.insert_item( item::spawn( "test_ear_plugs" ) ).success() );

    CHECK( bag->contents.get_pockets()[1].all_items_top().size() == 1 );
}

TEST_CASE( "a_disabled_pocket_takes_nothing_automatically", "[item][pocket][favorites]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    bag->contents.get_pockets()[0].get_settings().set_disabled( true );

    REQUIRE( bag->contents.insert_item( item::spawn( "test_ear_plugs" ) ).success() );

    CHECK( bag->contents.get_pockets()[0].empty() );
    CHECK( bag->contents.get_pockets()[1].all_items_top().size() == 1 );
}

// Deliberate placement is the player's own decision, so their standing filters
// must not overrule it.
TEST_CASE( "ignoring_settings_reaches_a_disabled_pocket", "[item][pocket][favorites]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    bag->contents.get_pockets()[0].get_settings().set_disabled( true );

    detached_ptr<item> plugs = item::spawn( "test_ear_plugs" );
    CHECK( bag->contents.best_pocket( *plugs, true ) == &bag->contents.get_pockets()[0] );
}

TEST_CASE( "classic_mode_ignores_pocket_settings", "[item][pocket][favorites][classic]" )
{
    override_option classic( "POCKET_SYSTEM", "classic" );

    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    bag->contents.get_pockets()[0].get_settings().set_disabled( true );
    bag->contents.get_pockets()[1].get_settings().set_priority( 10 );

    REQUIRE( bag->contents.insert_item( item::spawn( "test_ear_plugs" ) ).success() );

    // First fit, exactly as if nothing had been organised.
    CHECK( bag->contents.get_pockets()[0].all_items_top().size() == 1 );
}

// ---------------------------------------------------------------------------
// Pockets derived from use_actions
// ---------------------------------------------------------------------------

// A sheath's capacity lives in its holster use_action, not in a storage field.
// Before this was synthesized it had no pocket at all, and sheathing a knife
// failed outright once insertion was enforced.
TEST_CASE( "a_sheath_can_hold_its_knife", "[item][pocket][synthesis]" )
{
    detached_ptr<item> sheath = item::spawn( "sheath" );
    REQUIRE_FALSE( sheath->contents.get_pockets().empty() );

    CHECK( sheath->contents.insert_item( item::spawn( "knife_combat" ) ).success() );
}

TEST_CASE( "a_holster_pocket_takes_only_what_the_action_allows", "[item][pocket][synthesis]" )
{
    detached_ptr<item> sheath = item::spawn( "sheath" );
    // Not a knife, and carrying none of the action's flags.
    CHECK_FALSE( sheath->contents.insert_item( item::spawn( "rock" ) ).success() );
}

TEST_CASE( "a_holster_holds_one_thing_at_a_time", "[item][pocket][synthesis]" )
{
    detached_ptr<item> sheath = item::spawn( "sheath" );
    REQUIRE( sheath->contents.insert_item( item::spawn( "knife_combat" ) ).success() );

    CHECK_FALSE( sheath->contents.insert_item( item::spawn( "knife_combat" ) ).success() );
}

// The M240's bipod is fitted at the factory and the gun lists no underbarrel
// slot, so a pocket keyed on mod locations alone cannot hold it.
TEST_CASE( "a_gun_can_hold_its_built_in_mods", "[item][pocket][synthesis]" )
{
    detached_ptr<item> gun = item::spawn( "m240" );
    detached_ptr<item> bipod = item::spawn( "bipod" );

    CHECK( gun->contents.insert_item( std::move( bipod ) ).success() );
}

TEST_CASE( "the_built_in_mod_pocket_takes_nothing_else", "[item][pocket][synthesis]" )
{
    detached_ptr<item> gun = item::spawn( "m240" );
    // Another underbarrel mod, the location the M240 lacks. The general mod
    // pocket refuses it on location and the built-in pocket does not name it,
    // so the gun that accepts its own bipod still accepts nothing beside it.
    detached_ptr<item> mod = item::spawn( "laser_sight" );
    REQUIRE( mod->is_gunmod() );
    REQUIRE( gun->type->gun->valid_mod_locations.count(
                 mod->type->gunmod->location ) == 0 );

    CHECK_FALSE( gun->contents.insert_item( std::move( mod ) ).success() );
}

// A caliber conversion rewrites which magazines a gun takes. The magazine well
// was built from the gun's own list, before any mod existed, so it has to ask
// the gun as it is now rather than as it was defined.
TEST_CASE( "a_converted_gun_accepts_its_conversion_magazines", "[item][pocket][synthesis]" )
{
    detached_ptr<item> gun = item::spawn( "smg_luty" );
    detached_ptr<item> mag = item::spawn( "smg_40_mag" );
    mag->contents.clear_items();
    REQUIRE_FALSE( gun->contents.insert_item( std::move( mag ) ).success() );

    detached_ptr<item> converted = item::spawn( "smg_luty" );
    converted->contents.insert_item_forced( item::spawn( "retool_luty_40" ) );
    REQUIRE( converted->magazine_compatible().count( itype_id( "smg_40_mag" ) ) == 1 );

    detached_ptr<item> mag2 = item::spawn( "smg_40_mag" );
    mag2->contents.clear_items();
    CHECK( converted->contents.insert_item( std::move( mag2 ) ).success() );
}

// The handmade carbine is defined with an internal clip and so has no magazine
// well at all; its BAR magazine adapter still has to leave the magazine
// somewhere in the gun.
TEST_CASE( "an_adapter_gives_an_internal_clip_gun_somewhere_for_its_magazine",
           "[item][pocket][synthesis]" )
{
    detached_ptr<item> gun = item::spawn( "handmade_carbine" );
    detached_ptr<item> mag = item::spawn( "m1918mag" );
    mag->contents.clear_items();
    REQUIRE_FALSE( gun->contents.insert_item( std::move( mag ) ).success() );

    detached_ptr<item> adapted = item::spawn( "handmade_carbine" );
    adapted->contents.insert_item_forced( item::spawn( "hc_bar_mag_adapter" ) );
    REQUIRE( adapted->magazine_compatible().count( itype_id( "m1918mag" ) ) == 1 );

    detached_ptr<item> mag2 = item::spawn( "m1918mag" );
    mag2->contents.clear_items();
    CHECK( adapted->contents.insert_item( std::move( mag2 ) ).success() );
}

// Item info: what the player set has to be visible somewhere.

static std::string pocket_info_text( const item &it )
{
    std::vector<iteminfo> info;
    it.pocket_info( info, &iteminfo_query::all, 1, false );
    std::string text;
    for( const iteminfo &line : info ) {
        text += line.sName + line.sFmt;
    }
    return text;
}

TEST_CASE( "item_info_reports_what_the_player_organised", "[item][pocket][favorites][info]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    pocket_favorite_settings &settings = bag->contents.get_pockets()[1].get_settings();
    settings.set_priority( 4 );
    settings.blacklist_item( itype_id( "test_rock" ) );

    const std::string text = pocket_info_text( *bag );
    CHECK( text.find( "Organised" ) != std::string::npos );
    CHECK( text.find( "priority" ) != std::string::npos );
}

// A gun with RELOAD_EJECT keeps its spent hull inside itself, and a brass
// catcher does the same for every gun. Before pockets, `put_in` accepted that
// unconditionally; a gun's magazine pocket takes live rounds only, so without a
// pocket for casings every shot fired a debugmsg.
TEST_CASE( "a_gun_can_hold_its_own_spent_casing", "[item][pocket][casings]" )
{
    detached_ptr<item> gun = item::spawn( "shotgun_410" );
    detached_ptr<item> hull = item::spawn( "410shot_hull" );
    hull->set_flag( flag_CASING );

    item_pocket *pocket = gun->contents.best_pocket( *hull );
    REQUIRE( pocket != nullptr );
    CHECK( pocket->definition().type == pocket_type::CASINGS );
}

// The casings pocket is plumbing. It is not storage the player can use, so it
// must not appear as storage anywhere.
TEST_CASE( "the_casings_pocket_is_not_described", "[item][pocket][casings]" )
{
    detached_ptr<item> gun = item::spawn( "shotgun_410" );
    detached_ptr<item> rock = item::spawn( "test_rock" );

    CHECK( gun->contents.best_pocket( *rock ) == nullptr );
    CHECK( pocket_info_text( *gun ).find( "CASINGS" ) == std::string::npos );
}

// A pocket with no declared volume holds nothing, and describing storage the
// player does not have is worse than saying nothing.
TEST_CASE( "a_pocket_with_no_volume_holds_nothing", "[item][pocket][info]" )
{
    detached_ptr<item> thing = item::spawn( "test_no_volume_pocket_thing" );
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );

    CHECK_FALSE( thing->contents.get_pockets()[0].can_hold_anything() );
    CHECK( bag->contents.get_pockets()[0].can_hold_anything() );
}

TEST_CASE( "item_info_omits_a_pocket_that_holds_nothing", "[item][pocket][info]" )
{
    detached_ptr<item> thing = item::spawn( "test_no_volume_pocket_thing" );

    CHECK( pocket_info_text( *thing ).empty() );
}

// The organizer is the one screen that shows pockets, so it has to show what is
// in them: BN's inventory screens flatten pockets away entirely.
TEST_CASE( "the_organizer_lists_what_a_pocket_holds", "[item][pocket][favorites][menu]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    detached_ptr<item> rock = item::spawn( "test_rock" );
    bag->contents.get_pockets()[0].insert( std::move( rock ) );

    const std::vector<std::string> rows = bag->contents.get_pockets()[0].contents_rows();
    REQUIRE( rows.size() == 1 );
    CHECK( rows[0].find( item::nname( itype_id( "test_rock" ) ) ) != std::string::npos );
}

TEST_CASE( "the_organizer_lists_nothing_for_an_empty_pocket", "[item][pocket][favorites][menu]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );

    CHECK( bag->contents.get_pockets()[0].contents_rows().empty() );
}

// Nothing else in the game says which pocket an item ended up in, so the routing
// is invisible without this.
TEST_CASE( "item_info_lists_what_each_pocket_holds", "[item][pocket][favorites][info]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    detached_ptr<item> rock = item::spawn( "test_rock" );
    bag->contents.get_pockets()[0].insert( std::move( rock ) );

    const std::string text = pocket_info_text( *bag );
    CHECK( text.find( "Contains" ) != std::string::npos );
    CHECK( text.find( item::nname( itype_id( "test_rock" ) ) ) != std::string::npos );
}

TEST_CASE( "item_info_says_nothing_about_an_empty_pocket", "[item][pocket][favorites][info]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );

    CHECK( pocket_info_text( *bag ).find( "Contains" ) == std::string::npos );
}

TEST_CASE( "item_info_stays_quiet_about_pockets_nobody_organised",
           "[item][pocket][favorites][info]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );

    CHECK( pocket_info_text( *bag ).find( "Organised" ) == std::string::npos );
}

TEST_CASE( "classic_mode_says_nothing_about_pockets", "[item][pocket][favorites][info][classic]" )
{
    override_option classic( "POCKET_SYSTEM", "classic" );

    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    bag->contents.get_pockets()[0].get_settings().set_priority( 2 );

    CHECK( pocket_info_text( *bag ).empty() );
}

// Presets: named settings the player can reuse across pockets and worlds.

TEST_CASE( "a_preset_carries_the_rules_it_was_saved_from", "[item][pocket][favorites][preset]" )
{
    pocket_favorite_settings source;
    source.set_priority( 6 );
    source.whitelist_item( itype_id( "test_rock" ) );
    source.set_preset_name( "rocks first" );

    pocket_favorite_settings applied = source;
    CHECK( applied.priority() == 6 );
    CHECK( applied.get_item_whitelist().count( itype_id( "test_rock" ) ) == 1 );
    CHECK( applied.get_preset_name() == "rocks first" );
}

TEST_CASE( "a_preset_survives_a_serialization_round_trip", "[item][pocket][favorites][preset]" )
{
    pocket_favorite_settings preset;
    preset.set_priority( 2 );
    preset.blacklist_category( item_category_id( "food" ) );
    preset.set_preset_name( "no food" );

    std::ostringstream os;
    JsonOut jo( os );
    preset.serialize( jo );

    pocket_favorite_settings restored;
    std::istringstream is( os.str() );
    JsonIn ji( is );
    restored.deserialize( ji );

    CHECK( restored.get_preset_name() == "no food" );
    CHECK( restored.priority() == 2 );
    CHECK( restored.get_category_blacklist().count( item_category_id( "food" ) ) == 1 );
}

// Settings with no preset must not claim one, or every save would grow a name.
TEST_CASE( "settings_without_a_preset_write_no_name", "[item][pocket][favorites][preset]" )
{
    pocket_favorite_settings settings;
    settings.set_priority( 1 );

    std::ostringstream os;
    JsonOut jo( os );
    settings.serialize( jo );

    CHECK( os.str().find( "name" ) == std::string::npos );
}

// Sealing and preserving belong to the pocket, not to the whole item.

TEST_CASE( "a_sealed_pocket_seals_what_is_in_it", "[item][pocket][seal]" )
{
    detached_ptr<item> box = item::spawn( "test_sealed_pocket_box" );
    REQUIRE( box->contents.get_pockets().size() == 2 );
    REQUIRE( box->contents.get_pockets()[0].definition().sealed );

    detached_ptr<item> food = item::spawn( "test_pine_nuts" );
    food->charges = 1;
    item &sealed_food = *food;
    box->contents.get_pockets()[0].insert( std::move( food ) );

    CHECK( sealed_food.is_in_sealing_container() );
    CHECK( sealed_food.is_in_preserving_container() );
}

TEST_CASE( "the_open_pocket_of_a_sealed_item_seals_nothing", "[item][pocket][seal]" )
{
    detached_ptr<item> box = item::spawn( "test_sealed_pocket_box" );
    REQUIRE_FALSE( box->contents.get_pockets()[1].definition().sealed );

    detached_ptr<item> food = item::spawn( "test_pine_nuts" );
    food->charges = 1;
    item &loose_food = *food;
    box->contents.get_pockets()[1].insert( std::move( food ) );

    CHECK_FALSE( loose_food.is_in_sealing_container() );
    CHECK_FALSE( loose_food.is_in_preserving_container() );
}

TEST_CASE( "pocket_containing_finds_the_right_compartment", "[item][pocket][seal]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    detached_ptr<item> rock = item::spawn( "test_rock" );
    item &stored = *rock;
    bag->contents.get_pockets()[1].insert( std::move( rock ) );

    CHECK( bag->contents.pocket_containing( stored ) == &bag->contents.get_pockets()[1] );

    detached_ptr<item> elsewhere = item::spawn( "test_rock" );
    CHECK( bag->contents.pocket_containing( *elsewhere ) == nullptr );
}

// ---------------------------------------------------------------------------
// Routing: pickup tries worn pockets before the flat inventory
// (docs/superpowers/plans/2026-08-31-pocket-pickup-routing.md)
// ---------------------------------------------------------------------------

TEST_CASE( "picking_up_fills_a_worn_pocket_first", "[pocket][routing]" )
{
    clear_all_state();
    standard_npc dummy( "wearer" );
    dummy.wear_item( item::spawn( "test_pocket_vest" ) );

    detached_ptr<item> rock = item::spawn( "test_rock" );
    item &stored = *rock;
    detached_ptr<item> left = dummy.i_add_to_worn_pockets( std::move( rock ) );

    CHECK( !left );
    const item *vest = dummy.worn.front();
    CHECK( vest->contents.pocket_containing( stored ) != nullptr );
}

TEST_CASE( "a_priority_pocket_wins_across_garments", "[pocket][routing][favorites]" )
{
    clear_all_state();
    standard_npc dummy( "wearer" );
    dummy.wear_item( item::spawn( "test_pocket_vest" ) );
    dummy.wear_item( item::spawn( "test_pocket_vest" ) );

    item *first = dummy.worn.front();
    item *second = dummy.worn.back();
    second->contents.get_pockets()[1].get_settings().set_priority( 5 );

    detached_ptr<item> rock = item::spawn( "test_rock" );
    item &stored = *rock;
    REQUIRE( !dummy.i_add_to_worn_pockets( std::move( rock ) ) );

    CHECK( second->contents.pocket_containing( stored ) != nullptr );
    CHECK( first->contents.pocket_containing( stored ) == nullptr );
}

TEST_CASE( "an_item_barred_everywhere_stays_out_of_pockets", "[pocket][routing][favorites]" )
{
    clear_all_state();
    standard_npc dummy( "wearer" );
    dummy.wear_item( item::spawn( "test_pocket_vest" ) );
    item *vest = dummy.worn.front();
    for( item_pocket &pocket : vest->contents.get_pockets() ) {
        pocket.get_settings().blacklist_item( itype_id( "test_rock" ) );
    }

    detached_ptr<item> rock = item::spawn( "test_rock" );
    detached_ptr<item> left = dummy.i_add_to_worn_pockets( std::move( rock ) );

    REQUIRE( left );
    CHECK( vest->contents.empty() );
}

TEST_CASE( "a_disabled_pocket_takes_nothing_on_pickup", "[pocket][routing][favorites]" )
{
    clear_all_state();
    standard_npc dummy( "wearer" );
    dummy.wear_item( item::spawn( "test_pocket_vest" ) );
    item *vest = dummy.worn.front();
    for( item_pocket &pocket : vest->contents.get_pockets() ) {
        pocket.get_settings().set_disabled( true );
    }

    detached_ptr<item> rock = item::spawn( "test_rock" );
    detached_ptr<item> left = dummy.i_add_to_worn_pockets( std::move( rock ) );

    REQUIRE( left );
    CHECK( vest->contents.empty() );
}

TEST_CASE( "classic_mode_never_routes_into_pockets", "[pocket][routing][classic]" )
{
    override_option classic( "POCKET_SYSTEM", "classic" );
    clear_all_state();
    standard_npc dummy( "wearer" );
    dummy.wear_item( item::spawn( "test_pocket_vest" ) );

    detached_ptr<item> rock = item::spawn( "test_rock" );
    detached_ptr<item> left = dummy.i_add_to_worn_pockets( std::move( rock ) );

    REQUIRE( left );
    CHECK( dummy.worn.front()->contents.empty() );
}

// Free space must not depend on where an item sits. A routed item counts as
// carried exactly as a flat-inventory item would, so routing can never mint
// extra capacity.
TEST_CASE( "routing_does_not_mint_carry_capacity", "[pocket][routing]" )
{
    clear_all_state();
    standard_npc dummy( "wearer" );
    dummy.wear_item( item::spawn( "test_pocket_vest" ) );

    const units::volume free_before = dummy.volume_capacity() - dummy.volume_carried();
    detached_ptr<item> rock = item::spawn( "test_rock" );
    const units::volume rock_volume = rock->volume();
    REQUIRE( !dummy.i_add_to_worn_pockets( std::move( rock ) ) );

    CHECK( dummy.volume_capacity() - dummy.volume_carried() == free_before - rock_volume );
}

// ---------------------------------------------------------------------------
// Enforcement: what no worn pocket accepts cannot be stashed by pickup
// (docs/superpowers/plans/2026-08-31-pocket-enforcement-pickup.md)
// ---------------------------------------------------------------------------

// Wears a vest, bars rocks from every pocket when asked, drops a rock at the
// avatar's feet, and runs the real non-interactive pickup over it.
static item &enforcement_setup( bool barred )
{
    avatar &u = g->u;
    get_map().i_clear( u.bub_pos() );
    REQUIRE( !u.wear_item( item::spawn( "test_pocket_vest" ) ) );
    // Wearing charges moves, and do_pickup's loop refuses to start below zero.
    u.moves = 100;
    if( barred ) {
        for( item_pocket &pocket : u.worn.front()->contents.get_pockets() ) {
            pocket.get_settings().blacklist_item( itype_id( "test_rock" ) );
        }
    }
    get_map().add_item_or_charges( u.bub_pos(), item::spawn( "test_rock" ) );
    // Ask the map what it actually holds: add_item_or_charges may not keep the
    // object it was handed, and a stale reference makes pickup silently skip.
    map_stack stack = get_map().i_at( u.bub_pos() );
    REQUIRE( stack.size() == 1 );
    return **stack.begin();
}

TEST_CASE( "pickup_stashes_into_a_worn_pocket_end_to_end", "[pocket][routing][enforce]" )
{
    clear_all_state();
    item &rock = enforcement_setup( false );

    std::vector<pickup::pick_drop_selection> targets{ { rock, std::nullopt, {} } };
    REQUIRE( pickup::do_pickup( targets, true ) );

    CHECK( get_map().i_at( g->u.bub_pos() ).empty() );
    // Identity does not survive i_add's restacking, so ask by type: some worn
    // pocket holds a rock, and the flat inventory does not.
    int in_pockets = 0;
    for( const item *garment : g->u.worn ) {
        for( const item_pocket &pocket : garment->contents.get_pockets() ) {
            for( const item *stored : pocket.all_items_top() ) {
                if( stored->typeId() == itype_id( "test_rock" ) ) {
                    in_pockets++;
                }
            }
        }
    }
    CHECK( in_pockets == 1 );
    // One rock total on the character, and the loop above found it in a
    // pocket, so the flat inventory cannot also be holding one.
    CHECK( g->u.amount_of( itype_id( "test_rock" ) ) == 1 );
}

TEST_CASE( "full_pockets_refuse_to_stash_what_no_pocket_takes", "[pocket][routing][enforce]" )
{
    clear_all_state();
    item &rock = enforcement_setup( true );

    std::vector<pickup::pick_drop_selection> targets{ { rock, std::nullopt, {} } };
    pickup::do_pickup( targets, true );

    // Refused: the rock stays on the ground, nothing was quietly stashed.
    CHECK( get_map().i_at( g->u.bub_pos() ).size() == 1 );
    CHECK( g->u.worn.front()->contents.empty() );
}

// Regression: a new character's gear sits in worn pockets with an empty flat
// inventory. Carried volume counts those pockets, so the overflow check fired
// and asked the empty inventory to shed volume, walking off items.end() and
// crashing during worldgen (crash.log, 4620566dba).
TEST_CASE( "overflowing_worn_pockets_do_not_crash_the_overflow_drop", "[pocket][routing]" )
{
    clear_all_state();
    // Not the avatar: the test avatar carries DEBUG_STORAGE, whose infinite
    // capacity means the overflow branch this guards can never fire.
    standard_npc u( "overloaded" );
    REQUIRE( !u.wear_item( item::spawn( "test_overfull_pocket_vest" ) ) );

    item *vest = u.worn.front();
    // Forced insertion is how bad data gets past a pocket's capacity: item.cpp
    // keeps a refused item rather than destroying it. That is the only way to
    // end up carrying more than the pockets can hold now that capacity comes
    // from the pockets themselves.
    for( int i = 0; i < 40; i++ ) {
        vest->contents.insert_item_forced( item::spawn( "test_rock" ) );
    }

    REQUIRE( u.volume_carried() > u.volume_capacity() );
    REQUIRE( u.inv_size() == 0 );
    const units::volume carried_before = u.volume_carried();

    u.drop_invalid_inventory();

    // Nothing to shed, so nothing is shed - and above all, no crash.
    CHECK( u.volume_carried() == carried_before );
}

// Enforcement binds only characters who actually wear pockets. Anyone without
// them keeps the legacy flat stash, so ungeared characters and old content
// keep working until the day the flat inventory itself is retired.
TEST_CASE( "a_character_with_no_pockets_still_stashes_flat", "[pocket][routing][enforce]" )
{
    clear_all_state();
    avatar &u = g->u;
    get_map().i_clear( u.bub_pos() );
    u.moves = 100;
    get_map().add_item_or_charges( u.bub_pos(), item::spawn( "test_rock" ) );
    map_stack stack = get_map().i_at( u.bub_pos() );
    REQUIRE( stack.size() == 1 );
    item &rock = **stack.begin();

    std::vector<pickup::pick_drop_selection> targets{ { rock, std::nullopt, {} } };
    pickup::do_pickup( targets, true );

    CHECK( get_map().i_at( u.bub_pos() ).empty() );
    CHECK( u.amount_of( itype_id( "test_rock" ) ) == 1 );
}

TEST_CASE( "classic_mode_still_stashes_to_the_flat_inventory", "[pocket][routing][enforce][classic]" )
{
    override_option classic( "POCKET_SYSTEM", "classic" );
    clear_all_state();
    item &rock = enforcement_setup( true );

    std::vector<pickup::pick_drop_selection> targets{ { rock, std::nullopt, {} } };
    pickup::do_pickup( targets, true );

    CHECK( get_map().i_at( g->u.bub_pos() ).empty() );
    CHECK( g->u.amount_of( itype_id( "test_rock" ) ) == 1 );
    CHECK( g->u.worn.front()->contents.empty() );
}

// Reproducing the user's playtest: real clothing, default settings, no priorities
// touched. Routing must work out of the box or the feature is invisible.
TEST_CASE( "default_settings_route_into_real_clothing", "[pocket][routing][repro]" )
{
    clear_all_state();
    standard_npc dummy( "dressed" );
    REQUIRE( !dummy.wear_item( item::spawn( "dress_shirt" ) ) );
    REQUIRE( !dummy.wear_item( item::spawn( "jeans" ) ) );

    detached_ptr<item> plant = item::spawn( "withered" );
    CAPTURE( plant->tname(), units::to_milliliter( plant->volume() ) );
    detached_ptr<item> left = dummy.i_add_to_worn_pockets( std::move( plant ) );

    CHECK( !left );
    int in_pockets = 0;
    for( const item *garment : dummy.worn ) {
        for( const item_pocket &pocket : garment->contents.get_pockets() ) {
            in_pockets += pocket.all_items_top().size();
        }
    }
    CHECK( in_pockets == 1 );
}

// The user's playtest: routing filled the pockets, but capacity was still measured
// by the legacy `storage` field - jeans declare 500 ml and carry 4,660 ml of
// pockets - so a character went "full" almost immediately and pickup refused
// everything. Capacity and contents must be measured against the same thing.
TEST_CASE( "capacity_comes_from_the_pockets_that_hold_things", "[pocket][routing][capacity]" )
{
    clear_all_state();
    detached_ptr<item> jeans = item::spawn( "jeans" );
    units::volume pocket_total = 0_ml;
    for( const item_pocket &pocket : jeans->contents.get_pockets() ) {
        pocket_total += pocket.definition().max_contains_volume;
    }
    REQUIRE( pocket_total > jeans->get_storage() );
    CHECK( jeans->storage_capacity() == pocket_total );
}

// A garment with no authored pockets keeps exactly its old capacity: synthesis
// builds its single pocket out of the same legacy field.
TEST_CASE( "unpocketed_clothing_keeps_its_old_capacity", "[pocket][routing][capacity]" )
{
    clear_all_state();
    detached_ptr<item> shirt = item::spawn( "dress_shirt" );

    CHECK( shirt->storage_capacity() == shirt->get_storage() );
}

TEST_CASE( "classic_mode_measures_capacity_the_old_way", "[pocket][routing][capacity][classic]" )
{
    override_option classic( "POCKET_SYSTEM", "classic" );
    clear_all_state();
    detached_ptr<item> jeans = item::spawn( "jeans" );

    CHECK( jeans->storage_capacity() == jeans->get_storage() );
}

// The whole point: fill a real pair of jeans past the old 500 ml limit.
TEST_CASE( "a_character_can_fill_the_pockets_they_have", "[pocket][routing][capacity]" )
{
    clear_all_state();
    standard_npc dummy( "dressed" );
    REQUIRE( !dummy.wear_item( item::spawn( "jeans" ) ) );

    int stored = 0;
    for( int i = 0; i < 8; i++ ) {
        detached_ptr<item> plant = item::spawn( "withered" );
        if( !dummy.i_add_to_worn_pockets( std::move( plant ) ) ) {
            stored++;
        }
    }
    // 8 x 250ml = 2L, far past the legacy 500ml but well inside 4.66L of pockets.
    CHECK( stored == 8 );
    CHECK( dummy.volume_carried() <= dummy.volume_capacity() );
}

// Pockets made every garment a container, so BN's "contents (container)" name
// turned a pair of jeans holding one plant into "withered plant (jeans)".
// That form belongs to vessels only.
TEST_CASE( "clothing_keeps_its_own_name_when_it_holds_something", "[pocket][naming]" )
{
    clear_all_state();
    detached_ptr<item> jeans = item::spawn( "jeans" );
    REQUIRE( !jeans->put_in( item::spawn( "withered" ) ) );

    const std::string name = jeans->tname();
    CAPTURE( name );
    CHECK( name.find( "jeans" ) != std::string::npos );
    CHECK( name.find( "withered" ) == std::string::npos );
}

// A bottle of water is still a bottle of water.
TEST_CASE( "a_vessel_still_takes_the_name_of_what_is_in_it", "[pocket][naming]" )
{
    clear_all_state();
    detached_ptr<item> bottle = item::spawn( "bottle_plastic" );
    detached_ptr<item> water = item::spawn( "water" );
    water->charges = 1;
    REQUIRE( !bottle->put_in( std::move( water ) ) );

    const std::string name = bottle->tname();
    CAPTURE( name );
    CHECK( name.find( "water" ) != std::string::npos );
}

// Report 3 from the user's playtest: an item routed into a pair of jeans could not
// be got at again. Unload already empties containers, but a garment is not a
// container, a bandolier or a holster, so it refused to fire at all.
TEST_CASE( "unloading_a_garment_empties_its_pockets", "[pocket][unload]" )
{
    clear_all_state();
    avatar &u = g->u;
    REQUIRE( !u.wear_item( item::spawn( "jeans" ) ) );
    item *jeans = u.worn.front();
    REQUIRE( !jeans->put_in( item::spawn( "withered" ) ) );
    REQUIRE( !jeans->contents.empty() );

    u.moves = 100;
    CHECK( avatar_funcs::unload_item( u, *jeans ) );

    CHECK( jeans->contents.empty() );
    CHECK( u.amount_of( itype_id( "withered" ) ) == 1 );
}

// The user's playtest: routing looked intermittent. BN groups a pickup into a
// parent and its children, and the children were added straight to the flat
// inventory, so a batched pickup routed the first item and no others.
TEST_CASE( "children_of_a_pickup_route_like_their_parent", "[pocket][routing][enforce]" )
{
    clear_all_state();
    avatar &u = g->u;
    get_map().i_clear( u.bub_pos() );
    REQUIRE( !u.wear_item( item::spawn( "test_pocket_vest" ) ) );
    u.moves = 100;

    get_map().add_item_or_charges( u.bub_pos(), item::spawn( "bag_plastic" ) );
    get_map().add_item_or_charges( u.bub_pos(), item::spawn( "test_rock" ) );
    map_stack stack = get_map().i_at( u.bub_pos() );
    REQUIRE( stack.size() == 2 );

    item *bag = nullptr;
    item *rock = nullptr;
    for( item *it : stack ) {
        ( it->typeId() == itype_id( "test_rock" ) ? rock : bag ) = it;
    }
    REQUIRE( bag != nullptr );
    REQUIRE( rock != nullptr );

    // The rock rides along as a child of the bag, the shape BN builds when a
    // pickup is batched.
    std::vector<pickup::pick_drop_selection> targets{ { *bag, std::nullopt, { *rock } } };
    pickup::do_pickup( targets, true );

    int rocks_in_pockets = 0;
    for( const item *garment : u.worn ) {
        for( const item_pocket &pocket : garment->contents.get_pockets() ) {
            for( const item *stored : pocket.all_items_top() ) {
                if( stored->typeId() == itype_id( "test_rock" ) ) {
                    rocks_in_pockets++;
                }
            }
        }
    }
    CHECK( rocks_in_pockets == 1 );
}

// Counts items of a type sitting in a garment's pockets, as opposed to the flat
// inventory the pockets are meant to replace.
static int items_in_pockets( const item &garment, const itype_id &what )
{
    int found = 0;
    for( const item_pocket &pocket : garment.contents.get_pockets() ) {
        for( const item *stored : pocket.all_items_top() ) {
            if( stored->typeId() == what ) {
                found++;
            }
        }
    }
    return found;
}

// Unloading spent the pocket budget without using a pocket: the freed item went
// straight to the flat inventory via i_add, charged against capacity the pockets
// had granted but stored nowhere near them. Route it like a pickup instead.
TEST_CASE( "unloading_routes_into_a_worn_pocket", "[pocket][routing][unload]" )
{
    clear_all_state();
    avatar &u = g->u;
    REQUIRE( !u.wear_item( item::spawn( "test_pocket_vest" ) ) );
    item *vest = u.worn.front();

    detached_ptr<item> bag = item::spawn( "bag_plastic" );
    REQUIRE( !bag->put_in( item::spawn( "test_rock" ) ) );
    item &held = u.i_add( std::move( bag ) );

    u.moves = 100;
    CHECK( avatar_funcs::unload_item( u, held ) );

    CHECK( items_in_pockets( *vest, itype_id( "test_rock" ) ) == 1 );
}

// The garment being emptied must not take its own contents back. Without the
// exclusion, `U` on worn clothing routes each item straight home and reads as a
// no-op, and the reinsertion happens inside the very iteration doing the removal.
TEST_CASE( "unloading_does_not_refill_the_garment_being_emptied",
           "[pocket][routing][unload]" )
{
    clear_all_state();
    avatar &u = g->u;
    REQUIRE( !u.wear_item( item::spawn( "test_pocket_vest" ) ) );
    REQUIRE( !u.wear_item( item::spawn( "jeans" ) ) );

    item *vest = u.worn.front();
    item *jeans = u.worn.back();
    REQUIRE( vest->typeId() == itype_id( "test_pocket_vest" ) );
    REQUIRE( jeans->typeId() == itype_id( "jeans" ) );
    REQUIRE( !jeans->put_in( item::spawn( "test_rock" ) ) );

    u.moves = 100;
    CHECK( avatar_funcs::unload_item( u, *jeans ) );

    CHECK( jeans->contents.empty() );
    CHECK( items_in_pockets( *vest, itype_id( "test_rock" ) ) == 1 );
}

// Classic mode is stock BN: no routing, the flat inventory takes everything.
TEST_CASE( "classic_mode_unloads_to_the_flat_inventory",
           "[pocket][routing][unload][classic]" )
{
    override_option classic( "POCKET_SYSTEM", "classic" );
    clear_all_state();
    avatar &u = g->u;
    REQUIRE( !u.wear_item( item::spawn( "test_pocket_vest" ) ) );
    item *vest = u.worn.front();

    detached_ptr<item> bag = item::spawn( "bag_plastic" );
    REQUIRE( !bag->put_in( item::spawn( "test_rock" ) ) );
    item &held = u.i_add( std::move( bag ) );

    u.moves = 100;
    CHECK( avatar_funcs::unload_item( u, held ) );

    CHECK( vest->contents.empty() );
    CHECK( u.amount_of( itype_id( "test_rock" ) ) == 1 );
}

// Unlike pickup, unloading never drops. Everything freed was already carried and
// already counted against capacity, so the flat inventory costs nothing and is
// where stock BN puts it. Emptying the only garment worn must not tip its
// contents onto the floor.
TEST_CASE( "unloading_keeps_what_no_worn_pocket_will_hold",
           "[pocket][routing][unload][enforce]" )
{
    clear_all_state();
    avatar &u = g->u;
    get_map().i_clear( u.bub_pos() );
    REQUIRE( !u.wear_item( item::spawn( "test_pocket_vest" ) ) );
    item *vest = u.worn.front();
    for( item_pocket &pocket : vest->contents.get_pockets() ) {
        pocket.get_settings().set_disabled( true );
    }

    detached_ptr<item> bag = item::spawn( "bag_plastic" );
    REQUIRE( !bag->put_in( item::spawn( "test_rock" ) ) );
    item &held = u.i_add( std::move( bag ) );

    u.moves = 100;
    CHECK( avatar_funcs::unload_item( u, held ) );

    CHECK( vest->contents.empty() );
    CHECK( u.amount_of( itype_id( "test_rock" ) ) == 1 );
    int rocks_on_ground = 0;
    for( const item * const &it : get_map().i_at( u.bub_pos() ) ) {
        if( it->typeId() == itype_id( "test_rock" ) ) {
            rocks_on_ground++;
        }
    }
    CHECK( rocks_on_ground == 0 );
}

// A character wearing nothing with pockets keeps the legacy flat stash, so
// ungeared characters and old content go on working.
TEST_CASE( "unloading_without_pockets_still_stashes_flat",
           "[pocket][routing][unload][enforce]" )
{
    clear_all_state();
    avatar &u = g->u;
    REQUIRE( u.worn.empty() );

    detached_ptr<item> bag = item::spawn( "bag_plastic" );
    REQUIRE( !bag->put_in( item::spawn( "test_rock" ) ) );
    item &held = u.i_add( std::move( bag ) );

    u.moves = 100;
    CHECK( avatar_funcs::unload_item( u, held ) );

    CHECK( u.amount_of( itype_id( "test_rock" ) ) == 1 );
}

// Character creation hands out a kit item by item and wears the armour in the
// same pass, so the starting gear landed in the flat inventory with nothing to
// route into yet. The sweep afterwards is what puts it in the pockets.
// Foraging and NPC gifts acquire items without a deliberate per-item action,
// so they route into worn pockets silently rather than landing loose.
TEST_CASE( "i_add_routed puts an item in a worn pocket", "[pocket][routing]" )
{
    clear_all_state();
    avatar &u = g->u;
    REQUIRE( !u.wear_item( item::spawn( "test_pocket_vest" ) ) );
    item *vest = u.worn.front();

    u.i_add_routed( item::spawn( "test_rock" ) );

    CHECK( items_in_pockets( *vest, itype_id( "test_rock" ) ) == 1 );
}

TEST_CASE( "i_add_routed keeps what no pocket will take", "[pocket][routing]" )
{
    clear_all_state();
    avatar &u = g->u;
    get_map().i_clear( u.bub_pos() );
    REQUIRE( !u.wear_item( item::spawn( "test_pocket_vest" ) ) );
    item *vest = u.worn.front();
    for( item_pocket &pocket : vest->contents.get_pockets() ) {
        pocket.get_settings().set_disabled( true );
    }

    u.i_add_routed( item::spawn( "test_rock" ) );

    // Refused by every pocket, so it must still be carried, never dropped.
    CHECK( items_in_pockets( *vest, itype_id( "test_rock" ) ) == 0 );
    CHECK( u.has_amount( itype_id( "test_rock" ), 1 ) );
}

TEST_CASE( "starting_gear_is_stowed_into_worn_pockets", "[pocket][routing][newchar]" )
{
    clear_all_state();
    avatar &u = g->u;
    REQUIRE( !u.wear_item( item::spawn( "test_pocket_vest" ) ) );
    item *vest = u.worn.front();
    u.i_add( item::spawn( "test_rock" ) );
    REQUIRE( items_in_pockets( *vest, itype_id( "test_rock" ) ) == 0 );

    u.stow_loose_inventory_into_pockets();

    CHECK( items_in_pockets( *vest, itype_id( "test_rock" ) ) == 1 );
}

// A starting kit is never dropped. What no pocket will take stays exactly where
// character creation left it.
TEST_CASE( "starting_gear_no_pocket_takes_stays_in_the_inventory",
           "[pocket][routing][newchar]" )
{
    clear_all_state();
    avatar &u = g->u;
    get_map().i_clear( u.bub_pos() );
    REQUIRE( !u.wear_item( item::spawn( "test_pocket_vest" ) ) );
    item *vest = u.worn.front();
    for( item_pocket &pocket : vest->contents.get_pockets() ) {
        pocket.get_settings().set_disabled( true );
    }
    u.i_add( item::spawn( "test_rock" ) );

    u.stow_loose_inventory_into_pockets();

    CHECK( vest->contents.empty() );
    CHECK( u.amount_of( itype_id( "test_rock" ) ) == 1 );
    CHECK( get_map().i_at( u.bub_pos() ).empty() );
}

// Classic mode is stock BN: the starting kit stays in the flat inventory.
TEST_CASE( "classic_mode_leaves_starting_gear_in_the_inventory",
           "[pocket][routing][newchar][classic]" )
{
    override_option classic( "POCKET_SYSTEM", "classic" );
    clear_all_state();
    avatar &u = g->u;
    REQUIRE( !u.wear_item( item::spawn( "test_pocket_vest" ) ) );
    item *vest = u.worn.front();
    u.i_add( item::spawn( "test_rock" ) );

    u.stow_loose_inventory_into_pockets();

    CHECK( vest->contents.empty() );
    CHECK( u.amount_of( itype_id( "test_rock" ) ) == 1 );
}

// Stowing moves the kit around inside what the character already carries, so
// the totals it is measured by must not shift.
TEST_CASE( "stowing_starting_gear_preserves_what_is_carried", "[pocket][routing][newchar]" )
{
    clear_all_state();
    standard_npc dummy( "wearer" );
    dummy.wear_item( item::spawn( "test_pocket_vest" ) );
    dummy.i_add( item::spawn( "test_rock" ) );

    const units::volume before_volume = dummy.volume_carried();
    const units::mass before_weight = dummy.weight_carried();

    dummy.stow_loose_inventory_into_pockets();

    CHECK( dummy.volume_carried() == before_volume );
    CHECK( dummy.weight_carried() == before_weight );
    CHECK( items_in_pockets( *dummy.worn.front(), itype_id( "test_rock" ) ) == 1 );
}

// ---------------------------------------------------------------------------
// Item length limits
// ---------------------------------------------------------------------------

TEST_CASE( "a_synthesized_storage_pocket_limits_item_length", "[item][pocket][length]" )
{
    // 2250 ml of storage is a 13 cm cube, whose face diagonal is 183 mm.
    // Nothing longer goes in, however well it fits by volume.
    detached_ptr<item> hoodie = item::spawn( "hoodie" );
    const std::vector<item_pocket> &pockets = hoodie->contents.get_pockets();

    REQUIRE( pockets.size() == 1 );
    CHECK( pockets.front().definition().max_item_length == 183_mm );
}

TEST_CASE( "a_holster_pocket_keeps_no_length_limit", "[item][pocket][length]" )
{
    // A scabbard is sized for a sword by volume alone, so deriving a length
    // limit from that volume would make it refuse the weapon it exists to hold.
    detached_ptr<item> scabbard = item::spawn( "scabbard" );
    const std::vector<item_pocket> &pockets = scabbard->contents.get_pockets();

    REQUIRE( !pockets.empty() );
    CHECK( pockets.front().definition().max_item_length == 0_mm );
}

TEST_CASE( "a_wood_axe_does_not_fit_a_jean_pocket", "[item][pocket][length]" )
{
    detached_ptr<item> jeans = item::spawn( "jeans" );
    detached_ptr<item> axe = item::spawn( "ax" );

    // Jeans carry authored pocket_data, so ask every pocket: one accepting
    // pocket is enough to put the axe in a trouser leg.
    bool any_accepts = false;
    for( const item_pocket &pocket : jeans->contents.get_pockets() ) {
        INFO( "pocket volume " << units::to_milliliter( pocket.definition().max_contains_volume )
              << " ml, length " << units::to_millimeter( pocket.definition().max_item_length )
              << " mm" );
        if( pocket.can_contain( *axe ).success() ) {
            any_accepts = true;
        }
    }
    INFO( "axe is " << units::to_millimeter( axe->length() ) << " mm long, "
          << units::to_milliliter( axe->volume() ) << " ml" );
    CHECK_FALSE( any_accepts );
}

TEST_CASE( "a_long_tool_takes_the_backpack_side_pocket_not_the_main_one",
           "[item][pocket][length]" )
{
    // An axe is 78 cm of haft in 2500 ml of volume. Volume alone would drop it
    // into the 25 L main compartment, which is only 40 cm deep; it belongs in
    // one of the long side pockets instead. This is what an authored
    // longest_side buys - derived from volume the axe measures 14 cm and the
    // main compartment takes it, which is the behaviour this pins against.
    detached_ptr<item> backpack = item::spawn( "backpack" );
    detached_ptr<item> axe = item::spawn( "ax" );

    REQUIRE( axe->length() > 40_cm );

    bool main_pocket_refuses = false;
    bool a_long_pocket_accepts = false;
    for( const item_pocket &pocket : backpack->contents.get_pockets() ) {
        const units::length limit = pocket.definition().max_item_length;
        INFO( "pocket " << units::to_milliliter( pocket.definition().max_contains_volume )
              << " ml, length limit " << units::to_millimeter( limit ) << " mm" );
        if( limit == 40_cm && !pocket.can_contain( *axe ).success() ) {
            main_pocket_refuses = true;
        }
        if( limit >= 120_cm && pocket.can_contain( *axe ).success() ) {
            a_long_pocket_accepts = true;
        }
    }

    INFO( "axe is " << units::to_millimeter( axe->length() ) << " mm long" );
    CHECK( main_pocket_refuses );
    CHECK( a_long_pocket_accepts );
}

TEST_CASE( "an_authored_pocket_without_a_length_limit_omits_the_length_line",
           "[item][pocket][info][length]" )
{
    // 0 mm means "this pocket sets no length limit", so it must print nothing
    // rather than advertising a limit of zero. The bag declares 30 cm on its
    // first pocket and nothing on its second, so exactly one line may appear -
    // and counting them beats searching for "0 cm", which "30 cm" contains.
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    std::string joined;
    for( const iteminfo &entry : bag->info() ) {
        joined += entry.sName;
        joined += "\n";
    }

    int length_lines = 0;
    for( size_t at = joined.find( "items up to" ); at != std::string::npos;
         at = joined.find( "items up to", at + 1 ) ) {
        length_lines++;
    }

    INFO( joined );
    CHECK( length_lines == 1 );
    CHECK( joined.find( "30 cm" ) != std::string::npos );
}

TEST_CASE( "insert_into puts an item in the pocket it names", "[item][pocket][insert]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    REQUIRE( bag->contents.get_pockets().size() == 2 );

    // Pocket 1 holds 4 L; pocket 0 holds only 100 ml. Naming pocket 1 must put
    // it there even though best_pocket() prefers the tightest fit.
    const ret_val<bool> res = bag->contents.insert_into( 1, item::spawn( "test_rock" ) );

    CHECK( res.success() );
    CHECK( bag->contents.get_pockets()[1].all_items_top().size() == 1 );
    CHECK( bag->contents.get_pockets()[0].empty() );
}

TEST_CASE( "insert_into refuses a pocket that cannot hold the item", "[item][pocket][insert]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    detached_ptr<item> rock = item::spawn( "test_rock" );

    // Pocket 0 is 100 ml. A refusal must hand the item back, not eat it.
    const ret_val<bool> res = bag->contents.insert_into( 0, std::move( rock ) );

    CHECK_FALSE( res.success() );
    CHECK( rock );
    CHECK( bag->contents.get_pockets()[0].empty() );
}

TEST_CASE( "insert_into refuses a null item instead of dereferencing it",
           "[item][pocket][insert]" )
{
    // detached_ptr::operator* is a raw *get(), so a null one is an access
    // violation rather than a failed check. Callers hand ownership over, and a
    // moved-from or already-detached pointer arrives here as null.
    detached_ptr<item> backpack = item::spawn( "backpack" );
    detached_ptr<item> nothing;

    REQUIRE( !nothing );

    const ret_val<bool> inserted =
        backpack->contents.insert_into( 0, std::move( nothing ) );

    CHECK_FALSE( inserted.success() );
}

TEST_CASE( "insert_into refuses an index that does not exist", "[item][pocket][insert]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );
    detached_ptr<item> rock = item::spawn( "test_rock" );

    const ret_val<bool> res = bag->contents.insert_into( 99, std::move( rock ) );

    CHECK_FALSE( res.success() );
    CHECK( rock );
}

TEST_CASE( "insert_into invalidates the container-level all_items_top cache",
           "[item][pocket][insert]" )
{
    detached_ptr<item> bag = item::spawn( "test_two_pocket_bag" );

    // Prime the multi-pocket concatenated cache while it is still empty. If
    // insert_into skips invalidating it, the next call below returns this
    // stale (empty) vector instead of seeing the newly inserted item.
    REQUIRE( bag->contents.all_items_top().empty() );

    const ret_val<bool> res = bag->contents.insert_into( 1, item::spawn( "test_rock" ) );
    REQUIRE( res.success() );

    CHECK( bag->contents.all_items_top().size() == 1 );
}

// ---------------------------------------------------------------------------
// Ammo pockets count rounds, not millilitres
// ---------------------------------------------------------------------------

TEST_CASE( "a quiver holds arrows despite reporting no volume", "[item][pocket][ammo]" )
{
    // The pocket coverage report prints these as "0 ml", which reads as a
    // pocket that holds nothing. It is not: can_contain() returns on the round
    // count before volume is ever consulted, so the capacity is the 20 arrows
    // the bandolier action declares. This test exists to stop that number being
    // "fixed" by giving the pocket a volume it does not want.
    detached_ptr<item> quiver = item::spawn( "quiver" );
    detached_ptr<item> arrows = item::spawn( "arrow_wood" );

    REQUIRE( quiver->contents.get_pockets().size() == 1 );
    const item_pocket &pocket = quiver->contents.get_pockets().front();
    REQUIRE( pocket.definition().max_contains_volume == 0_ml );
    REQUIRE( !pocket.definition().ammo_restriction.empty() );

    CHECK( pocket.can_contain( *arrows ).success() );
}

TEST_CASE( "a_pocketed_item_has_no_flat_inventory_position", "[pocket][routing]" )
{
    // Dropping used to assume that an item which is neither worn nor wielded
    // sits in the flat inventory, and walked its stack by position. Routed
    // items live in a worn pocket instead, so that lookup asks for a negative
    // position in an empty inventory and the drop fails.
    clear_all_state();
    avatar &u = g->u;
    REQUIRE( !u.wear_item( item::spawn( "test_pocket_vest" ) ) );
    item *vest = u.worn.front();

    u.i_add_routed( item::spawn( "test_rock" ) );
    REQUIRE( items_in_pockets( *vest, itype_id( "test_rock" ) ) == 1 );

    item *stored = vest->contents.all_items_top().front();
    REQUIRE( !u.is_worn( *stored ) );
    REQUIRE( !u.is_wielding( *stored ) );

    INFO( "position " << u.get_item_position( stored ) );
    CHECK( u.get_item_position( stored ) < 0 );
}

TEST_CASE( "food in a worn pocket is held in storage, not built in",
           "[pocket][routing][consume]" )
{
    // The consume menu tested an item's parent for is_container() - the
    // CONTAINER itype slot, which a backpack does not have - so routing food
    // into a worn pocket hid it from the eat menu entirely. The preset itself
    // is file-local, so this pins the discriminator it now uses instead: a
    // garment is not a container, yet it holds the food in a CONTAINER pocket.
    clear_all_state();
    avatar &u = g->u;
    REQUIRE( !u.wear_item( item::spawn( "backpack" ) ) );
    item *pack = u.worn.front();

    u.i_add_routed( item::spawn( "sandwich_cheese" ) );
    REQUIRE( !pack->contents.all_items_top().empty() );
    item *meal = pack->contents.all_items_top().front();
    REQUIRE( meal->is_food() );
    REQUIRE( u.can_consume( *meal ) );

    // Why the old guard rejected it.
    CHECK_FALSE( pack->is_container() );

    // Why the new one accepts it.
    bool in_container_pocket = false;
    for( const item_pocket &pocket : pack->contents.get_pockets() ) {
        if( pocket.definition().type != pocket_type::CONTAINER ) {
            continue;
        }
        const std::vector<item *> &stored = pocket.all_items_top();
        if( std::ranges::find( stored, meal ) != stored.end() ) {
            in_container_pocket = true;
        }
    }
    CHECK( in_container_pocket );
}

TEST_CASE( "crafting sees a pocketed component", "[pocket][routing][crafting]" )
{
    clear_all_state();
    avatar &u = g->u;
    REQUIRE( !u.wear_item( item::spawn( "backpack" ) ) );

    u.i_add_routed( item::spawn( "duct_tape" ) );
    u.invalidate_crafting_inventory();

    // Prove the precondition: it really is in a pocket, not loose. Without this
    // the test passes whether or not crafting can see into pockets.
    item *pack = u.worn.front();
    REQUIRE( items_in_pockets( *pack, itype_id( "duct_tape" ) ) == 1 );
    REQUIRE( u.inv_size() == 0 );
    CHECK( u.crafting_inventory().has_amount( itype_id( "duct_tape" ), 1 ) );
}

TEST_CASE( "a pocketed item is found by items_with", "[pocket][routing]" )
{
    clear_all_state();
    avatar &u = g->u;
    REQUIRE( !u.wear_item( item::spawn( "backpack" ) ) );

    u.i_add_routed( item::spawn( "9mm", calendar::turn, 50 ) );
    item *pack = u.worn.front();
    REQUIRE( items_in_pockets( *pack, itype_id( "9mm" ) ) == 1 );
    REQUIRE( u.inv_size() == 0 );

    // What the reload UI asks: everything the character could reload from.
    const std::vector<item *> found = u.items_with( []( const item & it ) {
        return it.typeId() == itype_id( "9mm" );
    } );
    INFO( "items_with found " << found.size() );
    CHECK( !found.empty() );
}

// The wallet is the first CSE item to use flag_restriction, and the coin
// wrapper the first to use item_restriction. Both were read and enforced by
// item_pocket long before any item asked for them, so these cover the data as
// much as the code: an undeclared flag is erased from the itype on load, which
// would leave every restriction silently open.
// Deliberately put_in() and not item::can_contain(): the latter is the legacy
// container-slot check, which answers false for anything whose storage is
// pockets rather than an itype container slot. Nothing in the player's path
// calls it - insertion goes through item_contents::insert_item, which is what
// put_in() exercises here.
static bool accepts( const std::string &container, const std::string &content )
{
    detached_ptr<item> holder = item::spawn( container );
    detached_ptr<item> refused = holder->put_in( item::spawn( content ) );
    return !refused;
}

TEST_CASE( "wallet_sleeves_take_only_what_they_are_shaped_for", "[item][pocket][wallet]" )
{
    clear_all_state();
    // One per sleeve: a note, a card, a coin.
    CHECK( accepts( "wallet", "money_one" ) );
    CHECK( accepts( "wallet", "cash_card" ) );
    CHECK( accepts( "wallet", "coin_quarter" ) );
    CHECK( accepts( "wallet", "coin_penny" ) );
    // A rock is none of those shapes and fits by neither volume nor weight
    // rule but the flag one, which is the rule being tested.
    CHECK_FALSE( accepts( "wallet", "test_rock" ) );
    CHECK_FALSE( accepts( "wallet", "rock" ) );
}

TEST_CASE( "a_wallet_actually_holds_a_coin", "[item][pocket][wallet]" )
{
    clear_all_state();
    detached_ptr<item> holder = item::spawn( "wallet" );
    item *wallet = &*holder;
    REQUIRE( !wallet->put_in( item::spawn( "coin_quarter" ) ) );
    REQUIRE( wallet->contents.all_items_top().size() == 1 );
    CHECK( wallet->contents.all_items_top().front()->typeId() == itype_id( "coin_quarter" ) );
}

TEST_CASE( "a_coin_wrapper_takes_coins_and_nothing_else", "[item][pocket][wallet]" )
{
    clear_all_state();
    CHECK( accepts( "coin_wrapper", "coin_quarter" ) );
    CHECK( accepts( "coin_wrapper", "coin_penny" ) );
    // Bullion is a coin by shape but not on the wrapper's list, which is what
    // separates item_restriction from flag_restriction.
    CHECK_FALSE( accepts( "coin_wrapper", "coin_gold" ) );
    CHECK_FALSE( accepts( "coin_wrapper", "money_one" ) );
}

TEST_CASE( "the_card_sleeve_measures_length_not_just_volume", "[item][pocket][wallet]" )
{
    clear_all_state();
    // The sleeve stops at 85 mm and a card is 84 mm, so the margin is one
    // millimetre. Without the ported longest_side a 5 ml card derives about
    // 17 mm and the limit would never bite on anything card-sized.
    detached_ptr<item> card = item::spawn( "cash_card" );
    CHECK( card->length() <= 85_mm );
    CHECK( card->length() > 17_mm );
}

TEST_CASE( "the wallet spawn group fills the wallet it spawns", "[item][pocket][wallet]" )
{
    clear_all_state();
    // A restricted pocket that refuses its own spawn set would fail silently:
    // the wallet still spawns, just always empty. Probabilities in the group
    // are 60-80 per entry, so over this many draws an all-empty result is not
    // chance, it is a broken restriction.
    int wallets = 0;
    int filled = 0;
    for( int i = 0; i < 200; i++ ) {
        for( detached_ptr<item> &spawned : item_group::items_from( item_group_id( "wallets" ) ) ) {
            if( !spawned ) {
                continue;
            }
            wallets++;
            CHECK( spawned->typeId().str().starts_with( "wallet" ) );
            if( !spawned->contents.all_items_top().empty() ) {
                filled++;
            }
        }
    }
    REQUIRE( wallets > 0 );
    CHECK( filled > 0 );
}

TEST_CASE( "an ammo box is a rigid box, not a restricted one", "[item][pocket][ammobox]" )
{
    clear_all_state();
    // CDDA gives these no ammo_restriction on purpose, so a spare box takes
    // anything that fits. This pins that, so a later "tidy-up" adding a
    // restriction has to be a deliberate decision rather than a silent one.
    CHECK( accepts( "9mm_ammo_box_50", "9mm" ) );
    CHECK( accepts( "9mm_ammo_box_50", "coin_quarter" ) );
    // Rigid: what a rigid pocket holds does not swell the box.
    detached_ptr<item> holder = item::spawn( "9mm_ammo_box_50" );
    item *box = &*holder;
    const units::volume empty = box->volume();
    REQUIRE( !box->put_in( item::spawn( "9mm", calendar::turn, 10 ) ) );
    REQUIRE( !box->contents.all_items_top().empty() );
    CHECK( box->volume() == empty );
}

TEST_CASE( "an ammo box refuses what will not fit", "[item][pocket][ammobox]" )
{
    clear_all_state();
    // The smallest box holds 36 ml. A rock is 250 ml, so volume alone rejects
    // it - no flag rule involved, which is the whole difference from a wallet.
    CHECK_FALSE( accepts( "380_ammo_box_20", "test_rock" ) );
}

TEST_CASE( "classic mode drops the pocket rules and keeps volume",
           "[item][pocket][wallet][classic]" )
{
    override_option classic( "POCKET_SYSTEM", "classic" );
    clear_all_state();
    // Classic is meant to behave as inventory did before pockets: volume and
    // weight only. So the wallet stops being shaped and becomes three plain
    // compartments, and a rock goes in one.
    CHECK( accepts( "wallet", "coin_quarter" ) );
    // A length of string is none of the three shapes, so in pockets mode the
    // wallet refuses it. Here it goes in, which is the rule being switched off.
    CHECK( accepts( "wallet", "string_6" ) );
    // Volume still bites: the largest sleeve is 150 ml and test_rock is 250 ml,
    // so "no rules" is not "no limits".
    CHECK_FALSE( accepts( "wallet", "test_rock" ) );
    // An ammo box is one pocket, so in classic it is simply a box by volume.
    CHECK( accepts( "9mm_ammo_box_50", "coin_quarter" ) );
}
