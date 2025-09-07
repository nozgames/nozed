//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "tree_view.h"
#include "../tui/terminal.h"
#include "../tui/screen.h"

static std::string GetArraySizeIndicator(const TreeNode* node)
{
    if (node && node->has_children())
    {
        size_t child_count = node->children.size();
        return "\033[97m[" + std::to_string(child_count) + "]\033[0m"; // Bright white
    }
    return "";
}

void TreeView::Add(const TString& name, int indent_level, void* user_data)
{
    if (indent_level == 0)
    {
        auto root = std::make_unique<TreeNode>(name, 0, false);
        root->SetUserData(user_data);
        _root_nodes.push_back(std::move(root));
    }
    else
    {
        // Find the appropriate parent by looking for the most recent node at indent_level-1
        TreeNode* parent = nullptr;
        
        // Search through all root nodes and their children to find the most recently added parent
        std::function<TreeNode*(const std::vector<std::unique_ptr<TreeNode>>&)> find_parent = 
            [&](const std::vector<std::unique_ptr<TreeNode>>& nodes) -> TreeNode* {
                for (auto it = nodes.rbegin(); it != nodes.rend(); ++it)
                {
                    TreeNode* node = it->get();
                    if (node->indent_level == indent_level - 1)
                        return node;
                    
                    // Search children recursively
                    if (TreeNode* found = find_parent(node->children))
                        return found;
                }
                return nullptr;
            };
        
        parent = find_parent(_root_nodes);
        
        if (parent)
        {
            TreeNode* new_node = parent->AddChild(name);
            new_node->SetUserData(user_data);
        }
        else
        {
            // No parent found, add as root
            auto root = std::make_unique<TreeNode>(name, indent_level, false);
            root->SetUserData(user_data);
            _root_nodes.push_back(std::move(root));
        }
    }
    
    RebuildVisibleList();
    
    // Auto-scroll to bottom
    if (!_visible_nodes.empty())
    {
        _cursor_row = static_cast<int>(_visible_nodes.size()) - 1;
    }
}

void TreeView::SetCurrentNodeUserData(void* data)
{
    // Set user data on the currently selected node
    TreeNode* current_node = GetCurrentNode();
    if (current_node)
    {
        current_node->SetUserData(data);
    }
}

void TreeView::SetNodeUserData(const std::string& node_path, void* data)
{
    // Find node by path and set user data
    std::function<TreeNode*(TreeNode*, const std::string&)> find_by_path = [&](TreeNode* node, const std::string& path) -> TreeNode* {
        if (!node) return nullptr;

        for (auto& child : node->children)
        {
            TreeNode* found = find_by_path(child.get(), path);
            if (found) return found;
        }
        return nullptr;
    };
    
    for (auto& root : _root_nodes)
    {
        TreeNode* target_node = find_by_path(root.get(), node_path);
        if (target_node)
        {
            target_node->SetUserData(data);
            return;
        }
    }
}

TreeNode* TreeView::GetCurrentNode() const
{
    if (!_visible_nodes.empty() && _cursor_row >= 0 && _cursor_row < static_cast<int>(_visible_nodes.size()))
    {
        return _visible_nodes[_cursor_row];
    }
    return nullptr;
}

bool TreeView::HasCursorChanged() const
{
    return _cursor_row != _previous_cursor_row;
}

void TreeView::MarkCursorProcessed()
{
    _previous_cursor_row = _cursor_row;
}

void TreeView::RebuildVisibleList()
{
    // Remember current cursor node if any
    TreeNode* current_cursor_node = nullptr;
    if (!_visible_nodes.empty() && _cursor_row >= 0 && _cursor_row < static_cast<int>(_visible_nodes.size()))
    {
        current_cursor_node = _visible_nodes[_cursor_row];
    }
    
    _visible_nodes.clear();
    
    for (auto& root : _root_nodes)
    {
        if (_search_active && _search_regex_valid)
        {
            CollectSearchResults(root.get(), _visible_nodes);
        }
        else
        {
            CollectVisibleNodes(root.get(), _visible_nodes);
        }
    }
    
    // Try to restore cursor position to the same node, or closest available
    if (current_cursor_node && !_visible_nodes.empty())
    {
        // First try to find the exact same node
        for (size_t i = 0; i < _visible_nodes.size(); i++)
        {
            if (_visible_nodes[i] == current_cursor_node)
            {
                _cursor_row = static_cast<int>(i);
                return;
            }
        }
        
        // If exact node not found, try to find a close ancestor or descendant
        int best_distance = INT_MAX;
        int best_position = 0;
        
        for (size_t i = 0; i < _visible_nodes.size(); i++)
        {
            TreeNode* node = _visible_nodes[i];
            int distance = CalculateNodeDistance(current_cursor_node, node);
            if (distance < best_distance)
            {
                best_distance = distance;
                best_position = static_cast<int>(i);
            }
        }
        
        _cursor_row = best_position;
    }
    else
    {
        // No previous cursor or empty list - reset to top
        _cursor_row = 0;
    }
    
    // Ensure cursor is in valid range
    if (!_visible_nodes.empty())
    {
        _cursor_row = std::max(0, std::min(_cursor_row, static_cast<int>(_visible_nodes.size()) - 1));
    }
}

