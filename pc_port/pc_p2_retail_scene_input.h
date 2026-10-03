#pragma once
#include "pc_p2_retail_cave_plan.h"
#include <array>
#include <locale>

namespace p2retail {
// A selected input contract, not a live floor, card proof or World grant.
inline constexpr const char* developmentFloorRole="p2-original/development-floor.p2d";
enum class SceneInput : unsigned { Plan, Geometry, Routes, Start, Pool, Layout, Rooms, Water, Parameters, SourceRoutes, Count };
struct DevelopmentFloor {
 std::string cave;
 unsigned floor=0;
 unsigned version=1;
 std::array<std::string,10> sha256;
};
inline std::string sceneInputRole(const DevelopmentFloor& floor,SceneInput input){
 static constexpr const char* names[]={"floor.p2f","geometry.mod","routes.ini","start.json","unit-pool.txt","start-layout.txt","room-census.json","water-census.json","floor-parameters.json","source-routes.json"};
 const unsigned index=static_cast<unsigned>(input);
 if(floor.cave!="tutorial_1"||(floor.floor!=1&&floor.floor!=2)||floor.version<1||floor.version>4||index>=10||
    (index>=6&&floor.version<2)||(index==8&&floor.version<3)||(index==9&&floor.version<4))return {};
 return "p2-original/retail-caves/tutorial_1/floor"+std::to_string(floor.floor)+"/"+names[index];
}
inline bool parseDevelopmentFloor(const std::string& bytes,DevelopmentFloor& out,std::string& error){
 if(bytes.empty()||bytes.size()>1024){error="retail development selection size";return false;}
 std::istringstream input(bytes);input.imbue(std::locale::classic());
 DevelopmentFloor next;std::string tag;
 if(!(input>>tag>>next.cave>>next.floor)||(tag!="P2_RETAIL_DEVELOPMENT_FLOOR_1"&&tag!="P2_RETAIL_DEVELOPMENT_FLOOR_2"&&tag!="P2_RETAIL_DEVELOPMENT_FLOOR_3"&&tag!="P2_RETAIL_DEVELOPMENT_FLOOR_4")||
    next.cave!="tutorial_1"||(next.floor!=1&&next.floor!=2)){
  error="retail development selection framing";return false;
 }
 next.version=tag=="P2_RETAIL_DEVELOPMENT_FLOOR_4"?4:tag=="P2_RETAIL_DEVELOPMENT_FLOOR_3"?3:tag=="P2_RETAIL_DEVELOPMENT_FLOOR_2"?2:1;
 static constexpr const char* labels[]={"plan","geometry","routes","start","pool","layout","room-census","water-census","floor-parameters","source-routes"};
 for(unsigned i=0;i<(next.version>=3?next.version+6:next.version==2?8u:6u);++i)if(!(input>>tag>>next.sha256[i])||tag!=labels[i]||!hex64(next.sha256[i])){
  error="retail development selection digest/order";return false;
 }
 if(input>>tag){error="retail development selection trailing bytes";return false;}
 out=std::move(next);error.clear();return true;
}
struct SelectedSceneInputs {
 DevelopmentFloor selection;
 FloorPlan plan;
 std::array<std::string,10> bytes;
 std::string campaign,session;
 std::uint64_t revision=0;
};
}
// Absent is legitimate only after an authenticated OriginalSession is selected.
// A present unreadable/tampered role is always an error, never a surface fallback.
// Outputs remain unchanged on refusal. This reader does not activate a scene.
bool pc_p2_retail_scene_selection(p2retail::SelectedSceneInputs&,bool& present,std::string& error);
