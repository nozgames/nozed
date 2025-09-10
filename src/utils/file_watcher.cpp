//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "file_watcher.h"
#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <thread>

#ifndef nullptr
#define nullptr NULL
#endif

#define MAX_WATCHED_DIRS 32
#define MAX_EVENTS_QUEUE 4096
#define MAX_TRACKED_FILES 2048

struct FileInfo
{
    std::filesystem::path path;
    uint64_t modified_time;
    uint64_t size;
};

struct FileQueue
{
    FileChangeEvent events[MAX_EVENTS_QUEUE];
    size_t head;
    size_t tail;
    size_t count;
    std::mutex mutex;
};

// Global file watcher state
struct FileWatcher
{
    int poll_interval_ms;
    std::vector<std::filesystem::path> watched_dirs;
    std::map<std::string, FileInfo> file_map;
    FileQueue queue;
    std::thread thread;
    std::atomic<bool> should_stop;
    std::mutex dirs_mutex;
    bool initialized;
    bool running;
};

static FileWatcher g_watcher;

static void FileWatcherThread();
static void scan_directory_recursive(const char* dir_path);
static void process_file(const char* file_path, uint64_t modified_time, uint64_t size);
static void queue_event(const std::filesystem::path& path, FileChangeType type);
static bool StartFileWatcher();
static void scan_directory_files(const std::filesystem::path& dir_path);

void InitFileWatcher(int poll_interval_ms)
{
    if (g_watcher.initialized)
        return;

    g_watcher.poll_interval_ms = poll_interval_ms > 0 ? poll_interval_ms : 1000;
    g_watcher.watched_dirs.clear();
    g_watcher.file_map.clear();
    g_watcher.queue.head = 0;
    g_watcher.queue.tail = 0;
    g_watcher.queue.count = 0;
    g_watcher.should_stop = false;
    g_watcher.initialized = true;
    g_watcher.running = false;
}

void ShutdownFileWatcher()
{
    if (!g_watcher.initialized)
        return;

    g_watcher.should_stop = true;

    if (g_watcher.thread.joinable())
        g_watcher.thread.join();

    g_watcher.watched_dirs.clear();
    g_watcher.file_map.clear();
    g_watcher.initialized = false;
}

bool WatchDirectory(const std::filesystem::path& directory)
{
    if (!g_watcher.initialized || directory.empty())
        return false;

    std::filesystem::path full_path = std::filesystem::current_path() / directory;
    std::string dir_str = full_path.string();
    std::lock_guard lock(g_watcher.dirs_mutex);
    
    auto it = std::find(g_watcher.watched_dirs.begin(), g_watcher.watched_dirs.end(), full_path);
    if (it != g_watcher.watched_dirs.end())
        return true;

    if (g_watcher.watched_dirs.size() >= MAX_WATCHED_DIRS)
        return false;

    g_watcher.watched_dirs.push_back(full_path);
    
    if (!g_watcher.running && g_watcher.watched_dirs.size() == 1)
        StartFileWatcher();
    else if (g_watcher.running)
        scan_directory_recursive(dir_str.c_str());

    return true;
}

static bool StartFileWatcher()
{
    if (!g_watcher.initialized || g_watcher.running)
        return false;

    if (g_watcher.watched_dirs.empty())
        return false;  // No directories to watch

    // Do initial scan of all directories (temporarily without locking to debug)
    std::vector<std::filesystem::path> dirs_copy = g_watcher.watched_dirs;
    
    for (const auto& dir : dirs_copy)
    {
        scan_directory_recursive(dir.string().c_str());
    }
    
    // Start the watching thread
    g_watcher.should_stop = false;
    g_watcher.thread = std::thread(FileWatcherThread);
    
    if (!g_watcher.thread.joinable())
    {
        return false;
    }
    
    g_watcher.running = true;
    return true;
}

bool GetFileChangeEvent(FileChangeEvent* event)
{
    if (!g_watcher.initialized || !event)
        return false;

    std::lock_guard lock(g_watcher.queue.mutex);
    
    if (g_watcher.queue.count == 0)
        return false;

    *event = g_watcher.queue.events[g_watcher.queue.head];
    g_watcher.queue.head = (g_watcher.queue.head + 1) % MAX_EVENTS_QUEUE;
    g_watcher.queue.count--;
    
    return true;
}

