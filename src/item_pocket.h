#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "flat_set.h"

#include "enum_traits.h"
#include "location_vector.h"
#include "ret_val.h"
#include "units.h"

class item;
class JsonIn;
class JsonObject;
class JsonOut;

enum class pocket_type {
    CONTAINER,
    MAGAZINE,
    MAGAZINE_WELL,
    MOD,
    /** Spent casings a gun keeps: RELOAD_EJECT guns and brass catchers. */
    CASINGS,
    CORPSE,
    MIGRATION,
    LAST
};

template<>
struct enum_traits<pocket_type> {
    static constexpr pocket_type last = pocket_type::LAST;
};

/**
 * Immutable, shared definition of one pocket. Lives on itype.
 * Field names match CDDA's JSON schema; do not rename.
 */
struct pocket_data {
    pocket_type type = pocket_type::CONTAINER;
    units::volume max_contains_volume = 0_ml;
    /** zero means unbounded */
    units::mass max_contains_weight = 0_gram;
    /** zero means unbounded */
    units::length max_item_length = 0_mm;
    bool rigid = false;
    bool watertight = false;
    bool sealed = false;
    float spoil_multiplier = 1.0f;
    int moves = 100;

    /**
     * Which ammo may go in, and how many charges of it. Empty means the pocket
     * is not ammo-restricted at all; non-empty means *only* these are accepted.
     * Schema matches CDDA's: { "9mm": 17 }.
     */
    std::map<ammotype, int> ammo_restriction;

    /**
     * Which specific items may go in. Empty means unrestricted; non-empty means
     * *only* these. Used for magazine wells, which accept a known set of
     * magazines and nothing else. Schema matches CDDA's.
     */
    std::set<itype_id> item_restriction;

    /**
     * Accept only items carrying at least one of these flags. Empty means no
     * flag rule. CDDA's schema; values are validated against flags CSE actually
     * uses at import time, because a restriction naming a flag no item carries
     * would refuse everything.
     */
    std::set<std::string> flag_restriction;

    /** A holster holds exactly one item at a time. */
    bool holster = false;

    /**
     * Which gunmod locations this pocket accepts, and how many of each, keyed by
     * the location id (`gunmod_location::str()`).
     *
     * CDDA expresses mod restrictions with flag_restriction, which cannot carry
     * a per-location count. BN's valid_mod_locations can, so this is a superset
     * of CDDA's schema rather than a rename of it: CDDA content still loads, and
     * BN's richer mod data survives.
     *
     * Keyed by string rather than gunmod_location because that class lives in
     * itype.h, which already includes this header; keying by the id avoids
     * restructuring an upstream-shared header for no gain, and loses nothing
     * since gunmod_location is itself only a string.
     */
    std::map<std::string, int> mod_restriction;

    /**
     * True when this pocket was invented by legacy synthesis rather than authored
     * in JSON. Reported by the pocket coverage listing; never serialized.
     */
    bool synthesized = false;

    void load( const JsonObject &jo );
    void deserialize( JsonIn &jsin );
};

/**
 * Dry-run enforcement audit.
 *
 * Phase 1 removed the can_contain() gate from insertion because synthesis could
 * not yet supply a pocket for everything an item may hold. Rather than guess
 * when it is safe to put the gate back, every insertion that *would* have been
 * rejected is recorded here while still being allowed through. An empty report
 * after exercising the game is the evidence that enforcement can be enabled.
 */

/**
 * True when the world is running the classic pocket system.
 *
 * Classic keeps every item's full pocket set - synthesis is identical in both
 * modes - and relaxes behaviour instead: can_contain() checks only volume and
 * weight, and best_pocket() degenerates to first-fit. Keeping the pockets means
 * save data stays byte-identical between modes, so a character made in one
 * opens in the other gaining or losing restrictions rather than corrupting.
 */
bool pockets_are_classic();

/** True when the player has asked to pick a pocket for each item picked up. */
bool pockets_prompt_on_pickup();

void record_pocket_audit_miss( const item *container, const item &inserted );
std::string pocket_audit_report();
void clear_pocket_audit();

/**
 * Per-pocket player preferences: which pocket things should go into, and what
 * each pocket will accept. Stored per item instance, not per item type, so two
 * backpacks can be organised differently.
 *
 * Ported from CDDA's item_pocket::favorite_settings.
 */
class pocket_favorite_settings
{
    public:
        void clear();

        /** Higher wins when best_pocket() chooses. */
        void set_priority( int p ) {
            priority_rating = p;
            player_edited = true;
        }
        int priority() const {
            return priority_rating;
        }

        void whitelist_item( const itype_id &id );
        void blacklist_item( const itype_id &id );
        void clear_item( const itype_id &id );
        void whitelist_category( const item_category_id &id );
        void blacklist_category( const item_category_id &id );
        void clear_category( const item_category_id &id );

