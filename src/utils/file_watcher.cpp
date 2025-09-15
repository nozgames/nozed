//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "file_watcher.h"

struct FileInfo
{
    std::filesystem::path path;
    std::filesystem::path relative_path;
    uint64_t modified_time;
    uint64_t size;
    bool exists;
};

struct FileWatcher
{
    int poll_interval_ms;
    std::vector<std::filesystem::path> watched_dirs;
    std::map<std::filesystem::path, FileInfo> file_map;
    std::vector<FileChangeEvent> event_queue;
    std::mutex mutex;
    std::thread thread;
    std::atomic<bool> running;
};

static FileWatcher g_watcher = {};

static void QueueEvent(const FileInfo& file_info, FileChangeType type)
{
    std::lock_guard lock(g_watcher.mutex);
    g_watcher.event_queue.push_back({
        .path = file_info.path,
        .relative_path = file_info.relative_path,
        .type = type,
        .timestamp = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()) });
}

static void ProcessFile(const std::filesystem::path& dir, const std::filesystem::path& path, uint64_t modified_time, uint64_t size)
{
    auto it = g_watcher.file_map.find(path);
    if (it == g_watcher.file_map.end())
    {
        FileInfo& file_info = g_watcher.file_map[path];
        file_info = {
            .path = path,
            .relative_path = std::filesystem::relative(path, dir),
            .modified_time = modified_time,
            .size = size
        };

        std::string temp_path = file_info.path.string();
        std::ranges::replace(temp_path, '\\', '/');
        file_info.path = temp_path;

        std::string temp_relative_path = file_info.relative_path.string();
        std::ranges::replace(temp_relative_path, '\\', '/');
        file_info.relative_path = temp_relative_path;

        QueueEvent(file_info, FILE_CHANGE_TYPE_ADDED);
        return;
    }

    FileInfo& existing = it->second;
    uint64_t time_diff = existing.modified_time > modified_time ?
        existing.modified_time - modified_time :
        modified_time - existing.modified_time;

    if (time_diff > 2 || existing.size != size)
    {
        existing.modified_time = modified_time;
        existing.size = size;
        QueueEvent(existing, FILE_CHANGE_TYPE_MODIFIED);
    }

    existing.exists = true;
}

static void ScanDirectory(const std::filesystem::path& dir_path)
{
    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir_path, ec))
    {
        if (!entry.is_regular_file(ec) || ec)
            continue;

        auto file_time = entry.last_write_time(ec);
        if (ec)
            continue;

        auto size = entry.file_size(ec);
        if (ec)
            continue;

        // Convert filesystem time to timestamp
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            file_time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(sctp.time_since_epoch()).count();

        ProcessFile(dir_path, entry.path(), timestamp, size);
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
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    while (g_watcher.running)
    {
        if (std::chrono::steady_clock::now() < end)
        {
            ThreadYield();
            continue;
        }

        for (auto& pair : g_watcher.file_map)
            pair.second.exists = false;

        std::vector<std::filesystem::path> dirs_copy = g_watcher.watched_dirs;
        for (const auto& dir : dirs_copy)
            ScanDirectory(dir.string().c_str());

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
