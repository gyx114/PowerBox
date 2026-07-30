// pch.h: This is the precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, if any of the files listed here are updated between builds, they will all be recompiled.
// Do not add frequently updated files here, as this would negate the performance benefits.

#ifndef PCH_H
#define PCH_H

// Avoid Windows min/max macros interfering with std::min/std::max
#define NOMINMAX

// Add headers to precompile here
#include "framework.h"

// C++20 standard library
#include <format>
#include <filesystem>
namespace fs = std::filesystem;
#include <span>
#include <string_view>
#include <stop_token>
#include <thread>
#include <atomic>
#include <vector>
#include <functional>
#include <memory>
#include <string>
#include <algorithm>
#include <ranges>
#include <atlimage.h>

#endif //PCH_H