void TreeView::CollectVisibleNodes(TreeNode* node, std::vector<TreeNode*>& visible)
{
    if (!node) return;
    
    visible.push_back(node);
    
    if (node->is_expanded && node->has_children())
    {
        for (auto& child : node->children)
        {
            CollectVisibleNodes(child.get(), visible);
        }
    }
}

void TreeView::CollectSearchResults(TreeNode* node, std::vector<TreeNode*>& visible)
{
    if (!node) return;
    
    if (node->matches_search)
    {
        // Add this matching node
        visible.push_back(node);
        
        // Add children only if this node is expanded (respecting expansion state even in search)
        if (node->is_expanded && node->has_children())
        {
            for (auto& child : node->children)
            {
                visible.push_back(child.get());
                
                // Recursively add children of expanded nodes
                std::function<void(TreeNode*)> AddChildrenOfExpanded = [&](TreeNode* n) {
                    if (n->is_expanded && n->has_children())
                    {
                        for (auto& grandchild : n->children)
                        {
                            visible.push_back(grandchild.get());
                            AddChildrenOfExpanded(grandchild.get());
                        }
                    }
                };
                AddChildrenOfExpanded(child.get());
            }
        }
    }
    else if (node->is_search_parent)
    {
        // Add this parent node (will be rendered in darker color)
        visible.push_back(node);
        
        // Only add children that are either matches or search parents
        for (auto& child : node->children)
        {
            CollectSearchResults(child.get(), visible);
        }
    }
}

void TreeView::Clear()
{
    _root_nodes.clear();
    _visible_nodes.clear();
    _cursor_row = 0;
}

void TreeView::Render(const RectInt& rect)
{
#if 0
    if (_visible_nodes.empty())
        return;

    auto visible_rows = static_cast<int>(_visible_nodes.size());
    auto max_rows = std::min(rect.height, visible_rows);
        
    _cursor_row = std::max(0, std::min(_cursor_row, visible_rows - 1));
        
    // Calculate the display window
    i32 start_row = 0;
    if (visible_rows > max_rows)
    {
        auto ideal_start_row = _cursor_row - max_rows / 2;
        auto max_start_row = visible_rows - max_rows;
        start_row = std::max(0, std::min(ideal_start_row, max_start_row));
    }

    auto row_count = std::min(max_rows, visible_rows - start_row);
    int cursor_in_window = _cursor_row - start_row;

    for (i32 row = 0; row < row_count; row++)
    {
        auto node = _visible_nodes[start_row + row];

        MoveCursor(0, row);

        // Add indentation
        for (int indent = 0; indent < node->indent_level; indent++)
            AddPixels("  ");

        // Add expansion indicator for nodes with children
        if (node->has_children())
        {
            if (node->is_expanded)
                AddPixel('-');
            else
                AddPixel('+');
        }
        else
            AddPixel(' ');

        AddPixel(' ');
        //AddPixels(node->value.raw.c_str());

        //AddString(node->line_builder.ToString(), cursor_pos, rect.width);

        // if (_search_active && node->is_search_parent && !node->matches_search)
        //     line_builder.Add(node->value.raw, 128, 128, 128);
        // else
        //     line_builder.Add(node->value.formatted);
        //
        // // Add array size indicator for objects with children
        // std::string size_indicator = GetArraySizeIndicator(node);
        // if (!size_indicator.empty())
        // {
        //     line_builder.Add(" " + size_indicator);
        // }
        //
        // int cursor_pos = (_show_cursor && static_cast<int>(row) == cursor_in_window) ? 0 : -1;
        // AddString(line_builder.ToString(), cursor_pos, rect.width);
    }
#endif
}

size_t TreeView::NodeCount() const
{
    size_t count = 0;
    std::function<void(TreeNode*)> count_nodes = [&](TreeNode* node) {
        if (!node) return;
        count++;
        for (auto& child : node->children)
        {
            count_nodes(child.get());
        }
    };
    
    for (auto& root : _root_nodes)
    {
        count_nodes(root.get());
    }
    
    return count;
}

