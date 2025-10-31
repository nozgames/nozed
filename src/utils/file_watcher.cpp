//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "file_watcher.h"

namespace fs = std::filesystem;

struct FileInfo {
    fs::path path;
    fs::path relative_path;
    fs::path watch_path;
    fs::file_time_type time;
    uint64_t size;
    bool exists;
    uint64_t hash;
};

struct FileWatcher {
    int poll_interval_ms;
    std::vector<fs::path> watched_dirs;
    std::map<fs::path, FileInfo> file_map;
    std::vector<FileChangeEvent> event_queue;
    std::mutex mutex;
    std::thread thread;
    std::atomic<bool> running;
};

static FileWatcher g_watcher = {};

static void QueueEvent(const FileInfo& file_info, FileChangeType type) {
    std::lock_guard lock(g_watcher.mutex);
    g_watcher.event_queue.push_back({
        .path = file_info.path,
        .relative_path = file_info.relative_path,
        .watch_path = file_info.watch_path,
        .type = type,
    });
}

static void AddFile(const fs::path& watch_path, const fs::path& path) {
    FileInfo& file_info = g_watcher.file_map[path];
    file_info = {
        .path = path,
        .relative_path = fs::relative(path, watch_path),
        .watch_path = watch_path,
        .time = fs::last_write_time(path),
        .size = fs::file_size(path),
        .exists = true
    };

    std::string temp_path = file_info.path.string();
    std::ranges::replace(temp_path, '\\', '/');
    file_info.path = temp_path;

    std::string temp_relative_path = file_info.relative_path.string();
    std::ranges::replace(temp_relative_path, '\\', '/');
    file_info.relative_path = temp_relative_path;
}

static void ProcessFile(const fs::path& watch_path, const fs::path& path)
{
    auto it = g_watcher.file_map.find(path);
    if (it == g_watcher.file_map.end())
    {
        AddFile(watch_path, path);
        QueueEvent(g_watcher.file_map[path], FILE_CHANGE_TYPE_ADDED);
        return;
    }

    if (!fs::exists(path))
        return;
    
    size_t file_size = fs::file_size(path);
    fs::file_time_type file_time = fs::last_write_time(path);

    FileInfo& existing = it->second;
    existing.exists = true;

    if (file_size == existing.size && file_time == existing.time)
        return;

    if (file_size == existing.size && existing.hash != 0) {
        uint64_t hash = HashFile(path);
        if (hash == existing.hash)
            return;
    }

    existing.time = file_time;
    existing.size = file_size;

    QueueEvent(existing, FILE_CHANGE_TYPE_MODIFIED);
}

static void ScanDirectory(const fs::path& dir_path, void (*process_file)(const fs::path&, const fs::path&))
{
    assert(process_file);
    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(dir_path, ec))
    {
        if (!entry.is_regular_file(ec) || ec)
            continue;

        process_file(dir_path, entry.path());
    }
}

bool GetFileChangeEvent(FileChangeEvent* event)
{
    assert(event);

    if (!g_watcher.running)
        return false;

    std::lock_guard lock(g_watcher.mutex);
    if (g_watcher.event_queue.empty())
        return false;

    *event = g_watcher.event_queue[0];
    g_watcher.event_queue.erase(g_watcher.event_queue.begin() + 0);
    return true;
}

static void FileWatcherThread()
{
    // Add intial file list before sending changed events
    for (const auto& dir : g_watcher.watched_dirs)
        ScanDirectory(fs::path(dir).make_preferred(), AddFile);

    std::chrono::steady_clock::time_point end =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(g_watcher.poll_interval_ms);

    while (g_watcher.running)
    {
        if (std::chrono::steady_clock::now() < end)
        {
            ThreadYield();
            continue;
        }

        for (auto& pair : g_watcher.file_map)
            pair.second.exists = false;

        for (const auto& dir : g_watcher.watched_dirs)
            ScanDirectory(fs::path(dir).make_preferred(), ProcessFile);

        // Handle deletions
        auto it = g_watcher.file_map.begin();
        while (it != g_watcher.file_map.end())
        {
            if (it->second.exists)
            {
                ++it;
                continue;
            }

            QueueEvent(it->second, FILE_CHANGE_TYPE_DELETED);
            it = g_watcher.file_map.erase(it);
        }

        end = std::chrono::steady_clock::now() + std::chrono::milliseconds(g_watcher.poll_interval_ms);
        ThreadYield();
    }
}

void InitFileWatcher(int poll_interval_ms, const char** dirs)
{
    assert(!g_watcher.running);
    assert(dirs);

    for (; *dirs; dirs++)
        g_watcher.watched_dirs.push_back(*dirs);

    g_watcher.running = true;
    g_watcher.poll_interval_ms = poll_interval_ms > 0 ? poll_interval_ms : 1000;
    g_watcher.file_map.clear();
    g_watcher.thread = std::thread(FileWatcherThread);
}

void ShutdownFileWatcher()
{
    if (!g_watcher.running)
        return;

    g_watcher.running = false;
    g_watcher.thread.join();
    g_watcher.watched_dirs.clear();
    g_watcher.file_map.clear();
}
