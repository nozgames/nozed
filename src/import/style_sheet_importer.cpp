//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//
// @STL

using namespace std;
using namespace noz;

namespace fs = filesystem;

struct StyleKeyHash
{
    template <class T1, class T2>
    std::size_t operator () (const std::pair<T1, T2>& p) const
    {
        auto h1 = std::hash<T1>{}(p.first);
        auto h2 = std::hash<T2>{}(p.second);
        return h1 ^ (h2 << 1);
    }
};

using StyleKey = pair<string, PseudoState>;
using StyleDictionary = unordered_map<StyleKey, Style, StyleKeyHash>;

static PseudoState ParsePseudoState(const string& str)
{
    if (str == "selected:hover") return PSEUDO_STATE_HOVER | PSEUDO_STATE_SELECTED;
    if (str == "hover")    return PSEUDO_STATE_HOVER;
    if (str == "active")   return PSEUDO_STATE_ACTIVE;
    if (str == "selected") return PSEUDO_STATE_SELECTED;
    if (str == "disabled") return PSEUDO_STATE_DISABLED;
    if (str == "focused")  return PSEUDO_STATE_FOCUSED;
    if (str == "pressed")  return PSEUDO_STATE_PRESSED;
    if (str == "checked")  return PSEUDO_STATE_CHECKED;
    return PSEUDO_STATE_NONE;
}

static StyleColor ParseStyleColor(const string& value)
{
    Tokenizer tk = {};
    Color color = COLOR_TRANSPARENT;
    Init(tk, value.c_str());
    ExpectColor(tk, &color);
    return StyleColor{ {STYLE_KEYWORD_OVERWRITE}, color };
}

static StyleLength ParseStyleLength(const string& value)
{
    if (value == "auto")
        return StyleLength { .parameter = { .keyword = STYLE_KEYWORD_OVERWRITE }, .unit = STYLE_LENGTH_UNIT_AUTO, .value = 0.0f };

    if (!value.empty() && value.back() == '%')
        return StyleLength{.parameter = {.keyword = STYLE_KEYWORD_OVERWRITE},
                           .unit = STYLE_LENGTH_UNIT_PERCENT,
                           .value = stof(value.substr(0, value.length() - 1)) / 100.0f};

    return StyleLength { .parameter = {.keyword = STYLE_KEYWORD_OVERWRITE}, .unit = STYLE_LENGTH_UNIT_FIXED, .value = stof(value) };
}

static StyleInt ParseStyleInt (const string& value)
{
    return StyleInt { .parameter = {.keyword = STYLE_KEYWORD_OVERWRITE}, .value = stoi(value) };
}

static StyleFlexDirection ParseStyleFlexDirection(const string& value)
{
    if (value == "row") return StyleFlexDirection{ STYLE_KEYWORD_OVERWRITE, FLEX_DIRECTION_ROW };
    if (value == "column") return StyleFlexDirection{ STYLE_KEYWORD_OVERWRITE, FLEX_DIRECTION_COL };
    return StyleFlexDirection{ STYLE_KEYWORD_INHERIT, FLEX_DIRECTION_ROW };
}

static StyleTextAlign ParseTextAlign(const string& value)
{
    if (value == "center") return StyleTextAlign { STYLE_KEYWORD_OVERWRITE, TEXT_ALIGN_CENTER };
    if (value == "max") return StyleTextAlign { STYLE_KEYWORD_OVERWRITE, TEXT_ALIGN_MAX };
    return StyleTextAlign{ STYLE_KEYWORD_INHERIT, TEXT_ALIGN_MIN };
}

static void WriteStyleSheetData(Stream* stream, const StyleDictionary& styles)
{
    // Create a name table of all the style names
    set<const Name*> name_set;
    for (const auto& [style_key, style] : styles)
        name_set.insert(GetName(style_key.first.c_str()));

    const Name** name_table = (const Name**)Alloc(ALLOCATOR_DEFAULT, (u32)sizeof(const Name*) * (u32)name_set.size());
    u32 name_index = 0;
    for (const auto& name : name_set)
        name_table[name_index++] = name;

    // Write asset header
    AssetHeader header = {};
    header.signature = ASSET_SIGNATURE_STYLE_SHEET;
    header.version = 1;
    header.flags = 0;
    header.names = (u32)name_set.size();
    WriteAssetHeader(stream, &header, name_table);

    // Write number of styles
    WriteU32(stream, static_cast<uint32_t>(styles.size()));

    // Write the styles
    for (const auto& [class_name, style_data] : styles)
        SerializeStyle(style_data, stream);

    // Write the style names / states
    for (const auto& [style_key, style] : styles)
    {
        const Name* style_name = GetName(style_key.first.c_str());
        for (u32 i=0; i<header.names; i++)
            if (name_table[i] == style_name)
            {
                WriteU32(stream, i);
                break;
            }

        WriteU32(stream, style_key.second);
    }

    Free(name_table);
}

