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
    fs::path source_meta_path;
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
    JobHandle post_import_job;
};

static Importer g_importer = {};

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
    fs::path source_meta_path = source_path;
    source_meta_path += ".meta";
    target_path.replace_extension(GetExtensionFromSignature(importer->signature));

    bool target_exists = fs::exists(target_path);
    bool meta_changed = !target_exists || (fs::exists(source_meta_path) && CompareModifiedTime(source_meta_path, target_path) > 0);
    bool source_changed = !target_exists || CompareModifiedTime(source_path, target_path) > 0;

    if (!force && !meta_changed && !source_changed)
        return;

    fs::path source_name = source_relative_path;
    source_name.replace_extension("");

    std::lock_guard lock(g_importer.mutex);
    g_importer.jobs.push_back(CreateJob(ExecuteJob, new ImportJob{
        .source_path = fs::path(source_path).make_preferred(),
        .source_relative_path = source_relative_path.make_preferred(),
        .source_meta_path = source_meta_path.make_preferred(),
        .source_name = GetName(source_name.string().c_str()),
        .target_path = target_path.make_preferred(),
        .importer = importer,
        .force = force
    }, g_importer.post_import_job));
}

static void HandleFileChangeEvent(const FileChangeEvent& event)
{
    if (event.type == FILE_CHANGE_TYPE_DELETED)
        return;

    fs::path source_path = event.path;
    fs::path source_ext = source_path.extension();
    if (source_ext == ".meta")
    {
        fs::path target_path = event.path;
        target_path.replace_extension("");
        QueueImport(target_path, event.watch_path, false);
    }
    else
        QueueImport(event.path, event.watch_path, false);
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

static void PostImportJob(void *data)
{
    (void)data;

    GenerateAssetManifest(g_importer.output_dir, g_importer.manifest_path, g_importer.importers, g_config);

    // todo: cleanup unused assets
}

static bool UpdateJobs()
{
    std::lock_guard lock(g_importer.mutex);
    int old_job_count = (int)g_importer.jobs.size();
    if (!IsDone(g_importer.post_import_job))
        return true;

    if (old_job_count == 0)
        return false;

    for (int i=0; i<g_importer.jobs.size(); )
    {
        if (IsDone(g_importer.jobs[i]))
            g_importer.jobs.erase(g_importer.jobs.begin() + i);
        else
            i++;
    }

    int new_job_count = (int)g_importer.jobs.size();
    if (new_job_count > 0)
        return true;

    assert(IsDone(g_importer.post_import_job));
    g_importer.post_import_job = CreateJob(PostImportJob);
    return true;
}

static void WaitForJobs()
{
    while (g_importer.running && UpdateJobs())
        ThreadYield();
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

static void CleanupOrphanedAssets()
{
    if (g_importer.running)
        return;

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
            HandleFileChangeEvent(event);

        if (!g_importer.running)
            break;

        CleanupOrphanedAssets();
    }

    ShutdownFileWatcher();
    //g_importer.queue.clear();
}

void UpdateImporter()
{
    UpdateJobs();
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
