//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "../asset_manifest.h"
#include "../file_watcher.h"
#include "../server.h"

namespace fs = std::filesystem;

extern AssetImporterTraits* GetShaderImporterTraits();
extern AssetImporterTraits* GetTextureImporterTraits();
extern AssetImporterTraits* GetFontImporterTraits();
extern AssetImporterTraits* GetMeshImporterTraits();
extern AssetImporterTraits* GetStyleSheetImporterTraits();
extern AssetImporterTraits* GetVfxImporterTraits();
extern AssetImporterTraits* GetSoundImporterTraits();
extern AssetImporterTraits* GetSkeletonImporterTraits();

struct ImportJob
{
    fs::path source_path;
    const AssetImporterTraits* importer;

    ImportJob(const fs::path& path, const AssetImporterTraits* imp)
        : source_path(path), importer(imp) {}
};

struct Importer
{
    std::vector<ImportJob> queue;
    Props* config;
    std::atomic<bool> running;
    std::unique_ptr<std::thread> thread;
    std::atomic<bool> thread_running;
    std::vector<AssetImporterTraits*> importers;
};

static Importer g_importer = {};

static bool ProcessImportQueue();

static bool LoadConfig()
{
    std::filesystem::path config_path = "./editor.cfg";
    if (Stream* config_stream = LoadStream(nullptr, config_path))
    {
        g_importer.config = Props::Load(config_stream);
        Free(config_stream);

        if (g_importer.config != nullptr)
            return true;
    }

    LogError("missing configuration '%s'", config_path.string().c_str());
    return false;
}

const AssetImporterTraits* FindImporter(const fs::path& path)
{
    std::string file_ext = path.extension().string();
    std::transform(file_ext.begin(), file_ext.end(), file_ext.begin(), ::tolower);

    for (auto* importer : g_importer.importers)
        if (importer && importer->file_extensions)
            for (const char** ext_ptr = importer->file_extensions; *ext_ptr != nullptr; ++ext_ptr)
                if (file_ext == *ext_ptr && (!importer->can_import || importer->can_import(path)))
                    return importer;

    return nullptr;
}

const AssetImporterTraits* FindImporter(AssetSignature signature)
{
    for (const auto* importer : g_importer.importers)
        if (importer && importer->signature == signature)
            return importer;

    return nullptr;
}

void HandleFileChange(const fs::path& file_path, FileChangeType change_type)
{
    if (change_type == FILE_CHANGE_TYPE_DELETED)
        return;

    // Check if this is a .meta file
    std::string ext = file_path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".meta")
    {
        // Remove .meta extension to get the asset file path
        fs::path asset_path = file_path;
        asset_path.replace_extension("");

        // Check if the associated asset file exists
        if (fs::exists(asset_path) && fs::is_regular_file(asset_path))
            HandleFileChange(asset_path, change_type);

        return;
    }

    const AssetImporterTraits* importer = FindImporter(file_path);
    if (!importer)
        return;

    // Check if already in the queue
    auto it = std::find_if(g_importer.queue.begin(), g_importer.queue.end(),
        [&file_path](const ImportJob& job) {
            return job.source_path == file_path;
        });

    if (it != g_importer.queue.end())
        return; // Already in queue

    // Add new job to queue
    g_importer.queue.emplace_back(file_path, importer);
}

