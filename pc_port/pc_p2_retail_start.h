#pragma once
#include "pc_p2_retail_scene_input.h"

namespace p2retail {
struct SourceStart {
 std::array<double,3> mapStart{};
 std::array<double,3> slotPosition{};
 std::string unit;
 // Actual MapMgr supplies ground and map rotation after installation.
 static constexpr double groundOffset=8.5;
 static constexpr double captainX[2]={-4.526,18.082};
 static constexpr double captainZ[2]={7.453,-11.482};
};
// Independently decodes retained source-start JSON, original unit pool and
// BaseGen layout. No actor, geometry, stage or activity authority is issued.
bool parseSourceStart(const SelectedSceneInputs&,SourceStart&,std::string& error);
}
