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

#include "tokenizer.h"
#include "asset_importer.h"
#include "tui/tstring.h"

static constexpr int TERM_COLOR_STATUS_BAR = 1;
static constexpr int TERM_COLOR_COMMAND_LINE = 2;
static constexpr int TERM_COLOR_SUCCESS = 3;
static constexpr int TERM_COLOR_ERROR = 4;
static constexpr int TERM_COLOR_WARNING = 5;

static constexpr int TERM_COLOR_DISABLED_TEXT = 8;

// Terminal key constants
static constexpr int ERR = -1;
static constexpr int KEY_MOUSE = 409;

struct EditorEventStats
{
    i32 fps;
};

enum EditorEvent
{
    EDITOR_EVENT_STATS
};