size_t TreeView::VisibleCount() const
{
    return _visible_nodes.size();
}

void TreeView::SetMaxEntries(size_t max_entries)
{
    _max_entries = max_entries;
    RebuildVisibleList();
}

bool TreeView::HandleKey(int key)
{
    switch (key)
    {
        case KEY_UP:
            if (_cursor_row > 0)
            {
                _cursor_row--;
            }
            return true;
            
        case KEY_DOWN:
            if (!_visible_nodes.empty() && _cursor_row < static_cast<int>(_visible_nodes.size()) - 1)
            {
                _cursor_row++;
            }
            return true;
            
        case KEY_PPAGE:  // Page Up
            _cursor_row = std::max(0, _cursor_row - 10);
            return true;
            
        case KEY_NPAGE:  // Page Down
            if (!_visible_nodes.empty())
            {
                _cursor_row = std::min(_cursor_row + 10, static_cast<int>(_visible_nodes.size()) - 1);
            }
            return true;
            
        case KEY_HOME:
            _cursor_row = 0;
            return true;
            
        case KEY_END:
            if (!_visible_nodes.empty())
            {
                _cursor_row = static_cast<int>(_visible_nodes.size()) - 1;
            }
            return true;
            
        case KEY_RIGHT:
        case ' ':  // Space also expands
            // Expand current node
            if (!_visible_nodes.empty() && _cursor_row >= 0 && _cursor_row < static_cast<int>(_visible_nodes.size()))
            {
                TreeNode* node = _visible_nodes[_cursor_row];
                ToggleExpansion(node);
            }
            return true;
            
        case KEY_LEFT:
            // Collapse current node or move to parent
            if (!_visible_nodes.empty() && _cursor_row >= 0 && _cursor_row < static_cast<int>(_visible_nodes.size()))
            {
                TreeNode* node = _visible_nodes[_cursor_row];
                
                if (node->has_children() && node->is_expanded)
                {
                    // Collapse current node
                    ToggleExpansion(node);
                }
                else if (node->parent)
                {
                    // Move to parent
                    auto it = std::find(_visible_nodes.begin(), _visible_nodes.end(), node->parent);
                    if (it != _visible_nodes.end())
                    {
                        _cursor_row = static_cast<int>(std::distance(_visible_nodes.begin(), it));
                    }
                }
            }
            return true;
            
        default:
            return false;
    }
}

void TreeView::ScrollUp(int lines)
{
    _cursor_row = std::max(0, _cursor_row - lines);
}

void TreeView::ScrollDown(int lines)
{
    if (!_visible_nodes.empty())
    {
        _cursor_row = std::min(_cursor_row + lines, static_cast<int>(_visible_nodes.size()) - 1);
    }
}

void TreeView::ScrollToTop()
{
    _cursor_row = 0;
}

void TreeView::ScrollToBottom()
{
    if (!_visible_nodes.empty())
    {
        _cursor_row = static_cast<int>(_visible_nodes.size()) - 1;
    }
}

void TreeView::SetCursorVisible(bool visible)
{
    _show_cursor = visible;
}

void TreeView::SetCursorPosition(int row, int col)
{
    _cursor_row = row;
}

void TreeView::ExpandAll()
{
    std::function<void(TreeNode*)> expand_recursive = [&](TreeNode* node) {
        if (!node) return;
        if (node->has_children())
        {
            node->is_expanded = true;
        }
        for (auto& child : node->children)
        {
            expand_recursive(child.get());
        }
    };
    
    for (auto& root : _root_nodes)
    {
        expand_recursive(root.get());
    }
    RebuildVisibleList();
}

void TreeView::CollapseAll()
{
    std::function<void(TreeNode*)> collapse_recursive = [&](TreeNode* node) {
        if (!node) return;
        if (node->has_children())
        {
            node->is_expanded = false;
        }
        for (auto& child : node->children)
        {
            collapse_recursive(child.get());
        }
    };
    
    for (auto& root : _root_nodes)
    {
        collapse_recursive(root.get());
    }
    RebuildVisibleList();
}

void TreeView::ExpandCurrent()
{
    if (!_visible_nodes.empty() && _cursor_row >= 0 && _cursor_row < static_cast<int>(_visible_nodes.size()))
    {
        TreeNode* node = _visible_nodes[_cursor_row];
        if (node->has_children())
        {
            node->is_expanded = true;
            RebuildVisibleList();
        }
    }
}

