//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include <editor.h>
#include <server.h>
#include <utils/file_watcher.h>
#include "asset_manifest.h"

namespace fs = std::filesystem;

extern AssetImporterTraits* GetShaderImporterTraits();
extern AssetImporterTraits* GetTextureImporterTraits();
extern AssetImporterTraits* GetFontImporterTraits();
extern AssetImporterTraits* GetMeshImporterTraits();
extern AssetImporterTraits* GetStyleSheetImporterTraits();
extern AssetImporterTraits* GetVfxImporterTraits();
extern AssetImporterTraits* GetSoundImporterTraits();
extern AssetImporterTraits* GetSkeletonImporterTraits();
extern AssetImporterTraits* GetAnimationImporterTraits();

struct ImportJob
{
    fs::path source_path;
    fs::path source_relative_path;
    const Name* source_name;
    fs::path target_path;
    const AssetImporterTraits* importer;
    bool force;
};

struct Importer
{
    std::atomic<bool> running;
    std::atomic<bool> thread_running;
    std::queue<ImportJob> queue;
    std::unique_ptr<std::thread> thread;
    std::vector<AssetImporterTraits*> importers;
    std::filesystem::path output_dir;
    std::filesystem::path manifest_path;
    std::mutex mutex;
    std::vector<JobHandle> jobs;
};

static Importer g_importer = {};

static bool ProcessImportQueue();
static void ExecuteJob(void* data);

static const AssetImporterTraits* FindImporter(const fs::path& ext)
{
    for (auto* importer : g_importer.importers)
        if (ext == importer->ext)
            return importer;

    return nullptr;
}

static const AssetImporterTraits* FindImporter(AssetSignature signature)
{
    for (const auto* importer : g_importer.importers)
        if (importer && importer->signature == signature)
            return importer;

    return nullptr;
}

static void QueueImport(const fs::path& source_path, const fs::path& assets_path, bool force)
{
    if (!fs::exists(source_path))
        return;

    const AssetImporterTraits* importer = FindImporter(source_path.extension());
    if (!importer)
        return;

    fs::path source_relative_path = fs::relative(source_path, assets_path);
    fs::path target_path = g_importer.output_dir / source_relative_path;
    target_path.replace_extension(GetExtensionFromSignature(importer->signature));

    if (!force && fs::exists(target_path) && CompareModifiedTime(source_path, target_path) <= 0)
        return;

    fs::path source_name = source_relative_path;
    source_name.replace_extension("");

    std::lock_guard lock(g_importer.mutex);
    g_importer.jobs.push_back(CreateJob(ExecuteJob, new ImportJob{
        .source_path = fs::path(source_path).make_preferred(),
        .source_relative_path = source_relative_path.make_preferred(),
        .source_name = GetName(source_name.string().c_str()),
        .target_path = target_path.make_preferred(),
        .importer = importer,
        .force = force
    }));
}

void HandleFileChange(const FileChangeEvent& event)
{
    (void)event;

#if 0
    if (event.type == FILE_CHANGE_TYPE_DELETED)
        return;

    fs::path ext = event.path.extension();
    if (ext == ".meta")
    {
        // Remove .meta extension to get the asset file path
        fs::path asset_path = event.path;
        asset_path.replace_extension("");

        // Check if the associated asset file exists
        if (fs::exists(asset_path) && fs::is_regular_file(asset_path))
        {
            fs::path relative_asset_path = event.relative_path;
            relative_asset_path.replace_extension("");

            FileChangeEvent asset_event = {
                .path = asset_path,
                .relative_path = relative_asset_path,
                .type = event.type
            };
            HandleFileChange(asset_event);
        }

        return;
    }

    const AssetImporterTraits* importer = FindImporter(ext);
    if (!importer)
        return;

    g_importer.queue.push_back({
        .path = event.path,
        .relative_path = event.relative_path,
        .importer = importer
    });
#endif
}

static void ExecuteJob(void* data)
{
    ImportJob* job = (ImportJob*)data;
    std::unique_ptr<ImportJob> job_guard(job);

    if (!fs::exists(job->source_path))
        return;

    Stream* target_stream = CreateStream(nullptr, 4096);
    if (!target_stream)
        return;

    Props* meta = nullptr;
    std::filesystem::path meta_path = job->source_path;
    meta_path += ".meta";
    if (auto meta_stream = LoadStream(nullptr, meta_path))
    {
        meta = Props::Load(meta_stream);
        Free(meta_stream);
    }

    if (!meta)
        meta = new Props();

    std::unique_ptr<Props> meta_guard(meta);

    job->importer->import_func(job->source_path, target_stream, g_config, meta);

    bool result = SaveStream(target_stream, job->target_path);
    Free(target_stream);

    if (!result)
        throw std::runtime_error("Failed to save output file");

    // todo: Check if any other assets depend on this and if so requeue them

    // Print success message using the relative path we computed
    fs::path asset_path = job->source_relative_path;
    asset_path.replace_extension("");
    LogInfo("Imported \033[38;2;128;128;128m%s", asset_path.c_str());
}

