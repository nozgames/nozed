//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

// @STL

#include "asset_manifest.h"
#include <vector>
#include <map>

namespace fs = std::filesystem;

struct AssetEntry
{
    std::string path;
    uint32_t signature;
    size_t file_size;
    std::string var_name;
};

struct PathNode 
{
    std::map<std::string, std::unique_ptr<PathNode>> children;
    std::vector<AssetEntry> assets;
};

struct ManifestGenerator
{
    std::vector<AssetEntry> asset_entries;
    fs::path output_dir;
    Stream* manifest_stream;
    const std::vector<AssetImporterTraits*>* importers;
    Props* config;
};

static std::vector<std::pair<std::string, std::string>> core_assets = {
    { "shaders.ui", "shaders/ui" },
    { "shaders.text", "shaders/text" },
    { "shaders.vfx", "shaders/vfx" },
    { "fonts.fallback", "fonts/Roboto-Black" }
};

static void GenerateManifestCode(ManifestGenerator* generator, const fs::path& header_path, Props* config);
static void GenerateAssetsHeader(ManifestGenerator* generator, const fs::path& header_path);
static std::string PathToVarName(const std::string& path);
static std::string PathToNameVar(const std::string& path);
static void OrganizeAssetsByType(ManifestGenerator* generator);
static void ScanAssetFile(const fs::path& file_path, ManifestGenerator* generator);
static const char* ToStringFromSignature(AssetSignature signature, const std::vector<AssetImporterTraits*>& importers);
static const char* ToMacroFromSignature(AssetSignature signature, const std::vector<AssetImporterTraits*>& importers);
static const char* ToReloadMacroFromSignature(AssetSignature signature, const std::vector<AssetImporterTraits*>& importers);
static void GenerateCoreAssetAssignments(ManifestGenerator* generator, Stream* stream, Props* config);
static void GenerateHotloadNames(ManifestGenerator* generator, Stream* stream);
static void GenerateHotloadFunction(ManifestGenerator* generator, Stream* stream, Props* config);

bool GenerateAssetManifest(
    const fs::path& output_directory,
    const fs::path& manifest_output_path,
    const std::vector<AssetImporterTraits*>& importers,
    Props* config)
{
    if (output_directory.empty() || manifest_output_path.empty())
    {
        printf("ERROR: Invalid parameters for manifest generation\n");
        return false;
    }

    // Initialize manifest generator
    ManifestGenerator generator = {};
    generator.output_dir = output_directory;
    generator.asset_entries.reserve(64);
    generator.importers = &importers;
    generator.config = config;

    generator.manifest_stream = CreateStream(nullptr, 4096);
    if (!generator.manifest_stream)
    {
        printf("ERROR: Failed to create manifest stream\n");
        return false;
    }

    if (!fs::exists(generator.output_dir))
    {
        // Generate header file path
        fs::path header_path = manifest_output_path;
        header_path.replace_extension(".h");
        
        GenerateManifestCode(&generator, header_path, config);
        bool success = SaveStream(generator.manifest_stream, manifest_output_path);
        Free(generator.manifest_stream);
        return success;
    }

    if (!fs::is_directory(generator.output_dir))
    {
        printf("ERROR: '%s' is not a directory\n", generator.output_dir.string().c_str());
        Free(generator.manifest_stream);
        return false;
    }

    // Scan all files in the output directory recursively using std::filesystem
    try
    {
        for (const auto& entry : fs::recursive_directory_iterator(generator.output_dir))
        {
            if (entry.is_regular_file())
            {
                ScanAssetFile(entry.path(), &generator);
            }
        }
    }
    catch (const std::exception& e)
    {
        printf("ERROR: Failed to enumerate files in directory: %s - %s\n", 
               generator.output_dir.string().c_str(), e.what());
        Free(generator.manifest_stream);
        return false;
    }

    // Generate header file (change .cpp to .h)
    fs::path header_path = manifest_output_path;
    header_path.replace_extension(".h");
    
    // Generate the manifest C code
    GenerateManifestCode(&generator, header_path, config);
    GenerateAssetsHeader(&generator, header_path);

    // Save the manifest to file
    bool success = SaveStream(generator.manifest_stream, manifest_output_path);
    if (!success)
    {
        printf("ERROR: Failed to save manifest to: %s\n", manifest_output_path.string().c_str());
    }

    // Clean up
    Free(generator.manifest_stream);

    return success;
}