void TreeView::CollapseCurrent()
{
    if (!_visible_nodes.empty() && _cursor_row >= 0 && _cursor_row < static_cast<int>(_visible_nodes.size()))
    {
        TreeNode* node = _visible_nodes[_cursor_row];
        if (node->has_children())
        {
            node->is_expanded = false;
            RebuildVisibleList();
        }
    }
}

void TreeView::ToggleExpansion(TreeNode* node)
{
    if (node && node->has_children())
    {
        node->is_expanded = !node->is_expanded;
        RebuildVisibleList();
    }
}

bool TreeView::MatchesSearch(TreeNode* node) const
{
    if (!_search_regex_valid || !node)
        return false;
    
    try
    {
        return false; // return std::regex_search(node->value.raw, _search_regex);
    }
    catch (const std::exception&)
    {
        return false;
    }
}

void TreeView::SetSearchPattern(const std::string& pattern)
{
    _search_pattern = pattern;
    _search_regex_valid = false;
    
    if (!pattern.empty())
    {
        _search_active = true;
        try
        {
            _search_regex = std::regex(pattern, std::regex_constants::icase);
            _search_regex_valid = true;
        }
        catch (const std::exception&)
        {
            // Invalid regex, keep _search_regex_valid as false
        }
    }
    else
    {
        _search_active = false;
    }
    
    UpdateSearchFlags();
    RebuildVisibleList();
    
    // Position cursor on first matching result to ensure it's visible
    if (_search_active && _search_regex_valid && !_visible_nodes.empty())
    {
        // Find first matching node in visible list
        for (size_t i = 0; i < _visible_nodes.size(); i++)
        {
            if (_visible_nodes[i]->matches_search)
            {
                _cursor_row = static_cast<int>(i);
                break;
            }
        }
    }
    else
    {
        _cursor_row = 0; // Reset cursor to top when search cleared
    }
}

void TreeView::ClearSearch()
{
    _search_active = false;
    _search_pattern.clear();
    _search_regex_valid = false;
    UpdateSearchFlags();
    RebuildVisibleList(); // This will automatically preserve cursor position
}

bool TreeView::SupportsSearch() const
{
    return true;
}

void TreeView::UpdateSearchFlags()
{
    for (auto& root : _root_nodes)
    {
        UpdateSearchFlagsRecursive(root.get());
    }
}

void TreeView::UpdateSearchFlagsRecursive(TreeNode* node)
{
#if 0
    assert(node);
    
    node->matches_search = false;
    node->is_search_parent = false;
    
    if (_search_active && _search_regex_valid)
    {
        try
        {
            if (std::regex_search(node->value.raw, _search_regex))
            {
                node->matches_search = true;

                for (TreeNode* parent = node->parent; parent; parent = parent->parent)
                {
                    parent->is_search_parent = true;
                    parent->is_expanded = true;
                }
            }
        }
        catch (const std::exception&)
        {
        }
    }
    
    for (auto& child : node->children)
        UpdateSearchFlagsRecursive(child.get());
#endif
}

int TreeView::CalculateNodeDistance(TreeNode* from, TreeNode* to) const
{
    if (!from || !to) return INT_MAX;
    if (from == to) return 0;
    
    // Check if one is an ancestor of the other
    TreeNode* ancestor = from->parent;
    int distance = 1;
    while (ancestor)
    {
        if (ancestor == to) return distance;
        ancestor = ancestor->parent;
        distance++;
    }
    
    ancestor = to->parent;
    distance = 1;
    while (ancestor)
    {
        if (ancestor == from) return distance;
        ancestor = ancestor->parent;
        distance++;
    }
    
    // Find common ancestor and calculate distance through it
    std::vector<TreeNode*> from_path;
    std::vector<TreeNode*> to_path;
    
    TreeNode* node = from;
    while (node)
    {
        from_path.push_back(node);
        node = node->parent;
    }
    
    node = to;
    while (node)
    {
        to_path.push_back(node);
        node = node->parent;
    }
    
    // Find common ancestor
    int common_depth = 0;
    while (common_depth < static_cast<int>(from_path.size()) && 
           common_depth < static_cast<int>(to_path.size()) &&
           from_path[from_path.size() - 1 - common_depth] == to_path[to_path.size() - 1 - common_depth])
    {
        common_depth++;
    }
    
    if (common_depth == 0) return INT_MAX; // No common ancestor
    
    // Distance is sum of distances to common ancestor
    return static_cast<int>(from_path.size()) + static_cast<int>(to_path.size()) - 2 * common_depth;
}