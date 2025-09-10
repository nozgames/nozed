//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

#include <filesystem>

enum FileChangeType
{
    FILE_CHANGE_TYPE_ADDED,
    FILE_CHANGE_TYPE_MODIFIED,
    FILE_CHANGE_TYPE_DELETED
};

struct FileChangeEvent
{
    std::filesystem::path path;
    FileChangeType type;
    uint64_t timestamp;
};

void InitFileWatcher(int poll_interval_ms);
void ShutdownFileWatcher();
bool WatchDirectory(const std::filesystem::path& directory);
bool GetFileChangeEvent(FileChangeEvent* event);
