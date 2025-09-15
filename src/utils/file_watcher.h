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
    std::filesystem::path relative_path;
    std::filesystem::path watch_path;
    FileChangeType type;
};

extern void InitFileWatcher(int poll_interval_ms, const char** dirs);
extern void ShutdownFileWatcher();
extern bool GetFileChangeEvent(FileChangeEvent* event);
