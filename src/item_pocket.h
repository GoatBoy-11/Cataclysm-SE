#pragma once

#include <vector>

#include "location_vector.h"
#include "ret_val.h"
#include "units.h"

class item;
class JsonIn;
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
};

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
            ERR_NO_SPACE
        };

        item_pocket( item *owner, const pocket_data *data );

        bool empty() const;
        const std::vector<item *> &all_items_top() const;

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
        const pocket_data *data;
        location_vector<item> contents;
};