static void WriteHeaderNestedStructs(Stream* stream, const PathNode& node, const std::vector<AssetImporterTraits*>& importers, int indent_level = 1)
{
    std::string indent(indent_level * 4, ' ');
    
    // Write child directories as nested structs
    for (const auto& [name, child] : node.children)
    {
        WriteCSTR(stream, "%sstruct\n%s{\n", indent.c_str(), indent.c_str());
        WriteHeaderNestedStructs(stream, *child, importers, indent_level + 1);
        WriteCSTR(stream, "%s} %s;\n", indent.c_str(), name.c_str());
    }
    
    // Write assets as forward declarations
    for (const auto& entry : node.assets)
    {
        const char* type_name = ToStringFromSignature(entry.signature, importers);
        if (type_name)
        {
            std::string var_name = PathToVarName(fs::path(entry.path).filename().replace_extension("").string());
            WriteCSTR(stream, "%s%s* %s;\n", indent.c_str(), type_name, var_name.c_str());
        }
    }
}

static void GenerateAssetsHeader(ManifestGenerator* generator, const fs::path& header_path)
{
    Stream* header_stream = CreateStream(nullptr, 1024);
    if (!header_stream)
        return;
        
    WriteCSTR(header_stream,
        "//\n"
        "// Auto-generated asset header - DO NOT EDIT MANUALLY\n"
        "// Generated by NoZ Game Engine Asset Importer\n"
        "//\n\n");
    
    // Generate asset listing comment block
    if (!generator->asset_entries.empty())
    {
        // Group assets by type
        std::map<std::string, std::vector<std::string>> assets_by_type;
        
        for (const auto& entry : generator->asset_entries)
        {
            const char* type_name = ToStringFromSignature(entry.signature, *generator->importers);
            if (!type_name)
                continue;

            // Convert asset path to access path (e.g., "textures/icons/button" -> "LoadedAssets.textures.icons.button")
            fs::path asset_path(entry.path);
            std::string access_path = "LoadedAssets";

            auto parent_path = asset_path.parent_path();
            for (const auto& part : parent_path)
                access_path += "." + part.string();

            std::string var_name = PathToVarName(asset_path.filename().string());
            access_path += "." + var_name;

            // Add to the appropriate type group
            std::string type_key = std::string(type_name) + "s";
            std::transform(type_key.begin(), type_key.end(), type_key.begin(), ::tolower);
            assets_by_type[type_key].push_back(access_path);
        }
        
        // Write the comment block
        for (const auto& [type_name, asset_list] : assets_by_type)
        {
            WriteCSTR(header_stream, "// @%s\n", type_name.c_str());
            for (const auto& asset_path : asset_list)
                WriteCSTR(header_stream, "// %s\n", asset_path.c_str());
            WriteCSTR(header_stream, "//\n");
        }
        WriteCSTR(header_stream, "\n");
    }
    
    WriteCSTR(header_stream,
        "#pragma once\n\n"
        "#include <noz/core_assets.h>\n\n"
        "// Forward declarations\n"
        "struct Allocator;\n"
        "struct Shader;\n"
        "struct Vfx;\n"
        "struct Texture;\n"
        "struct Mesh;\n"
        "struct Font;\n"
        "struct Material;\n"
        "struct Skeleton;\n"
        "struct Sound;\n\n");
    
    // Build directory tree (same as in OrganizeAssetsByType)
    PathNode root;
    for (const auto& entry : generator->asset_entries)
    {
        fs::path asset_path(entry.path);
        PathNode* current = &root;
        
        auto parent_path = asset_path.parent_path();
        for (const auto& part : parent_path)
        {
            std::string part_str = part.string();
            if (current->children.find(part_str) == current->children.end())
            {
                current->children[part_str] = std::make_unique<PathNode>();
            }
            current = current->children[part_str].get();
        }
        
        AssetEntry modified_entry = entry;
        current->assets.push_back(modified_entry);
    }
    
    // Write LoadedAssets struct
    WriteCSTR(header_stream, "struct LoadedAssets\n{\n");
    if (generator->asset_entries.empty())
        WriteCSTR(header_stream, "    void* _dummy;\n");
    else
        WriteHeaderNestedStructs(header_stream, root, *generator->importers);

    WriteCSTR(header_stream, "};\n\n");
    
    WriteCSTR(header_stream, "extern LoadedAssets %s;\n", generator->config->GetString("manifest", "global_variable", "Assets").c_str());
    WriteCSTR(header_stream, "extern LoadedCoreAssets g_core_assets;\n\n");
    WriteCSTR(header_stream, "bool LoadAssets(Allocator* allocator);\n");
    WriteCSTR(header_stream, "void UnloadAssets();\n\n");
    WriteCSTR(header_stream, "#ifdef NOZ_EDITOR\n");
    WriteCSTR(header_stream, "void HotloadAsset(const Name* incoming_name);\n");
    WriteCSTR(header_stream, "#endif\n");
    
    // Save header file
    SaveStream(header_stream, header_path);
    Free(header_stream);
}

