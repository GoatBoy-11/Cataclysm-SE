#include "loot_pocket_nesting.h"

#include <utility>
#include <vector>

#include "cata_utility.h"
#include "flag.h"
#include "item.h"
#include "item_pocket.h"
#include "rng.h"

namespace loot_pocket_nesting
{

namespace
{

struct pocket_match {
    item *host = nullptr;
    item_pocket *pocket = nullptr;
    std::pair<bool, bool> rank{ false, false };
    int priority = 0;
    units::volume remaining = 0_ml;
};

auto has_container_pockets( const item &it ) -> bool
{
    for( const item_pocket &pocket : it.contents.get_pockets() ) {
        if( pocket.definition().type == pocket_type::CONTAINER ) {
            return true;
        }
    }
    return false;
}

auto pocket_rank( const item_pocket &pocket ) -> std::pair<bool, bool>
{
    const pocket_data &def = pocket.definition();
    const bool restricted = !def.ammo_restriction.empty() ||
                            !def.item_restriction.empty() ||
                            !def.mod_restriction.empty();
    return { restricted, restricted };
}

auto best_container_pocket( item &host, const item &nestee ) -> item_pocket *
{
    item_pocket *best = nullptr;
    int best_priority = 0;
    std::pair<bool, bool> best_rank{ false, false };

    for( item_pocket &pocket : host.contents.get_pockets() ) {
        if( pocket.definition().type != pocket_type::CONTAINER ) {
            continue;
        }
        if( !pocket.can_contain( nestee ).success() ) {
            continue;
        }

        const int priority = pocket.get_settings().priority();
        const std::pair<bool, bool> rank = pocket_rank( pocket );

        if( best == nullptr || priority > best_priority ||
            ( priority == best_priority &&
              ( rank > best_rank ||
                ( rank == best_rank &&
                  pocket.remaining_volume() < best->remaining_volume() ) ) ) ) {
            best = &pocket;
            best_priority = priority;
            best_rank = rank;
        }
    }
    return best;
}

auto find_best_host( const std::vector<detached_ptr<item>> &items, const size_t nestee_index,
                     const item &nestee ) -> pocket_match
{
    auto best = pocket_match{};
    for( size_t host_index = 0; host_index < items.size(); host_index++ ) {
        if( host_index == nestee_index || !items[host_index] ) {
            continue;
        }
        item &host = *items[host_index];
        if( !has_container_pockets( host ) ) {
            continue;
        }
        item_pocket *const pocket = best_container_pocket( host, nestee );
        if( pocket == nullptr ) {
            continue;
        }
        const int priority = pocket->get_settings().priority();
        const std::pair<bool, bool> rank = pocket_rank( *pocket );
        if( best.pocket == nullptr || priority > best.priority ||
            ( priority == best.priority &&
              ( rank > best.rank ||
                ( rank == best.rank &&
                  pocket->remaining_volume() < best.remaining ) ) ) ) {
            best = pocket_match{
                .host = &host,
                .pocket = pocket,
                .rank = rank,
                .priority = priority,
                .remaining = pocket->remaining_volume()
            };
        }
    }
    return best;
}

auto should_attempt_nest( const item &it ) -> bool
{
    if( it.has_flag( flag_MISSION_ITEM ) ) {
        return false;
    }
    if( it.is_armor() ) {
        return false;
    }
    return true;
}

auto roll_nest_chance( const nest_chance &chance ) -> bool
{
    if( chance.numerator <= 0 || chance.denominator <= 0 ) {
        return false;
    }
    if( chance.numerator >= chance.denominator ) {
        return true;
    }
    return one_in( chance.denominator / chance.numerator );
}

} // namespace

void nest_spawned_loot_in_containers( std::vector<detached_ptr<item>> &items,
                                      const nest_chance chance )
{
    if( pockets_are_classic() || items.size() < 2 ) {
        return;
    }

    for( size_t nestee_index = 0; nestee_index < items.size(); ) {
        if( !items[nestee_index] || !should_attempt_nest( *items[nestee_index] ) ) {
            nestee_index++;
            continue;
        }
        if( !roll_nest_chance( chance ) ) {
            nestee_index++;
            continue;
        }

        const pocket_match match = find_best_host( items, nestee_index, *items[nestee_index] );
        if( match.pocket == nullptr || match.host == nullptr ) {
            nestee_index++;
            continue;
        }

        match.pocket->insert( std::move( items[nestee_index] ) );
        match.host->invalidate_processing_cache_upwards();
        items.erase( items.begin() + static_cast<std::ptrdiff_t>( nestee_index ) );
    }
}

} // namespace loot_pocket_nesting
