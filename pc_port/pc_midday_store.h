#pragma once
#include "pc_midday_codec.h"
#include <filesystem>
#include <functional>

namespace pc_midday {
enum class LoadResult { Current, RecoveryAvailable, Recovered, Missing, Invalid };
// Returning false simulates failure at a completed durability boundary. A process
// exit here also leaves exactly the same published filesystem state.
using Boundary = std::function<bool(const char*)>;
bool publish(const std::filesystem::path& directory, const Snapshot&, const Coverage&,
             std::string& error, Boundary boundary = {});
LoadResult load(const std::filesystem::path& directory, const Binding&, const Coverage&,
                Snapshot& out, std::string& error, bool acceptPrevious = false,
                uint64_t latestDayEndGeneration = 0);
}