static bool ReadAssetHeader(const fs::path& file_path, uint32_t* signature)
{
    Stream* stream = LoadStream(nullptr, file_path);
    if (!stream)
        return false;

    // Read asset header
    AssetHeader header;
    if (!ReadAssetHeader(stream, &header))
    {
        Free(stream);
        return false;
    }

    *signature = header.signature;

    Free(stream);
    return true;
}

static void ScanAssetFile(const fs::path& file_path, ManifestGenerator* generator)
{
    uint32_t signature = 0;
    if (!ReadAssetHeader(file_path, &signature))
        return;

    // Check if any importer recognizes this signature
    bool is_recognized_asset = false;
    for (const auto* importer : *generator->importers)
    {
        if (importer && importer->signature == signature)
        {
            is_recognized_asset = true;
            break;
        }
    }
    
    if (!is_recognized_asset)
        return;

    // Make path relative to output_dir first
    fs::path relative_path = fs::relative(file_path, generator->output_dir);
    
    // Remove extension for comparison and storage
    relative_path.replace_extension("");
    std::string relative_str = relative_path.string();
    
    // Check if this asset is already in the list (compare without extensions)
    for (const auto& existing : generator->asset_entries)
        if (existing.path == relative_str)
            return;

    AssetEntry entry = {};
    entry.path = relative_str;
    entry.file_size = fs::file_size(file_path);
    entry.var_name = PathToVarName(entry.path);
    entry.signature = signature;
    generator->asset_entries.push_back(entry);
}

