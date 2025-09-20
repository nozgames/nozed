//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//
// @STL

using namespace std;
using namespace noz;

namespace fs = filesystem;

using StyleDictionary = unordered_map<string, Style>;

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

static StyleInt ParseStyleInt(const string& value)
{
    return StyleInt { .parameter = {.keyword = STYLE_KEYWORD_OVERWRITE}, .value = stoi(value) };
}

static StyleFloat ParseStyleFloat(const string& value)
{
    return StyleFloat { .parameter = {.keyword = STYLE_KEYWORD_OVERWRITE}, .value = stof(value) };
}

static StyleFlexDirection ParseStyleFlexDirection(const string& value)
{
    if (value == "row") return StyleFlexDirection{ STYLE_KEYWORD_OVERWRITE, FLEX_DIRECTION_ROW };
    if (value == "column") return StyleFlexDirection{ STYLE_KEYWORD_OVERWRITE, FLEX_DIRECTION_COL };
    return StyleFlexDirection{ STYLE_KEYWORD_INHERIT, FLEX_DIRECTION_ROW };
}

static StylePosition ParsePosition(const string& value)
{
    if (value == "absolute") return StylePosition{ STYLE_KEYWORD_OVERWRITE, POSITION_TYPE_ABSOLUTE };
    return StylePosition{ STYLE_KEYWORD_INHERIT, POSITION_TYPE_RELATIVE };
}

static StyleTextAlign ParseTextAlign(const string& value)
{
    if (value == "center") return StyleTextAlign { STYLE_KEYWORD_OVERWRITE, TEXT_ALIGN_CENTER };
    if (value == "max") return StyleTextAlign { STYLE_KEYWORD_OVERWRITE, TEXT_ALIGN_MAX };
    return StyleTextAlign{ STYLE_KEYWORD_INHERIT, TEXT_ALIGN_MIN };
}

static StyleFont ParseFont(const string& value)
{
    StyleFont font = { .parameter = {.keyword = STYLE_KEYWORD_OVERWRITE}, .id = 0 };
    Copy(font.name, MAX_NAME_LENGTH, value.c_str());
    return font;
}

static void SerializeStyles(Stream* stream, const StyleDictionary& styles)
{
    const Name** name_table = (const Name**)Alloc(ALLOCATOR_DEFAULT, (u32)sizeof(const Name*) * (u32)styles.size());
    u32 name_index = 0;
    for (const auto& kv : styles)
        name_table[name_index++] = GetName(kv.first.c_str());

    // Write asset header
    AssetHeader header = {};
    header.signature = ASSET_SIGNATURE_STYLE_SHEET;
    header.version = 1;
    header.flags = 0;
    header.names = (u32)styles.size();
    WriteAssetHeader(stream, &header, name_table);

    // Write number of styles
    WriteU32(stream, static_cast<uint32_t>(styles.size()));

    // Write the styles
    for (const auto& [style_name, style] : styles)
        SerializeStyle(style, stream);

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
    else if (key == "background-vignette-color")
        style.background_vignette_color = ParseStyleColor(value);
    else if (key == "background-vignette-intensity")
        style.background_vignette_intensity = ParseStyleFloat(value);
    else if (key == "background-vignette-smoothness")
        style.background_vignette_smoothness = ParseStyleFloat(value);
    else if (key == "color")
        style.color = ParseStyleColor(value);
    else if (key == "font-size")
        style.font_size = ParseStyleInt(value);
    else if (key == "font")
        style.font = ParseFont(value);
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
    else if (key == "position")
        style.position = ParsePosition(value);
    else if (key == "text-align")
        style.text_align = ParseTextAlign(value);
    else if (key == "vertical-align")
        style.vertical_align = ParseTextAlign(value);
    else if (key == "rotate")
        style.rotate = ParseStyleFloat(value);
    else if (key == "translate-x")
        style.translate_x = ParseStyleFloat(value);
    else if (key == "translate-y")
        style.translate_y = ParseStyleFloat(value);
    else if (key == "scale")
        style.scale = ParseStyleFloat(value);
    else if (key == "transform-origin-x")
        style.translate_origin_x = ParseStyleFloat(value);
    else if (key == "transform-origin-y")
        style.translate_origin_y = ParseStyleFloat(value);

    return true;
}

static void ParseStyle(Props* source, const std::string& group_name, StyleDictionary& styles)
{
    if (styles.contains(group_name))
        return;

    std::string inherit = source->GetString(group_name.c_str(), "inherit", "");
    if (!inherit.empty())
    {
        ParseStyle(source, inherit, styles);

        auto inherit_it = styles.find(inherit);
        if (inherit_it != styles.end())
            styles[group_name] = inherit_it->second;
    }

    Style style = GetDefaultStyle();
    auto style_keys = source->GetKeys(group_name.c_str());
    for (const auto& key_name : style_keys)
        ParseParameter(group_name, key_name, source, style);

    auto it = styles.find(group_name);
    if (it != styles.end())
        MergeStyles(it->second, style);
    else
        styles[group_name] = style;
}

static void ParseStyles(Props* source, Props* meta, StyleDictionary& styles)
{
    (void)meta;

    for (auto& group_name : source->GetGroups())
        ParseStyle(source, group_name, styles);
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
    SerializeStyles(output_stream, styles);
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
