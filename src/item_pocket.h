#pragma once

#include <map>
#include <set>
#include <vector>

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

void record_pocket_audit_miss( const item *container, const item &inserted );
std::string pocket_audit_report();
void clear_pocket_audit();

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

    private:
        item *owner;
        const pocket_data *data;
        location_vector<item> contents;
};
