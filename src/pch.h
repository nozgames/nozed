#pragma once

// Standard Library Headers (commonly used across editor)
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#include <functional>
#include <atomic>
#include <memory>
#include <mutex>
#include <queue>
#include <stack>
#include <cctype>
#include <cstdio>
#include <cstdarg>
#include <ranges>

// Define Windows header control macros before any Windows includes
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN  
#define WIN32_LEAN_AND_MEAN
#endif
#define NOGDI
#define NOUSER

#include <noz/noz.h>
#include "../../../src/vfx/vfx_internal.h"

struct EditorEventStats
{
    i32 fps;
};

enum EditorEvent
{
    EDITOR_EVENT_STATS,
    EDITOR_EVENT_IMPORTED
};

#include <utils/props.h>
#include <../noz/include/noz/tokenizer.h>
#include <utils/file_helpers.h>
#include "editor.h"
#include "nozed_assets.h"

extern Props* g_config;