static void GenerateManifestCode(ManifestGenerator* generator, const fs::path& header_path, Props* config)
{
    auto stream = generator->manifest_stream;

    // Extract just the filename from the header path
    std::string header_filename = header_path.filename().string();

    WriteCSTR(stream,
        "//\n"
        "// Auto-generated asset manifest - DO NOT EDIT MANUALLY\n"
        "// Generated by NoZ Game Engine Asset Importer\n"
        "//\n\n"
        "// @includes\n"
        "#include <noz/noz.h>\n"
        "#include \"%s\"\n\n", header_filename.c_str());

    WriteCSTR(stream, "// @assets\n");
    WriteCSTR(stream, "LoadedAssets %s = {};\n\n", config->GetString("manifest", "global_variable", "Assets").c_str());

    // Generate name variables
    GenerateHotloadNames(generator, stream);
    
    // Generate hotload function
    GenerateHotloadFunction(generator, stream, config);

    OrganizeAssetsByType(generator);

    WriteCSTR(stream,
        "// @init\n"
        "bool LoadAssets(Allocator* allocator)\n"
        "{\n");
    
    // Initialize static name_t variables for asset management
    WriteCSTR(stream, "    // Initialize static name_t variables for asset management\n");
    
    // Collect unique asset paths to avoid duplicate initialization
    std::set<std::string> unique_paths;
    for (const auto& entry : generator->asset_entries)
    {
        // Convert backslashes to forward slashes for the asset path
        std::string normalized_path = entry.path;
        std::replace(normalized_path.begin(), normalized_path.end(), '\\', '/');
        unique_paths.insert(normalized_path);
    }
    
    // Initialize name variables for unique paths only
    for (const auto& path : unique_paths)
    {
        std::string name_var = PathToNameVar(path);
        WriteCSTR(stream, "    %s = GetName(\"%s\");\n", name_var.c_str(), path.c_str());
    }
    
    WriteCSTR(stream, "\n");
    
    for (const auto& entry : generator->asset_entries)
    {
        const char* macro_name = ToMacroFromSignature(entry.signature, *generator->importers);
        if (!macro_name)
            continue;

        // Convert backslashes to forward slashes for the asset path
        std::string normalized_path = entry.path;
        std::replace(normalized_path.begin(), normalized_path.end(), '\\', '/');

        // Build nested access path (e.g., Assets.textures.icons.myicon)
        fs::path asset_path(entry.path);
        std::string access_path = config->GetString("manifest", "global_variable", "Assets");
        
        auto parent_path = asset_path.parent_path();
        for (const auto& part : parent_path)
        {
            access_path += "." + part.string();
        }
        
        std::string var_name = PathToVarName(asset_path.filename().replace_extension("").string());
        access_path += "." + var_name;

        std::string name_var = PathToNameVar(normalized_path);
        WriteCSTR(
            stream,
            "    %s(allocator, %s, %s);\n",
            macro_name,
            name_var.c_str(),
            access_path.c_str());
    }
    
    // Generate core asset assignments
    GenerateCoreAssetAssignments(generator, stream, config);
    
    WriteCSTR(stream, "\n    return true;\n}\n\n");

    std::string global_var = config->GetString("manifest", "global_variable", "Assets");

    // Write UnloadAssets function
    WriteCSTR(stream,
        "// @uninit\n"
        "void UnloadAssets()\n"
        "{\n"
        "    // Clear all asset pointers\n"
        "    memset(&%s, 0, sizeof(%s));\n"
        "}\n", global_var.c_str(), global_var.c_str());
}


static std::string PathToVarName(const std::string& path_str)
{
    if (path_str.empty())
    {
        return "unknown";
    }
    
    // Convert full path to filesystem path and remove extension
    fs::path path(path_str);
    path.replace_extension("");
    std::string full_path = path.string();
    
    // If path is empty, use "unknown"
    if (full_path.empty())
    {
        return "unknown";
    }
    
    // Convert full path to valid C variable name
    std::string result;
    
    for (char c : full_path)
    {
        if (std::isalnum(c))
            result += std::tolower(c);
        else
            result += '_';
    }
    
    // Check for C keywords and add underscore prefix if needed
    static std::vector<std::string> c_keywords = {
        "default", "switch", "case", "break", "continue", "return",
        "if", "else", "for", "while", "do", "goto", "void",
        "int", "float", "double", "char", "const", "static",
        "struct", "union", "enum", "typedef"
    };
    
    if (std::find(c_keywords.begin(), c_keywords.end(), result) != c_keywords.end())
    {
        result = "_" + result;
    }
    
    return result;
}



static void WriteNestedStructs(Stream* stream, const PathNode& node, const std::vector<AssetImporterTraits*>& importers, int indent_level = 1)
{
    std::string indent(indent_level * 4, ' ');
    
    // Write child directories as nested structs
    for (const auto& [name, child] : node.children)
    {
        WriteCSTR(stream, "%sstruct\n%s{\n", indent.c_str(), indent.c_str());
        WriteNestedStructs(stream, *child, importers, indent_level + 1);
        WriteCSTR(stream, "%s} %s;\n", indent.c_str(), name.c_str());
    }
    
    // Write assets as pointers
    for (const auto& entry : node.assets)
    {
        const char* type_name = ToStringFromSignature(entry.signature, importers);
        if (type_name)
        {
            WriteCSTR(stream, "%s%s* %s;\n", indent.c_str(), type_name, entry.var_name.c_str());
        }
    }
}

