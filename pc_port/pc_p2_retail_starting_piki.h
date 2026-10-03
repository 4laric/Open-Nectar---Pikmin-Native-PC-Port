#pragma once
#include "pc_p2_original_piki_manifest.h"
#include <map>
namespace p2retail {
class SceneContext;
struct SourceStart;
inline constexpr const char* startingPikiRole="p2-original/development-starting.p2ps";
inline constexpr const char* startingManifestRole="p2-original/development-starting.p2pk";
inline constexpr const char* startingProvenanceRole="p2-original/development-starting.txt";
// Explicit engineered twenty-red fixture, distinct from the floor GenPiki
// census and story acquisition. Catalog positions are absolute authored poses;
// the actual factory must validate dry support and refuse, never relocate them.
struct StartingPikiInputs {
 p2original::PikiManifest manifest;
 std::string cave,planSha,startSha,baselineSha,provenanceSha,manifestSha;
 unsigned floor=0;
 std::map<std::string,std::string> bank;
};
bool parseStartingPiki(const std::string&,const std::string& manifestBytes,
                      const std::string& provenanceBytes,StartingPikiInputs&,std::string&);
bool validateStartingPikiPlacement(const StartingPikiInputs&,const SourceStart&,std::string&);
// Rechecks every explicitly selected closure member on the actual owned scene.
// Resource reads grant no native body lifetime, admission or World activity.
bool selectedStartingPiki(const SceneContext&,StartingPikiInputs&,std::string&);
}