static void FileWatcherThread()
{
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    while (!g_watcher.should_stop)
    {
        if (std::chrono::steady_clock::now() < end)
        {
            ThreadYield();
            continue;
        }

        // Mark all existing files as "not seen"
        for (auto& pair : g_watcher.file_map)
            pair.second.size = 0;

        std::vector<std::filesystem::path> dirs_copy = g_watcher.watched_dirs;
        for (const auto& dir : dirs_copy)
            scan_directory_recursive(dir.string().c_str());

        // Check for deleted files (files that weren't seen in this pass)
        auto it = g_watcher.file_map.begin();
        while (it != g_watcher.file_map.end())
        {
            if (it->second.size == 0)
            {
                // File was not seen in this pass, it was deleted
                queue_event(it->second.path, FILE_CHANGE_TYPE_DELETED);
                it = g_watcher.file_map.erase(it);
            }
            else
            {
                ++it;
            }
        }

        end = std::chrono::steady_clock::now() + std::chrono::milliseconds(g_watcher.poll_interval_ms);
        ThreadYield();
    }
}

static void scan_directory_files(const std::filesystem::path& dir_path)
{
    std::error_code ec;
    
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir_path, ec))
    {
        if (ec)
        {
            // Skip directories that we can't access
            ec.clear();
            continue;
        }
        
        if (entry.is_regular_file(ec) && !ec)
        {
            auto file_time = entry.last_write_time(ec);
            if (!ec)
            {
                auto size = entry.file_size(ec);
                if (!ec)
                {
                    // Convert filesystem time to timestamp
                    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                        file_time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
                    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(sctp.time_since_epoch()).count();
                    
                    process_file(entry.path().string().c_str(), timestamp, size);
                }
            }
        }
    }
}

// STL filesystem implementation
static void scan_directory_recursive(const char* dir_path)
{
    std::filesystem::path path(dir_path);
    scan_directory_files(path);
}

// Process a single file (STL filesystem)
static void process_file(const char* file_path, uint64_t modified_time, uint64_t size)
{
    std::filesystem::path path(file_path);
    std::string path_str = path.string();
    
    auto it = g_watcher.file_map.find(path_str);
    
    if (it != g_watcher.file_map.end())
    {
        // File already tracked, check for modifications
        FileInfo& existing = it->second;
        // Use tolerance for timestamp comparison to avoid precision issues
        uint64_t time_diff = existing.modified_time > modified_time ? 
            existing.modified_time - modified_time : 
            modified_time - existing.modified_time;
            
        if (time_diff > 2) // 2ms tolerance to account for timestamp precision
        {
            // File was modified
            queue_event(existing.path, FILE_CHANGE_TYPE_MODIFIED);
            existing.modified_time = modified_time;
        }
        // Mark as seen by restoring the size
        existing.size = size;
    }
    else
    {
        // New file
        FileInfo info;
        info.path = path;
        info.modified_time = modified_time;
        info.size = size;
        
        g_watcher.file_map[path_str] = info;
        queue_event(info.path, FILE_CHANGE_TYPE_ADDED);
    }
}

// Queue an event
static void queue_event(const std::filesystem::path& path, FileChangeType type)
{
    std::lock_guard<std::mutex> lock(g_watcher.queue.mutex);
    
    if (g_watcher.queue.count >= MAX_EVENTS_QUEUE)
    {
        // Queue is full, drop oldest event
        g_watcher.queue.head = (g_watcher.queue.head + 1) % MAX_EVENTS_QUEUE;
        g_watcher.queue.count--;
    }
    
    // Add new event
    FileChangeEvent* event = &g_watcher.queue.events[g_watcher.queue.tail];
    event->path = path;
    event->type = type;
    event->timestamp = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    
    g_watcher.queue.tail = (g_watcher.queue.tail + 1) % MAX_EVENTS_QUEUE;
    g_watcher.queue.count++;
    
}