static int ProcessFinishedJobs()
{
    std::lock_guard lock(g_importer.mutex);
    for (int i=0; i<g_importer.jobs.size(); )
    {
        if (IsDone(g_importer.jobs[i]))
            g_importer.jobs.erase(g_importer.jobs.begin() + i);
        else
            i++;
    }

    return (int)g_importer.jobs.size();
}

static void WaitForJobs()
{
    while (g_importer.running)
    {
        int count = ProcessFinishedJobs();
        if (count <= 0)
            break;

        ThreadYield();
    }
}

static void InitialImport()
{
    std::error_code ec;
    for (int i=0; i<g_editor.asset_path_count; i++)
    {
        fs::path assets_path(g_editor.asset_paths[i]);
        for (const auto& entry : std::filesystem::recursive_directory_iterator(assets_path, ec))
        {
            if (!entry.is_regular_file(ec) || ec)
                continue;

            QueueImport(entry.path(), assets_path, false);
        }
    }

    WaitForJobs();
}

static bool ProcessImportQueue()
{
    if (g_importer.queue.empty())
        return false;

    std::vector<ImportJob> remaining_jobs;
    bool made_progress = true;
    bool any_imports_processed = false;

    while (g_importer.running && made_progress && !g_importer.queue.empty())
    {
        // todo: start a job for each asset
        // todo: when an asset is imported we check if anything depends on it and if so requeue those

        // for (const auto& job : g_importer.queue)
        // {
        //
        //
        //
        //
        // }




#if 0
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
                        job.importer->import_func(job.source_path, output_stream, g_config, meta);

                        delete meta;

                        // Build output file path with correct extension
                        fs::path relative_path;
                        bool found_relative = false;

                        // Get source directories from config and find the relative path
                        for (int p=0; p<g_editor.asset_path_count; p++)
                        {
                            fs::path source_dir(g_editor.asset_paths[p]);
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
                        fs::path final_path = g_importer.output_dir / relative_path;
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
#endif
    }

    return any_imports_processed;
}

static void CleanupOrphanedAssets()
{
    // Build set of valid imported asset files by scanning source directories
    std::set<fs::path> valid_asset_files;
    
    // Get source directories from config
    for (int p=0; p<g_editor.asset_path_count; p++)
    {
        fs::path source_dir(g_editor.asset_paths[p]);
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

                fs::path expected_output = g_importer.output_dir / relative_path;
                std::string derived_extension = GetExtensionFromSignature(importer->signature);
                expected_output.replace_extension(derived_extension);

                valid_asset_files.insert(expected_output);
            }
        }
        catch (const std::exception&)
        {
            continue;
        }
    }

    // Scan output directory and remove files not in the valid set
    std::vector<fs::path> files_to_remove;
    
    try
    {
        for (const auto& entry : fs::recursive_directory_iterator(g_importer.output_dir))
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
    catch (const std::exception&)
    {
        return;
    }

    // Remove orphaned files
    for (const fs::path& file_to_remove : files_to_remove)
    {
        try
        {
            fs::remove(file_to_remove);
            
            // Convert to relative path for logging
            fs::path relative_removed = fs::relative(file_to_remove, g_importer.output_dir);
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

static void RunImporter()
{
    if (g_editor.asset_path_count == 0)
        return;

    // Initialize file watcher
    const char* dirs[MAX_ASSET_PATHS];
    for (int p=0; p<g_editor.asset_path_count; p++)
        dirs[p] = g_editor.asset_paths[p];
    dirs[g_editor.asset_path_count] = nullptr;
    InitFileWatcher(500, dirs);

    while (g_importer.running)
    {
        ThreadYield();

        FileChangeEvent event;
        while (g_importer.running && GetFileChangeEvent(&event))
            HandleFileChange(event);

        if (!ProcessImportQueue())
            continue;

        CleanupOrphanedAssets();
        GenerateAssetManifest(g_importer.output_dir, g_importer.manifest_path, g_importer.importers, g_config);
    }

    ShutdownFileWatcher();
    //g_importer.queue.clear();
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
        GetSkeletonImporterTraits(),
        GetAnimationImporterTraits()
    };

    g_importer.output_dir = fs::absolute(fs::path(g_config->GetString("output", "directory", "assets")));
    g_importer.manifest_path = g_config->GetString("manifest", "output_file", "src/assets.cpp");

    fs::create_directories(g_importer.output_dir);

    g_importer.thread = std::make_unique<std::thread>([]
    {
        RunImporter();
        g_importer.thread_running = false;
    });

    InitialImport();
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
