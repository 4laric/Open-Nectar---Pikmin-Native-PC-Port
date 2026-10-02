#pragma once
#include "pc_midday_codec.h"
#include <filesystem>
#include <functional>

namespace pc_midday {
enum class LoadResult { Current, RecoveryAvailable, Recovered, Missing, Invalid };
// Returning false simulates failure at a completed durability boundary. A process
// exit here also leaves exactly the same published filesystem state.
using Boundary = std::function<bool(const char*)>;
struct RecoveryPlan {
    uint64_t selectedGeneration = 0, highWater = 0, dayEndGeneration = 0;
    Digest lockOwner{}, currentRecord{}, selectedCheckpoint{};
    bool currentMissing = false;
    bool explicitUncommittedSelection = false;
};
// Inspect/execute are separate so UI can display the exact proposed recovery.
// Neither accepts an empty/unknown/live writer lock. CURRENT and selection hashes
// are rechecked under an OS process-lifetime gate before any recovery mutation.
bool inspectRecovery(const std::filesystem::path&, uint64_t selectedGeneration,
                     const Binding&, const Coverage&, RecoveryPlan&, std::string&,
                     uint64_t latestDayEndGeneration = 0,
                     bool explicitUncommittedSelection = false);
bool recover(const std::filesystem::path&, const RecoveryPlan&, const Binding&,
             const Coverage&, std::string&, Boundary boundary = {});
bool publish(const std::filesystem::path& directory, const Snapshot&, const Coverage&,
             std::string& error, Boundary boundary = {});
LoadResult load(const std::filesystem::path& directory, const Binding&, const Coverage&,
                Snapshot& out, std::string& error, bool acceptPrevious = false,
                uint64_t latestDayEndGeneration = 0);
}
