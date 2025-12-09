//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include <utils/file_watcher.h>
#include "asset_manifest.h"

static void ExecuteJob(void* data);
extern AssetData* CreateAssetDataForImport(const std::filesystem::path& path);

namespace fs = std::filesystem;

struct ImportJob {
    AssetData* asset;
    fs::path source_path;
    fs::path meta_path;
};

struct Importer {
    std::atomic<bool> running;
    std::atomic<bool> thread_running;
    std::unique_ptr<std::thread> thread;
    std::filesystem::path manifest_path;
    std::mutex mutex;
    std::vector<JobHandle> jobs;
    std::vector<ImportEvent> import_events;
    JobHandle post_import_job;
};

static Importer g_importer = {};

static const AssetImporter* FindImporter(const fs::path& ext) {
    for (int i=0; i<ASSET_TYPE_COUNT; i++) {
        AssetImporter* importer = &g_editor.importers[i];
        if (ext == importer->ext)
            return importer;
    }

    return nullptr;
}

bool InitImporter(AssetData* a) {
    fs::path path = a->path;
    if (!fs::exists(path))
        return false;

    const AssetImporter* importer = FindImporter(path.extension());
    if (!importer)
        return false;

    a->importer = importer;
    a->type = importer->type;
    return true;
}

static void QueueImport(AssetData* a) {
    fs::path path = a->path;
    if (!fs::exists(path))
        return;

    if (!a->importer)
        return;

    fs::path target_path = GetTargetPath(a);
    fs::path source_meta_path = path;
    source_meta_path += ".meta";

    bool target_exists = fs::exists(target_path);
    bool meta_changed = !target_exists || (fs::exists(source_meta_path) && CompareModifiedTime(source_meta_path, target_path) > 0);
    bool source_changed = !target_exists || CompareModifiedTime(path, target_path) > 0;
    bool config_changed = !target_exists || CompareModifiedTime(g_editor.config_timestamp, fs::last_write_time(target_path)) > 0;

    if (!meta_changed && !source_changed && !config_changed)
        return;

    std::lock_guard lock(g_importer.mutex);
    g_importer.jobs.push_back(CreateJob(ExecuteJob, new ImportJob{
        .asset = a,
        .source_path = fs::path(path).make_preferred(),
        .meta_path = source_meta_path.make_preferred()
    }, g_importer.post_import_job));
}

void QueueImport(const fs::path& path) {
    const AssetImporter* importer = FindImporter(path.extension());
    if (!importer)
        return;

    const Name* asset_name = MakeCanonicalAssetName(fs::path(path));
    if (!asset_name)
        return;

    AssetData* a = GetAssetData(importer->type, asset_name);
    if (!a) {
        a = CreateAssetDataForImport(path);
        if (!a) return;
    }

    QueueImport(a);
}

static void HandleFileChangeEvent(const FileChangeEvent& event) {
    if (event.type == FILE_CHANGE_TYPE_DELETED)
        return;

    fs::path source_ext = event.path.extension();
    if (source_ext == ".meta") {
        fs::path target_path = event.path;
        target_path.replace_extension("");
        QueueImport(target_path);
    } else {
        QueueImport(event.path);
    }
}

static void ExecuteJob(void* data) {
    ImportJob* job = (ImportJob*)data;
    std::unique_ptr<ImportJob> job_guard(job);

    if (!fs::exists(job->source_path))
        return;

    Props* meta = nullptr;
    std::filesystem::path meta_path = job->source_path;
    meta_path += ".meta";
    if (auto meta_stream = LoadStream(nullptr, meta_path)) {
        meta = Props::Load(meta_stream);
        Free(meta_stream);
    }

    if (!meta)
        meta = new Props();

    std::unique_ptr<Props> meta_guard(meta);

    fs::path target_dir =
        g_editor.output_dir /
        ToString(job->asset->importer->type) /
        job->asset->name->value;

    std::string target_dir_lower = target_dir.string();
    Lowercase(target_dir_lower.data(), (u32)target_dir_lower.size());

    job->asset->importer->import_func(job->asset, target_dir_lower, g_config, meta);


    if (g_editor.unity) {
        fs::path unity_dir =
            g_editor.unity_path /
            ToString(job->asset->importer->type) /
            (std::string(job->asset->name->value) + ".noz");

        //SaveStream(target_stream, unity_dir);
    }

    // todo: Check if any other assets depend on this and if so requeue them

    g_importer.mutex.lock();
    g_importer.import_events.push_back({
        .name =  job->asset->name,
        .type = job->asset->importer->type
    });
    g_importer.mutex.unlock();
}

static void CleanupOrphanedAssets() {
    std::set<fs::path> source_paths;
    for (u32 i=0, c=GetAssetCount(); i<c; i++)
        source_paths.insert(GetTargetPath(GetAssetData(i)));

    // todo: this was deleting the glsl files, etc
#if 0
    std::vector<fs::path> target_paths;
    GetFilesInDirectory(g_editor.output_dir, target_paths);

    for (const fs::path& target_path : target_paths) {
        if (!source_paths.contains(target_path)) {
            fs::remove(target_path);
            AddNotification(NOTIFICATION_TYPE_INFO, "Removed '%s'", target_path.filename().string().c_str());
        }
    }
#endif
}

static void PostImportJob(void *data) {
    (void)data;

    GenerateAssetManifest(g_editor.output_dir, g_importer.manifest_path, g_config);
}

static bool UpdateJobs() {
    std::lock_guard lock(g_importer.mutex);
    int old_job_count = (int)g_importer.jobs.size();
    if (!IsDone(g_importer.post_import_job))
        return true;

    if (old_job_count == 0)
        return false;

    for (int i=0; i<g_importer.jobs.size(); ) {
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

void WaitForImportJobs() {
    while (g_importer.running && UpdateJobs())
        ThreadYield();
}

static void InitialImport() {
    for (u32 i=0, c=GetAssetCount(); i<c; i++)
        QueueImport(GetAssetData(i));

    WaitForImportJobs();
    CleanupOrphanedAssets();
}

static void RunImporter() {
    if (g_editor.asset_path_count == 0)
        return;

    // Initialize file watcher
    const char* dirs[MAX_ASSET_PATHS];
    for (int p=0; p<g_editor.asset_path_count; p++)
        dirs[p] = g_editor.asset_paths[p];
    dirs[g_editor.asset_path_count] = nullptr;
    InitFileWatcher(500, dirs);

    while (g_importer.running) {
        ThreadYield();

        FileChangeEvent event;
        while (g_importer.running && GetFileChangeEvent(&event))
            HandleFileChangeEvent(event);
    }

    ShutdownFileWatcher();
}

void UpdateImporter() {
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

    SortAssets();
    CleanupOrphanedAssets();

    for (const ImportEvent& event : events)
        Send(EDITOR_EVENT_IMPORTED, &event);
}

const std::filesystem::path& GetManifestPath() {
    return g_importer.manifest_path;
}

void InitImporter() {
    assert(!g_importer.thread_running);

    g_importer.running = true;
    g_importer.thread_running = true;
    g_importer.manifest_path = g_config->GetString("manifest", "output_file", "src/assets.cpp");
    g_importer.thread = std::make_unique<std::thread>([] {
        RunImporter();
        g_importer.thread_running = false;
    });

    InitialImport();
}

void ShutdownImporter() {
    if (!g_importer.thread_running)
        return;

    g_importer.running = false;

    if (g_importer.thread && g_importer.thread->joinable())
        g_importer.thread->join();

    g_importer.thread.reset();
}
