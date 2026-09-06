#pragma once

#include <vector>

template<typename T>
class detached_ptr;

class item;

namespace loot_pocket_nesting
{

struct nest_chance {
    int numerator = 1;
    int denominator = 3;
};

/**
 * After a loot batch is rolled, tuck some loose items into container pockets
 * already present in the same batch. No-op in classic pocket mode.
 */
void nest_spawned_loot_in_containers( std::vector<detached_ptr<item>> &items,
                                     nest_chance chance = {} );

} // namespace loot_pocket_nesting