static void OrganizeAssetsByType(ManifestGenerator* generator)
{
    // Build directory tree
    PathNode root;
    
    for (const auto& entry : generator->asset_entries)
    {
        fs::path asset_path(entry.path);
        PathNode* current = &root;
        
        // Navigate/create directory structure
        auto parent_path = asset_path.parent_path();
        for (const auto& part : parent_path)
        {
            std::string part_str = part.string();
            if (current->children.find(part_str) == current->children.end())
            {
                current->children[part_str] = std::make_unique<PathNode>();
            }
            current = current->children[part_str].get();
        }
        
        // Update var_name to just be the filename
        AssetEntry modified_entry = entry;
        modified_entry.var_name = PathToVarName(asset_path.filename().replace_extension("").string());
        
        // Add asset to the final directory
        current->assets.push_back(modified_entry);
    }
}


static const char* ToMacroFromSignature(AssetSignature signature, const std::vector<AssetImporterTraits*>& importers)
{
    static std::map<AssetSignature, std::string> macro_cache;
    
    for (const auto* importer : importers)
    {
        if (importer && importer->signature == signature)
        {
            // Check cache first
            if (macro_cache.find(signature) != macro_cache.end())
                return macro_cache[signature].c_str();

            // Build macro name: NOZ_LOAD_ + uppercase type name
            std::string type_name = importer->type_name;
            std::string macro_name = "NOZ_LOAD_";
            
            // Convert type name to uppercase and handle special cases
            for (char c : type_name)
            {
                if (std::islower(c))
                    macro_name += std::toupper(c);
                else if (std::isupper(c))
                {
                    // Insert underscore before uppercase letters (except first)
                    if (macro_name.size() > 9) // 9 = length of "NOZ_LOAD_"
                        macro_name += '_';
                    macro_name += c;
                }
                else
                    macro_name += c;
            }
            
            // Cache and return
            macro_cache[signature] = macro_name;
            return macro_cache[signature].c_str();
        }
    }
    return nullptr;
}

static const char* ToReloadMacroFromSignature(AssetSignature signature, const std::vector<AssetImporterTraits*>& importers)
{
    static std::map<AssetSignature, std::string> reload_macro_cache;
    
    for (const auto* importer : importers)
    {
        if (importer && importer->signature == signature)
        {
            // Check cache first
            if (reload_macro_cache.find(signature) != reload_macro_cache.end())
                return reload_macro_cache[signature].c_str();

            // Build macro name: NOZ_RELOAD_ + uppercase type name
            std::string type_name = importer->type_name;
            std::string macro_name = "NOZ_RELOAD_";
            
            // Convert type name to uppercase and handle special cases
            for (char c : type_name)
            {
                if (std::islower(c))
                    macro_name += std::toupper(c);
                else if (std::isupper(c))
                {
                    // Insert underscore before uppercase letters (except first)
                    if (macro_name.size() > 11) // 11 = length of "NOZ_RELOAD_"
                        macro_name += '_';
                    macro_name += c;
                }
                else
                    macro_name += c;
            }
            
            // Cache and return
            reload_macro_cache[signature] = macro_name;
            return reload_macro_cache[signature].c_str();
        }
    }
    return nullptr;
}

static const char* ToStringFromSignature(AssetSignature signature, const std::vector<AssetImporterTraits*>& importers)
{
    for (const auto* importer : importers)
        if (importer && importer->signature == signature)
            return importer->type_name;

    return nullptr;
}

