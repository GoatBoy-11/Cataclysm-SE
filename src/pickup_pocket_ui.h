#pragma once

#include <cstddef>
#include <list>
#include <optional>
#include <vector>

#include "coordinates.h"
#include "item_stack.h"

class item;

namespace pickup_pocket_ui
{

struct pickup_stack_entry {
    std::list<item_stack::iterator> ground_iters;
    std::vector<item *> pocket_items;

    item *front_item() const;
    size_t stack_size() const;
    bool is_pocket_row() const;
};

/**
 * Append pocket contents as child rows beneath @p container_index.
 * Recurses into nested containers. Does nothing in classic pocket mode.
 */
void append_pocket_content_rows( std::vector<pickup_stack_entry> &entries,
                                 std::vector<std::optional<size_t>> &parents,
                                 std::vector<std::vector<size_t>> &children,
                                 std::vector<int> &indents,
                                 size_t container_index,
                                 item &container,
                                 int depth );

/**
 * Expand a ground-only pickup list with pocket content rows and parent links.
 * Drop-token parents from @p drop_token_parents are remapped to the new indices
 * first; pocket containment then overwrites parent where both apply, because the
 * item is physically inside the container rather than merely sharing a drop token.
 */
void expand_with_pocket_contents( std::vector<pickup_stack_entry> &entries,
                                  std::vector<std::optional<size_t>> &parents,
                                  std::vector<std::vector<size_t>> &children,
                                  std::vector<int> &indents,
                                  const std::vector<std::optional<size_t>> &drop_token_parents );

void apply_initial_collapse( std::vector<bool> &collapsed,
                             const std::vector<std::vector<size_t>> &children );

bool hidden_by_collapsed_ancestor( size_t index,
                                   const std::vector<std::optional<size_t>> &parents,
                                   const std::vector<bool> &collapsed );

bool skip_because_parent_picked( size_t index,
                                   const std::vector<std::optional<size_t>> &parents,
                                   const std::vector<bool> &picked );

void spill_container_pockets_onto_tile( item &container, const tripoint_bub_ms &pos );

} // namespace pickup_pocket_ui
