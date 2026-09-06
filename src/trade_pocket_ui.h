#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

class item;
class item_pricing;

namespace trade_pocket_ui
{

inline constexpr size_t invalid_pricing_index = static_cast<size_t>( -1 );

struct trade_display_row {
    size_t pricing_index = invalid_pricing_index;
    item *pocket_item = nullptr;
    int indent = 0;

    auto is_pocket_row() const -> bool {
        return pocket_item != nullptr;
    }
};

struct trade_display_tree {
    std::vector<trade_display_row> rows;
    std::vector<std::optional<size_t>> parents;
    std::vector<std::vector<size_t>> children;
    std::vector<bool> collapsed;
};

/**
 * Order trade rows as a pocket tree. Items whose container is also offered for
 * trade are drawn beneath it instead of at their category-sorted position.
 * Pocket contents that are not separate trade lines are added as display rows.
 */
void build_trade_display_tree( trade_display_tree &tree,
                               const std::vector<item_pricing> &list );

auto item_for_row( const trade_display_tree &tree,
                   const std::vector<item_pricing> &list,
                   size_t display_index ) -> item *;

auto row_is_tradeable( const trade_display_row &row ) -> bool;

using trade_filter_fn = std::function<bool( const item & )>;

/** Visible display-row indices after filter and collapse. */
auto build_visible_trade_rows( const trade_display_tree &tree,
                               const std::vector<item_pricing> &list,
                               const trade_filter_fn &filter_fn ) -> std::vector<size_t>;

auto row_has_collapsible_children( const trade_display_tree &tree,
                                   size_t display_index ) -> bool;

void toggle_row_collapse( trade_display_tree &tree, size_t display_index );

/** Clear trade counts on nested rows when their container is selected. */
void deselect_nested_offerings( trade_display_tree &tree,
                                std::vector<item_pricing> &list,
                                size_t pricing_index,
                                bool focus_theirs );

/** Skip transferring contents already inside a container being traded. */
auto skip_because_container_traded( const item &it,
                                    const std::vector<item_pricing> &stuff,
                                    bool npc_gives ) -> bool;

} // namespace trade_pocket_ui
