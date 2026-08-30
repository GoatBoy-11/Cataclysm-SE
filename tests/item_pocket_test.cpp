#include "catch/catch.hpp"

#include <sstream>
#include <utility>

#include "detached_ptr.h"
#include "item.h"
#include "item_pocket.h"
#include "item_factory.h"
#include "itype.h"
#include "json.h"
#include "options_helpers.h"
#include "relic.h"
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
    CHECK( pockets[0].definition().max_item_length == 5_cm );
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
