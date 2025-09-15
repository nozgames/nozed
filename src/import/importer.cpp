//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include <editor.h>
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
    std::vector<const Name*> imported_files;
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

#if 0
static const AssetImporterTraits* FindImporter(AssetSignature signature)
{
    for (const auto* importer : g_importer.importers)
        if (importer && importer->signature == signature)
            return importer;

    return nullptr;
}
#endif

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

    g_importer.mutex.lock();
    g_importer.imported_files.push_back(job->source_name);
    g_importer.mutex.unlock();
}

static void CleanupOrphanedAssets()
{
    // todo: handle deleted files.
}

static void PostImportJob(void *data)
{
    (void)data;

    GenerateAssetManifest(g_importer.output_dir, g_importer.manifest_path, g_importer.importers, g_config);
    CleanupOrphanedAssets();
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
    }

    ShutdownFileWatcher();
}

void UpdateImporter()
{
    if (UpdateJobs())
        return;

    g_importer.mutex.lock();
    if (g_importer.imported_files.empty())
    {
        g_importer.mutex.unlock();
        return;
    }

    std::vector<const Name*> names = std::move(g_importer.imported_files);
    g_importer.mutex.unlock();

    for (const Name* name : names)
        Send(EDITOR_EVENT_IMPORTED, name);
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
