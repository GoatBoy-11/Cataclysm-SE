#include "pickup_pocket_ui.h"

#include <algorithm>
#include <utility>

#include "game.h"
#include "item.h"
#include "item_contents.h"
#include "item_pocket.h"
#include "map.h"

namespace pickup_pocket_ui
{

item *pickup_stack_entry::front_item() const
{
    if( !ground_iters.empty() ) {
        return *ground_iters.front();
    }
    if( !pocket_items.empty() ) {
        return pocket_items.front();
    }
    return nullptr;
}

size_t pickup_stack_entry::stack_size() const
{
    if( !ground_iters.empty() ) {
        return ground_iters.size();
    }
    return pocket_items.size();
}

bool pickup_stack_entry::is_pocket_row() const
{
    return ground_iters.empty() && !pocket_items.empty();
}

namespace
{

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

} // namespace

void append_pocket_content_rows( std::vector<pickup_stack_entry> &entries,
                                 std::vector<std::optional<size_t>> &parents,
                                 std::vector<std::vector<size_t>> &children,
                                 std::vector<int> &indents,
                                 const size_t container_index,
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
        const size_t child_index = entries.size();
        pickup_stack_entry child_entry;
        child_entry.pocket_items.assign( stack.begin(), stack.end() );
        entries.push_back( std::move( child_entry ) );
        parents.emplace_back( container_index );
        children.emplace_back();
        indents.push_back( std::min( depth, 3 ) );
        children[container_index].push_back( child_index );

        append_pocket_content_rows( entries, parents, children, indents, child_index,
                                    *stack.front(), depth + 1 );
    }
}

void expand_with_pocket_contents( std::vector<pickup_stack_entry> &entries,
                                  std::vector<std::optional<size_t>> &parents,
                                  std::vector<std::vector<size_t>> &children,
                                  std::vector<int> &indents,
                                  const std::vector<std::optional<size_t>> &drop_token_parents )
{
    if( pockets_are_classic() ) {
        return;
    }

    const std::vector<pickup_stack_entry> ground_entries = entries;
    const size_t ground_count = ground_entries.size();

    entries.clear();
    parents.clear();
    children.clear();
    indents.clear();

    std::vector<size_t> ground_to_new( ground_count );
    for( size_t i = 0; i < ground_count; i++ ) {
        const size_t new_index = entries.size();
        ground_to_new[i] = new_index;
        entries.push_back( ground_entries[i] );
        parents.emplace_back( std::nullopt );
        children.emplace_back();
        indents.push_back( 0 );

        // Pocket containment overwrites a drop-token parent when both exist: the
        // token only says these items fell together, but a pocketed item is
        // physically inside its container and belongs under that row.
        if( drop_token_parents[i] ) {
            parents.back() = ground_to_new[*drop_token_parents[i]];
        }

        item *const container = entries.back().front_item();
        if( container != nullptr ) {
            append_pocket_content_rows( entries, parents, children, indents, new_index,
                                        *container, 1 );
        }
    }
}

void apply_initial_collapse( std::vector<bool> &collapsed,
                             const std::vector<std::vector<size_t>> &children )
{
    collapsed.assign( children.size(), false );
    for( size_t i = 0; i < children.size(); i++ ) {
        collapsed[i] = !children[i].empty();
    }
}

bool hidden_by_collapsed_ancestor( const size_t index,
                                   const std::vector<std::optional<size_t>> &parents,
                                   const std::vector<bool> &collapsed )
{
    std::optional<size_t> parent = index < parents.size() ? parents[index] : std::nullopt;
    while( parent ) {
        if( *parent < collapsed.size() && collapsed[*parent] ) {
            return true;
        }
        parent = *parent < parents.size() ? parents[*parent] : std::nullopt;
    }
    return false;
}

bool skip_because_parent_picked( const size_t index,
                                   const std::vector<std::optional<size_t>> &parents,
                                   const std::vector<bool> &picked )
{
    std::optional<size_t> parent = index < parents.size() ? parents[index] : std::nullopt;
    while( parent ) {
        if( *parent < picked.size() && picked[*parent] ) {
            return true;
        }
        parent = *parent < parents.size() ? parents[*parent] : std::nullopt;
    }
    return false;
}

void spill_container_pockets_onto_tile( item &container, const tripoint_bub_ms &pos )
{
    map &here = get_map();
    bool changed = false;

    for( item_pocket &pocket : container.contents.get_pockets() ) {
        if( pocket.definition().type != pocket_type::CONTAINER ) {
            continue;
        }
        for( detached_ptr<item> &it : pocket.clear() ) {
            changed = true;
            here.add_item_or_charges( pos, std::move( it ) );
        }
    }

    if( changed ) {
        container.invalidate_processing_cache_upwards();
    }
}

} // namespace pickup_pocket_ui