static bool ProcessImportQueue()
{
    if (g_importer.queue.empty())
        return false;

    // Get output directory from config
    auto output_dir = g_importer.config->GetString("output", "directory", "assets");

    // Convert to filesystem::path
    fs::path output_path = fs::absolute(fs::path(output_dir));

    // Ensure output directory exists
    fs::create_directories(output_path);

    std::vector<ImportJob> remaining_jobs;
    bool made_progress = true;
    bool any_imports_processed = false;

    // Keep processing until no more progress is made
    while (g_importer.running && made_progress && !g_importer.queue.empty())
    {
        made_progress = false;
        remaining_jobs.clear();

        for (const auto& job : g_importer.queue)
        {
            bool can_import_now = true;

            // Check dependencies if the importer supports it
            if (job.importer->does_depend_on)
            {
                // Check if any files this one depends on are still in the queue
                for (const auto& other_job : g_importer.queue)
                {
                    if (&job == &other_job)
                        continue; // Don't check against self

                    if (job.importer->does_depend_on(job.source_path, other_job.source_path))
                    {
                        can_import_now = false;
                        break;
                    }
                }
            }

            if (can_import_now)
            {
                // Import this file
                if (job.importer->import_func)
                {
                    try
                    {
                        // Create output stream
                        Stream* output_stream = CreateStream(nullptr, 4096);
                        if (!output_stream)
                        {
                            std::cout << job.source_path.string() << ": error: Failed to create output stream" << std::endl;
                            continue;
                        }

                        // Load .meta file or create default props
                        Props* meta = nullptr;
                        if (auto meta_stream = LoadStream(nullptr, fs::path(job.source_path.string() + ".meta")))
                        {
                            meta = Props::Load(meta_stream);
                            Free(meta_stream);
                        }

                        // Create default props if meta file failed to load
                        if (!meta)
                            meta = new Props();

                        // Call the importer
                        job.importer->import_func(job.source_path, output_stream, g_importer.config, meta);

                        delete meta;

                        // Build output file path with correct extension
                        fs::path relative_path;
                        bool found_relative = false;

                        // Get source directories from config and find the relative path
                        auto source_list = g_importer.config->GetKeys("source");
                        for (auto& source_dir_str : source_list)
                        {
                            fs::path source_dir(source_dir_str);
                            std::error_code ec;
                            relative_path = fs::relative(job.source_path, source_dir, ec);
                            if (!ec && !relative_path.empty() && relative_path.string().find("..") == std::string::npos)
                            {
                                found_relative = true;
                                break;
                            }
                        }

                        if (!found_relative)
                            relative_path = job.source_path.filename();

                        // Build final output path with extension derived from signature
                        fs::path final_path = output_path / relative_path;
                        std::string derived_extension = GetExtensionFromSignature(job.importer->signature);
                        final_path.replace_extension(derived_extension);

                        // Ensure output directory exists
                        fs::create_directories(final_path.parent_path());

                        // Save the output stream
                        if (!SaveStream(output_stream, final_path))
                        {
                            Free(output_stream);
                            throw std::runtime_error("Failed to save output file");
                        }

                        Free(output_stream);

                        // Print success message using the relative path we computed
                        fs::path asset_path = relative_path;
                        asset_path.replace_extension("");
                        std::string asset_name = asset_path.string();
                        std::replace(asset_name.begin(), asset_name.end(), '\\', '/');
                        LogInfo("Imported \033[38;2;128;128;128m%s", asset_name.c_str());

                        // todo: format nicely using TStringBuilder

                        // Broadcast hotload message
                        BroadcastAssetChange(asset_name);
                    }
                    catch (const std::exception& e)
                    {
                        char path[1024];
                        Copy(path, 1024, job.source_path.string().c_str());
                        CleanPath(path);
                        LogError("%s: %s", path, e.what());
                        continue;
                    }
                }
                made_progress = true;
                any_imports_processed = true;
            }
            else
            {
                // Keep this job for next iteration
                remaining_jobs.push_back(job);
            }
        }

        // Swap the queues - remaining_jobs becomes the new import queue
        g_importer.queue = std::move(remaining_jobs);
    }

    return any_imports_processed;
}

