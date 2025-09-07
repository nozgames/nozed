//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

#include "../tui/tstring.h"

struct TreeNode
{
    TString value;
    int indent_level;
    bool is_expanded;
    bool matches_search;
    bool is_search_parent;
    
    TreeNode* parent;
    std::vector<std::unique_ptr<TreeNode>> children;
    void* user_data = nullptr;
    
    TreeNode(const TString& value, int indent = 0, bool expanded = false)
        : value(value)
        , indent_level(indent)
        , is_expanded(expanded)
        , matches_search(false)
        , is_search_parent(false)
        , parent(nullptr)
    {}
    
    bool has_children() const { return !children.empty(); }
    bool has_user_data() const { return user_data != nullptr; }
    
    TreeNode* AddChild(const TString& value)
    {
        auto child = std::make_unique<TreeNode>(value, indent_level + 1, false);
        child->parent = this;
        TreeNode* ptr = child.get();
        children.push_back(std::move(child));
        return ptr;
    }
    
    void SetUserData(void* data)
    {
        user_data = data;
    }
    
    void* GetUserData() const { return user_data; }
};

class TreeView : public IView
{
protected:

    std::vector<std::unique_ptr<TreeNode>> _root_nodes;
    std::vector<TreeNode*> _visible_nodes;  // Pointers to currently visible nodes
    size_t _max_entries = 1000;
    int _cursor_row = 0;     // Current cursor position in visible list
    int _previous_cursor_row = -1; // Track cursor changes
    bool _show_cursor = false;
    
    // Search functionality
    bool _search_active = false;
    std::string _search_pattern;
    std::regex _search_regex;
    bool _search_regex_valid = false;

    void RebuildVisibleList();
    void CollectVisibleNodes(TreeNode* node, std::vector<TreeNode*>& visible);
    void CollectSearchResults(TreeNode* node, std::vector<TreeNode*>& visible);
    void ToggleExpansion(TreeNode* node);
    bool MatchesSearch(TreeNode* node) const;
    void UpdateSearchFlags();
    void UpdateSearchFlagsRecursive(TreeNode* node);
    int CalculateNodeDistance(TreeNode* from, TreeNode* to) const;
    
public:

    void Add(const TString& name, int indent_level = 0, void* user_data = nullptr);
    
    void Clear();
    size_t NodeCount() const;
    size_t VisibleCount() const;
    void SetMaxEntries(size_t max_entries);
    
    // User data management for nodes
    void SetCurrentNodeUserData(void* data);
    void SetNodeUserData(const std::string& node_path, void* data);
    
    // Cursor and selection
    TreeNode* GetCurrentNode() const;
    bool HasCursorChanged() const;
    void MarkCursorProcessed();
    
    // IView interface
    void Render(const RectInt& rect) override;
    bool HandleKey(int key) override;
    void SetCursorVisible(bool visible) override;
    bool CanPopFromStack() const override { return false; }
    
    // Navigation support
    void ScrollUp(int lines = 1);
    void ScrollDown(int lines = 1);
    void ScrollToTop();
    void ScrollToBottom();
    
    // Cursor support
    void SetCursorPosition(int row, int col);
    
    // Tree-specific operations
    void ExpandAll();
    void CollapseAll();
    void ExpandCurrent();
    void CollapseCurrent();
    
    // IView search interface
    void SetSearchPattern(const std::string& pattern) override;
    void ClearSearch() override;
    bool SupportsSearch() const override;
};