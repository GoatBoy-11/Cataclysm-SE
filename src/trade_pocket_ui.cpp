#include "trade_pocket_ui.h"

#include <algorithm>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "item.h"
#include "item_pocket.h"
#include "npctrade.h"
#include "pickup_pocket_ui.h"

namespace trade_pocket_ui
{

namespace
{

auto find_pricing_for_parent_chain(
    const item *it,
    const std::unordered_map<const item *, size_t> &item_to_pricing ) -> std::optional<size_t>
{
    if( it == nullptr ) {
        return std::nullopt;
    }
    for( const item *parent = it->parent_item(); parent != nullptr;
         parent = parent->parent_item() ) {
        const auto found = item_to_pricing.find( parent );
        if( found != item_to_pricing.end() ) {
            return found->second;
        }
    }
    return std::nullopt;
}

void append_display_row( trade_display_tree &tree,
                         trade_display_row row,
                         const std::optional<size_t> parent_display )
{
    const size_t display_index = tree.rows.size();
    tree.rows.push_back( std::move( row ) );
    tree.parents.push_back( parent_display );
    tree.children.emplace_back();
    if( parent_display && *parent_display < tree.children.size() ) {
        tree.children[*parent_display].push_back( display_index );
    }
}

void emit_subtree( const size_t pricing_index,
                   const int indent,
                   const std::vector<std::vector<size_t>> &pricing_children,
                   trade_display_tree &tree,
                   const std::optional<size_t> parent_display )
{
    append_display_row( tree, trade_display_row{
        .pricing_index = pricing_index,
        .pocket_item = nullptr,
        .indent = std::min( indent, 3 )
    }, parent_display );
    const size_t display_index = tree.rows.size() - 1;
    for( const size_t child_pricing_index : pricing_children[pricing_index] ) {
        emit_subtree( child_pricing_index, indent + 1, pricing_children, tree, display_index );
    }
}

std::vector<std::list<item *>> restack_pocket_items( const std::vector<item *> &items )
{
    std::vector<std::list<item *>> res;
    for( item * const it : items ) {
        auto match = std::find_if( res.begin(), res.end(),
        [it]( const std::list<item *> &stack ) {
            return it->display_stacked_with( *stack.back() );
        } );
        if( match != res.end() ) {
            match->push_back( it );
        } else {
            res.emplace_back( 1, it );
        }
    }
    return res;
}

void gather_container_pocket_items( item &container, std::vector<item *> &stored )
{
    for( item_pocket &pocket : container.contents.get_pockets() ) {
        if( pocket.definition().type != pocket_type::CONTAINER ) {
            continue;
        }
        const std::vector<item *> &top = pocket.all_items_top();
        stored.insert( stored.end(), top.begin(), top.end() );
    }
}

void append_unlisted_pocket_contents( trade_display_tree &tree,
                                      const std::unordered_set<const item *> &listed_items,
                                      const size_t parent_display,
                                      item &container,
                                      const int depth )
{
    if( pockets_are_classic() ) {
        return;
    }

    std::vector<item *> stored;
    gather_container_pocket_items( container, stored );
    if( stored.empty() ) {
        return;
    }

    for( const std::list<item *> &stack : restack_pocket_items( stored ) ) {
        item *const child = stack.front();
        if( listed_items.contains( child ) ) {
            continue;
        }
        const size_t child_display = tree.rows.size();
        append_display_row( tree, trade_display_row{
            .pricing_index = invalid_pricing_index,
            .pocket_item = child,
            .indent = std::min( depth, 3 )
        }, parent_display );
        append_unlisted_pocket_contents( tree, listed_items, child_display, *child, depth + 1 );
    }
}

void clear_trade_amount( item_pricing &ip, const bool focus_theirs )
{
    if( ip.charges >  0 ) {
        if( focus_theirs ) {
            ip.u_charges = 0;
        } else {
            ip.npc_charges = 0;
        }
    } else if( focus_theirs ) {
        ip.u_has = 0;
    } else {
        ip.npc_has = 0;
    }
    ip.selected = false;
}

} // namespace

void build_trade_display_tree( trade_display_tree &tree,
                               const std::vector<item_pricing> &list )
{
    tree = trade_display_tree{};
    if( list.empty() ) {
        return;
    }

    if( pockets_are_classic() ) {
        tree.rows.reserve( list.size() );
        tree.parents.reserve( list.size() );
        tree.children.reserve( list.size() );
        for( size_t i = 0; i < list.size(); i++ ) {
            tree.rows.push_back( trade_display_row{ .pricing_index = i, .indent = 0 } );
            tree.parents.emplace_back( std::nullopt );
            tree.children.emplace_back();
        }
        pickup_pocket_ui::apply_initial_collapse( tree.collapsed, tree.children );
        return;
    }

    std::unordered_map<const item *, size_t> item_to_pricing;
    item_to_pricing.reserve( list.size() );
    for( size_t i = 0; i < list.size(); i++ ) {
        if( list[i].locs.empty() ) {
            continue;
        }
        item_to_pricing[list[i].locs.front()] = i;
    }

    auto pricing_children = std::vector<std::vector<size_t>>( list.size() );
    auto pricing_parent = std::vector<std::optional<size_t>>( list.size() );
    for( size_t i = 0; i < list.size(); i++ ) {
        if( list[i].locs.empty() ) {
            continue;
        }
        const item *const it = list[i].locs.front();
        const std::optional<size_t> parent_pricing = find_pricing_for_parent_chain( it,
                item_to_pricing );
        if( parent_pricing && *parent_pricing != i ) {
            pricing_parent[i] = parent_pricing;
            pricing_children[*parent_pricing].push_back( i );
        }
    }

    for( size_t i = 0; i < list.size(); i++ ) {
        if( !pricing_parent[i] ) {
            emit_subtree( i, 0, pricing_children, tree, std::nullopt );
        }
    }

    std::unordered_set<const item *> listed_items;
    listed_items.reserve( list.size() );
    for( const item_pricing &ip : list ) {
        if( !ip.locs.empty() ) {
            listed_items.insert( ip.locs.front() );
        }
    }

    const size_t priced_row_count = tree.rows.size();
    for( size_t display_index = 0; display_index < priced_row_count; display_index++ ) {
        const trade_display_row &row = tree.rows[display_index];
        if( row.is_pocket_row() || row.pricing_index >= list.size() ) {
            continue;
        }
        item *const container = list[row.pricing_index].locs.front();
        append_unlisted_pocket_contents( tree, listed_items, display_index, *container,
                                         row.indent + 1 );
    }

    pickup_pocket_ui::apply_initial_collapse( tree.collapsed, tree.children );
}

auto item_for_row( const trade_display_tree &tree,
                   const std::vector<item_pricing> &list,
                   const size_t display_index ) -> item *
{
    if( display_index >= tree.rows.size() ) {
        return nullptr;
    }
    const trade_display_row &row = tree.rows[display_index];
    if( row.is_pocket_row() ) {
        return row.pocket_item;
    }
    if( row.pricing_index >= list.size() || list[row.pricing_index].locs.empty() ) {
        return nullptr;
    }
    return list[row.pricing_index].locs.front();
}

auto row_is_tradeable( const trade_display_row &row ) -> bool
{
    return !row.is_pocket_row();
}

auto build_visible_trade_rows( const trade_display_tree &tree,
                               const std::vector<item_pricing> &list,
                               const trade_filter_fn &filter_fn ) -> std::vector<size_t>
{
    auto visible = std::vector<size_t> {};
    visible.reserve( tree.rows.size() );
    for( size_t display_index = 0; display_index < tree.rows.size(); display_index++ ) {
        if( pickup_pocket_ui::hidden_by_collapsed_ancestor( display_index, tree.parents,
                tree.collapsed ) ) {
            continue;
        }
        item *const it = item_for_row( tree, list, display_index );
        if( it == nullptr ) {
            continue;
        }
        if( filter_fn && !filter_fn( *it ) ) {
            continue;
        }
        visible.push_back( display_index );
    }
    return visible;
}

auto row_has_collapsible_children( const trade_display_tree &tree,
                                   const size_t display_index ) -> bool
{
    return display_index < tree.children.size() && !tree.children[display_index].empty();
}

void toggle_row_collapse( trade_display_tree &tree, const size_t display_index )
{
    if( display_index >= tree.collapsed.size() || !row_has_collapsible_children( tree,
            display_index ) ) {
        return;
    }
    tree.collapsed[display_index] = !tree.collapsed[display_index];
}

void deselect_nested_offerings( trade_display_tree &tree,
                                std::vector<item_pricing> &list,
                                const size_t pricing_index,
                                const bool focus_theirs )
{
    if( pricing_index >= list.size() ) {
        return;
    }
    const auto deselect_subtree = [&]( const auto & self, const size_t display_index ) -> void {
        for( const size_t child_display : tree.children[display_index] ) {
            const trade_display_row &child = tree.rows[child_display];
            if( row_is_tradeable( child ) && child.pricing_index < list.size() ) {
                clear_trade_amount( list[child.pricing_index], focus_theirs );
            }
            self( self, child_display );
        }
    };

    for( size_t display_index = 0; display_index < tree.rows.size(); display_index++ ) {
        if( tree.rows[display_index].pricing_index != pricing_index ) {
            continue;
        }
        deselect_subtree( deselect_subtree, display_index );
    }
}

auto skip_because_container_traded( const item &it,
                                    const std::vector<item_pricing> &stuff,
                                    const bool npc_gives ) -> bool
{
    for( const item *parent = it.parent_item(); parent != nullptr;
         parent = parent->parent_item() ) {
        for( const item_pricing &ip : stuff ) {
            if( !ip.selected || ip.locs.empty() ) {
                continue;
            }
            const bool parent_listed = std::ranges::any_of( ip.locs,
            [parent]( item * loc ) {
                return loc == parent;
            } );
            if( !parent_listed ) {
                continue;
            }
            const int amount = ip.charges > 0 ?
                               ( npc_gives ? ip.u_charges : ip.npc_charges ) :
                               ( npc_gives ? ip.u_has : ip.npc_has );
            if( amount > 0 ) {
                return true;
            }
        }
    }
    return false;
}

} // namespace trade_pocket_ui
