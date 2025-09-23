//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include <editor.h>
#include <utils/file_watcher.h>
#include "asset_manifest.h"

namespace fs = std::filesystem;

struct ImportJob
{
    EditorAsset* ea;
    fs::path source_path;
    fs::path source_relative_path;
    fs::path source_meta_path;
    const Name* source_name;
    fs::path target_path;
    fs::path target_short_path;
    const AssetImporter* importer;
    bool force;
};

struct Importer
{
    std::atomic<bool> running;
    std::atomic<bool> thread_running;
    std::queue<ImportJob> queue;
    std::unique_ptr<std::thread> thread;
    std::filesystem::path output_dir;
    std::filesystem::path manifest_path;
    std::mutex mutex;
    std::vector<JobHandle> jobs;
    std::vector<ImportEvent> import_events;
    JobHandle post_import_job;
};

static Importer g_importer = {};

static void ExecuteJob(void* data);

static const AssetImporter* FindImporter(const fs::path& ext)
{
    for (int i=0; i<EDITOR_ASSET_TYPE_COUNT; i++)
    {
        AssetImporter* importer = &g_editor.importers[i];
        if (ext == importer->ext)
            return importer;
    }

    return nullptr;
}

#if 0
static const AssetImporter* FindImporter(AssetSignature signature)
{
    for (const auto* importer : g_importer.importers)
        if (importer && importer->signature == signature)
            return importer;

    return nullptr;
}
#endif

const char* GetVarTypeNameFromSignature(AssetSignature signature)
{
    return GetTypeNameFromSignature(signature);
}

static void QueueImport(EditorAsset* ea, bool force)
{
    fs::path path = ea->path;
    if (!fs::exists(path))
        return;

    const AssetImporter* importer = FindImporter(path.extension());
    if (!importer)
        return;

    std::string type_name_lower = GetVarTypeNameFromSignature(importer->signature);
    Lowercase(type_name_lower.data(), (u32)type_name_lower.size());

    fs::path source_relative_path = fs::relative(path, g_editor.asset_paths[ea->asset_path_index]);
    fs::path target_short_path = type_name_lower / GetSafeFilename(source_relative_path.filename().string().c_str());
    fs::path target_path = g_importer.output_dir / target_short_path;
    fs::path source_meta_path = path;
    source_meta_path += ".meta";
    target_path.replace_extension("");
    target_short_path.replace_extension("");

    bool target_exists = fs::exists(target_path);
    bool meta_changed = !target_exists || (fs::exists(source_meta_path) && CompareModifiedTime(source_meta_path, target_path) > 0);
    bool source_changed = !target_exists || CompareModifiedTime(path, target_path) > 0;
    bool config_changed = CompareModifiedTime(g_editor.config_timestamp, fs::last_write_time(target_path)) > 0;

    if (!force && !meta_changed && !source_changed && !config_changed)
        return;

    fs::path source_name = source_relative_path;
    source_name.replace_extension("");

    std::lock_guard lock(g_importer.mutex);
    g_importer.jobs.push_back(CreateJob(ExecuteJob, new ImportJob{
        .ea = ea,
        .source_path = fs::path(path).make_preferred(),
        .source_relative_path = source_relative_path.make_preferred(),
        .source_meta_path = source_meta_path.make_preferred(),
        .source_name = GetName(source_name.string().c_str()),
        .target_path = target_path.make_preferred(),
        .target_short_path = target_short_path,
        .importer = importer,
        .force = force
    }, g_importer.post_import_job));
}

static void HandleFileChangeEvent(const FileChangeEvent& event)
{
    if (event.type == FILE_CHANGE_TYPE_DELETED)
        return;

#if 0
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

    job->importer->import_func(job->ea, target_stream, g_config, meta);

    bool result = SaveStream(target_stream, job->target_path);
    Free(target_stream);

    if (!result)
        throw std::runtime_error("Failed to save output file");

    // todo: Check if any other assets depend on this and if so requeue them

    g_importer.mutex.lock();
    g_importer.import_events.push_back({
        .name =  job->source_name,
        .target_path = job->target_short_path
    });
    g_importer.mutex.unlock();
}

static void CleanupOrphanedAssets()
{
    // todo: handle deleted files.
}

static void PostImportJob(void *data)
{
    (void)data;

    GenerateAssetManifest(g_importer.output_dir, g_importer.manifest_path, g_config);
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
    for (int i=0; i<MAX_ASSETS; i++)
    {
        EditorAsset* ea = GetEditorAsset(i);
        if (!ea)
            continue;

        QueueImport(ea, false);
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
    if (g_importer.import_events.empty())
    {
        g_importer.mutex.unlock();
        return;
    }

    std::vector<ImportEvent> events = std::move(g_importer.import_events);
    g_importer.mutex.unlock();

    for (const ImportEvent& event : events)
        Send(EDITOR_EVENT_IMPORTED, &event);
}

void InitImporter()
{
    assert(!g_importer.thread_running);

    g_importer.running = true;
    g_importer.thread_running = true;
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