static void GenerateCoreAssetAssignments(ManifestGenerator* generator, Stream* stream, Props* config)
{
    WriteCSTR(stream, "\n    // Assign core engine assets\n");
    
    for (const auto& [core_path, asset_path] : core_assets)
    {
        // Convert asset path to access path (e.g., "shaders/shadow" -> "Assets.shaders.shadow")
        fs::path path(asset_path);
        std::string access_path = config->GetString("manifest", "global_variable", "Assets");

        auto parent_path = path.parent_path();
        for (const auto& part : parent_path)
            access_path += "." + part.string();

        std::string var_name = PathToVarName(path.filename().string());
        access_path += "." + var_name;
        
        // Generate: CoreAssets.shaders.shadow = Assets.shaders.shadow;
        WriteCSTR(stream, "    g_core_assets.%s = %s;\n", core_path.c_str(), access_path.c_str());
    }
}

static std::string PathToNameVar(const std::string& path)
{
    // Convert path to NAME_ variable format
    // e.g., "textures/icons/button" -> "NAME_textures_icons_button"
    std::string result = "NAME_";
    
    for (char c : path)
    {
        if (std::isalnum(c))
            result += std::tolower(c);
        else
            result += '_';
    }
    
    return result;
}


static void GenerateHotloadNames(ManifestGenerator* generator, Stream* stream)
{
    WriteCSTR(stream, "// @names\n");
    WriteCSTR(stream, "// Static asset names for efficient comparison and safer asset management\n");
    
    // Collect unique asset paths to avoid duplicate name variables
    std::set<std::string> unique_paths;
    
    for (const auto& entry : generator->asset_entries)
    {
        // Convert backslashes to forward slashes for the asset path
        std::string normalized_path = entry.path;
        std::replace(normalized_path.begin(), normalized_path.end(), '\\', '/');
        unique_paths.insert(normalized_path);
    }
    
    // Generate name variables for unique paths only
    for (const auto& path : unique_paths)
    {
        std::string name_var = PathToNameVar(path);
        WriteCSTR(stream, "static const Name* %s;\n", name_var.c_str());
    }
    
    WriteCSTR(stream, "\n");
}

static void GenerateHotloadFunction(ManifestGenerator* generator, Stream* stream, Props* config)
{
    WriteCSTR(stream, "#ifdef NOZ_EDITOR\n\n");
    WriteCSTR(stream, "void HotloadAsset(const Name* incoming_name)\n");
    WriteCSTR(stream, "{\n");
    
    // Group assets by type for cleaner organization
    std::map<std::string, std::vector<AssetEntry>> assets_by_type;
    
    for (const auto& entry : generator->asset_entries)
    {
        const char* type_name = ToStringFromSignature(entry.signature, *generator->importers);
        if (!type_name)
            continue;

        std::string type_key = std::string(type_name) + "s";
        std::transform(type_key.begin(), type_key.end(), type_key.begin(), ::tolower);
        assets_by_type[type_key].push_back(entry);
    }
    
    // Generate hotload checks grouped by type
    for (const auto& [type_name, entries] : assets_by_type)
    {
        WriteCSTR(stream, "    // @%s\n", type_name.c_str());
        
        for (const auto& entry : entries)
        {
            // Convert backslashes to forward slashes for the asset path
            std::string normalized_path = entry.path;
            std::replace(normalized_path.begin(), normalized_path.end(), '\\', '/');
            
            std::string name_var = PathToNameVar(normalized_path);
            
            // Build nested access path (e.g., Assets.textures.icons.myicon)
            fs::path asset_path(entry.path);
            std::string access_path = config->GetString("manifest", "global_variable", "Assets");
            
            auto parent_path = asset_path.parent_path();
            for (const auto& part : parent_path)
            {
                access_path += "." + part.string();
            }
            
            std::string var_name = PathToVarName(asset_path.filename().replace_extension("").string());
            access_path += "." + var_name;
            
            // Get type-specific reload macro
            const char* reload_macro = ToReloadMacroFromSignature(entry.signature, *generator->importers);
            if (reload_macro)
            {
                WriteCSTR(stream, "    %s(%s, %s);\n",
                    reload_macro,
                    name_var.c_str(),
                    access_path.c_str());
            }
        }
        
        WriteCSTR(stream, "\n");
    }
    
    WriteCSTR(stream, "}\n");
    WriteCSTR(stream, "#endif // NOZ_EDITOR\n\n");
}