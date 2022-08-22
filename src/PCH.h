#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <unordered_map>
#include <unordered_set>

#include "F4SE/F4SE.h"
#include "RE/Fallout.h"

#include <fmt/printf.h>
#include <spdlog/sinks/basic_file_sink.h>

#define DLLEXPORT __declspec(dllexport)

using namespace std::literals;

namespace logger = F4SE::log;

namespace stl {
	using namespace F4SE::stl;
}

#include "Version.h"