static bool ParseParameter(const string& group, const string& key, Props* source, Style& style)
{
    auto value = source->GetString(group.c_str(),key.c_str(), nullptr);
    if (value.empty())
        return false;

    if (key == "width")
        style.width = ParseStyleLength(value);
    else if (key == "height")
        style.height = ParseStyleLength(value);
    else if (key == "background-color")
        style.background_color = ParseStyleColor(value);
    else if (key == "color")
        style.color = ParseStyleColor(value);
    else if (key == "font-size")
        style.font_size = ParseStyleInt(value);
    else if (key == "margin")
        style.margin_top = style.margin_left = style.margin_right = style.margin_bottom = ParseStyleLength(value);
    else if (key == "margin-top")
        style.margin_top = ParseStyleLength(value);
    else if (key == "margin-left")
        style.margin_left = ParseStyleLength(value);
    else if (key == "margin-bottom")
        style.margin_bottom = ParseStyleLength(value);
    else if (key == "margin-right")
        style.margin_right = ParseStyleLength(value);
    else if (key == "padding")
        style.padding_top = style.padding_left = style.padding_right = style.padding_bottom = ParseStyleLength(value);
    else if (key == "padding-top")
        style.padding_top = ParseStyleLength(value);
    else if (key == "padding-left")
        style.padding_left = ParseStyleLength(value);
    else if (key == "padding-bottom")
        style.padding_bottom = ParseStyleLength(value);
    else if (key == "padding-right")
        style.padding_right = ParseStyleLength(value);
    else if (key == "flex-direction")
        style.flex_direction = ParseStyleFlexDirection(value);
    else if (key == "text-align")
        style.text_align = ParseTextAlign(value);
    else if (key == "vertical-align")
        style.vertical_align = ParseTextAlign(value);

    return true;
}

static StyleKey ParseStyleKey(const string& value)
{
    auto colon_pos = value.find(':');
    if (colon_pos == string::npos)
        return StyleKey{ value, PSEUDO_STATE_NONE };

    auto class_name = value.substr(0, colon_pos);
    auto state_name = value.substr(colon_pos + 1);
    auto state = ParsePseudoState(state_name);
    return { class_name, state };
}

static void ParseStyles(Props* source, Props* meta, StyleDictionary& styles)
{
    (void)meta;

    for (const auto& inherit : source->GetKeys("inherit"))
    {
        (void)inherit;
    }

    for (auto& group_name : source->GetGroups())
    {
        if (group_name == "inherit")
            continue;

        auto style_key = ParseStyleKey(group_name);

        Style style = GetDefaultStyle();
        auto style_keys = source->GetKeys(group_name.c_str());
        for (const auto& key_name : style_keys)
            ParseParameter(group_name, key_name, source, style);

        auto it = styles.find(style_key);
        if (it != styles.end())
            MergeStyles(it->second, style);
        else
            styles[style_key] = style;
    }

    for (const auto& [style_key, style] : styles)
    {
        auto base_style_it = styles.find({style_key.first, PSEUDO_STATE_NONE});
        if (base_style_it == styles.end())
            continue;

        auto resolved_style = base_style_it->second;
        MergeStyles(resolved_style, style);
        styles[style_key] = resolved_style;
    }
}

static StyleDictionary ParseStyles(Props* source, Props* meta)
{
    StyleDictionary styles = {};
    ParseStyles(source, meta, styles);
    return styles;
}

void ImportStyleSheet(const fs::path& source_path, Stream* output_stream, Props* config, Props* meta)
{
    (void)config;

    // Read source file
    std::ifstream file(source_path, std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("could not read file");

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    Props* style_props = Props::Load(content.c_str(), content.size());
    if (!style_props)
        throw std::runtime_error("could not load style sheet");

    // Parse styles from source file  
    auto styles = ParseStyles(style_props, meta);
    
    // Write stylesheet data using Stream API
    WriteStyleSheetData(output_stream, styles);
}

static AssetImporterTraits g_stylesheet_importer_traits = {
    .signature = ASSET_SIGNATURE_STYLE_SHEET,
    .ext = ".styles",
    .import_func = ImportStyleSheet
};

AssetImporterTraits* GetStyleSheetImporterTraits()
{
    return &g_stylesheet_importer_traits;
}