        const cata::flat_set<itype_id> &get_item_whitelist() const {
            return item_whitelist;
        }
        const cata::flat_set<itype_id> &get_item_blacklist() const {
            return item_blacklist;
        }
        const cata::flat_set<item_category_id> &get_category_whitelist() const {
            return category_whitelist;
        }
        const cata::flat_set<item_category_id> &get_category_blacklist() const {
            return category_blacklist;
        }

        /** Whether an item passes these filters. Precedence follows CDDA's. */
        bool accepts_item( const item &it ) const;

        bool is_collapsed() const {
            return collapsed;
        }
        void set_collapse( bool flag ) {
            collapsed = flag;
            player_edited = true;
        }
        bool is_disabled() const {
            return disabled;
        }
        void set_disabled( bool flag ) {
            disabled = flag;
            player_edited = true;
        }
        bool is_unloadable() const {
            return unload;
        }
        void set_unloadable( bool flag ) {
            unload = flag;
            player_edited = true;
        }

        /**
         * True while the player has never touched these settings. Serialization
         * skips null settings entirely: most pockets on most items are never
         * edited, and writing an empty object for each would bloat every save.
         */
        bool is_null() const;

        /** The preset these settings came from, if the player applied one. */
        const std::optional<std::string> &get_preset_name() const {
            return preset_name;
        }
        void set_preset_name( const std::string &name ) {
            preset_name = name;
            player_edited = true;
        }

        void serialize( JsonOut &json ) const;
        void deserialize( JsonIn &jsin );

    private:
        std::optional<std::string> preset_name;
        int priority_rating = 0;
        cata::flat_set<itype_id> item_whitelist;
        cata::flat_set<itype_id> item_blacklist;
        cata::flat_set<item_category_id> category_whitelist;
        cata::flat_set<item_category_id> category_blacklist;
        bool collapsed = false;
        bool disabled = false;
        bool unload = true;
        bool player_edited = false;
};

/**
 * Named pocket settings the player can reuse.
 *
 * These live in the config directory rather than in a save, exactly as CDDA's
 * do, so a rule worked out once follows the player into every world.
 */
namespace pocket_presets
{
/** Reads the file. Called once at startup; harmless to call again. */
void load();
const std::vector<pocket_favorite_settings> &all();
/** Stores under the settings' own preset name, replacing any of that name. */
void add( const pocket_favorite_settings &preset );
void remove( const std::string &name );
const pocket_favorite_settings *find( const std::string &name );
} // namespace pocket_presets

/**
 * One compartment of an item. Owns its contents through the same
 * contents_item_location the owning item uses, so item ownership and
 * location tracking are unchanged by pockets.
 */
class item_pocket
{
    public:
        enum class contain_code {
            SUCCESS,
            ERR_TOO_BIG,
            ERR_TOO_HEAVY,
            ERR_NO_SPACE,
            /** wrong ammo type, or more charges than the pocket holds */
            ERR_AMMO,
            /** not one of the specific items this pocket accepts */
            ERR_ITEM,
            /** not a gunmod, wrong mod location, or that location is full */
            ERR_MOD
        };

        item_pocket( item *owner, const pocket_data *data );
        /**
         * Needed so pockets can live in a std::vector. location_vector has no
         * move constructor, and its move assignment dereferences the target's
         * location, so a fresh contents_item_location must exist beforehand.
         */
        item_pocket( item_pocket &&other ) noexcept;
        item_pocket( const item_pocket & ) = delete;
        item_pocket &operator=( const item_pocket & ) = delete;
        item_pocket &operator=( item_pocket && ) = delete;

        bool empty() const;
        const std::vector<item *> &all_items_top() const;

        /**
         * One display line per item held, for the organizer. No inventory screen
         * in CSE renders pockets, so the organizer is the only place the player
         * can see which compartment a rule actually put something in.
         */
        std::vector<std::string> contents_rows() const;

        /**
         * Whether this pocket can hold anything at all. A CONTAINER pocket that
         * declares no volume holds nothing, and describing it only tells the
         * player about storage they do not have. Ammo, item and mod pockets are
         * bounded by their restrictions rather than by volume, so they always
         * count as usable.
         */
        bool can_hold_anything() const;

        /** direct access for the visitable machinery, which needs the vector itself */
        location_vector<item> &get_contents() {
            return contents;
        }
        const location_vector<item> &get_contents() const {
            return contents;
        }

        units::volume contents_volume() const;
        units::volume remaining_volume() const;
        units::mass contents_weight() const;

        ret_val<contain_code> can_contain( const item &it ) const;
        void insert( detached_ptr<item> &&it );
        detached_ptr<item> remove( item *it );

        std::vector<detached_ptr<item>> clear();
        void on_destroy();

        const pocket_data &definition() const {
            return *data;
        }

        /**
         * Player preferences for this pocket. Per item instance, so the same
         * itype's pockets can be organised differently on two copies of an item.
         */
        pocket_favorite_settings &get_settings() {
            return settings;
        }
        const pocket_favorite_settings &get_settings() const {
            return settings;
        }

    private:
        item *owner;
        const pocket_data *data;
        location_vector<item> contents;
        pocket_favorite_settings settings;
};