static void CleanupOrphanedAssets(const fs::path& output_dir)
{
    if (!fs::exists(output_dir) || !fs::is_directory(output_dir))
        return;

    // Build set of valid imported asset files by scanning source directories
    std::set<fs::path> valid_asset_files;
    
    // Get source directories from config
    auto source_list = g_importer.config->GetKeys("source");
    for (const auto& source_dir_str : source_list)
    {
        fs::path source_dir(source_dir_str);
        if (!fs::exists(source_dir) || !fs::is_directory(source_dir))
            continue;

        try
        {
            for (const auto& entry : fs::recursive_directory_iterator(source_dir))
            {
                if (!entry.is_regular_file())
                    continue;

                const fs::path& source_file = entry.path();
                std::string ext = source_file.extension().string();

                // Skip .meta files
                if (ext == ".meta")
                    continue;


                const AssetImporterTraits* importer = FindImporter(source_file);
                if (!importer)
                    continue;

                // Build expected output file path
                fs::path relative_path;
                std::error_code ec;
                relative_path = fs::relative(source_file, source_dir, ec);
                if (ec || relative_path.empty() || relative_path.string().find("..") != std::string::npos)
                    relative_path = source_file.filename();

                fs::path expected_output = output_dir / relative_path;
                std::string derived_extension = GetExtensionFromSignature(importer->signature);
                expected_output.replace_extension(derived_extension);

                valid_asset_files.insert(expected_output);
            }
        }
        catch (const std::exception& e)
        {
            LogWarning("Failed to scan source directory '%s': %s", source_dir_str.c_str(), e.what());
            continue;
        }
    }

    // Scan output directory and remove files not in the valid set
    std::vector<fs::path> files_to_remove;
    
    try
    {
        for (const auto& entry : fs::recursive_directory_iterator(output_dir))
        {
            if (!entry.is_regular_file())
                continue;

            const fs::path& output_file = entry.path();
            
            bool is_asset_file = false;
            if (Stream* stream = LoadStream(nullptr, output_file))
            {
                AssetHeader header;
                is_asset_file = ReadAssetHeader(stream, &header) && nullptr != FindImporter(header.signature);
                Free(stream);
            }

            // If it's an asset file but not in our valid set, mark for removal
            if (is_asset_file && valid_asset_files.find(output_file) == valid_asset_files.end())
                files_to_remove.push_back(output_file);
        }
    }
    catch (const std::exception& e)
    {
        LogWarning("Failed to scan output directory '%s': %s", output_dir.string().c_str(), e.what());
        return;
    }

    // Remove orphaned files
    for (const fs::path& file_to_remove : files_to_remove)
    {
        try
        {
            fs::remove(file_to_remove);
            
            // Convert to relative path for logging
            fs::path relative_removed = fs::relative(file_to_remove, output_dir);
            relative_removed.replace_extension("");
            std::string asset_name = relative_removed.string();
            std::replace(asset_name.begin(), asset_name.end(), '\\', '/');
            LogInfo("Removed orphaned asset: \033[38;2;128;128;128m%s", asset_name.c_str());
        }
        catch (const std::exception& e)
        {
            LogWarning("Failed to remove orphaned file '%s': %s", file_to_remove.string().c_str(), e.what());
        }
    }
}

static int RunImporter()
{
    if (!LoadConfig())
        return 1;

    InitFileWatcher(500);

    // Get source directories from config
    if (!g_importer.config->HasGroup("source"))
    {
        LogError("No [source] section found in config");
        ShutdownFileWatcher();
        return 1;
    }

    // Add directories to watch (file watcher will auto-start when first directory is added)
    auto source = g_importer.config->GetKeys("source");
    for (const auto& source_dir_str : source)
        if (!WatchDirectory(fs::path(source_dir_str)))
            LogWarning("Failed to add directory '%s'", source_dir_str.c_str());

    FileChangeEvent event;

    while (g_importer.running)
    {
        ThreadYield();

        while (g_importer.running && GetFileChangeEvent(&event))
            HandleFileChange(event.path, event.type);

        if (!ProcessImportQueue())
            continue;

        // Clean up orphaned asset files before generating manifest
        auto output_dir = g_importer.config->GetString("output", "directory", "assets");
        CleanupOrphanedAssets(fs::path(output_dir));

        // Generate a new asset manifest
        auto manifest_path = g_importer.config->GetString("manifest", "output_file", "src/assets.cpp");
        GenerateAssetManifest(fs::path(output_dir), fs::path(manifest_path), g_importer.importers, g_importer.config);
    }

    ShutdownFileWatcher();
    g_importer.queue.clear();
    return 0;
}

void InitImporter()
{
    assert(!g_importer.thread_running);

    g_importer.running = true;
    g_importer.thread_running = true;
    g_importer.importers = {
        GetShaderImporterTraits(),
        GetTextureImporterTraits(),
        GetFontImporterTraits(),
        GetMeshImporterTraits(),
        GetStyleSheetImporterTraits(),
        GetVfxImporterTraits(),
        GetSoundImporterTraits(),
        GetSkeletonImporterTraits()
    };

    g_importer.thread = std::make_unique<std::thread>([]
    {
        RunImporter();
        g_importer.thread_running = false;
    });
}

void ShutdownImporter()
{
    if (!g_importer.thread_running)
        return;

    g_importer.running = false;

    if (g_importer.thread && g_importer.thread->joinable())
        g_importer.thread->join();

    g_importer.thread.reset();
}
