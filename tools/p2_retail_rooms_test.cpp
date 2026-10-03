#include "pc_p2_retail_rooms.h"
#include <cassert>
#include <fstream>
#include <iostream>
static std::string file(const std::string& path){std::ifstream f(path,std::ios::binary);assert(f);return {std::istreambuf_iterator<char>(f),{}};}
static std::string hash(const std::string& bytes){unsigned char d[32];pc_netplay_sha::sha256(bytes.data(),bytes.size(),d);return pc_netplay_sha::hex(d,32);}
int main(int argc,char** argv){
 assert(argc==5);using namespace p2retail;
 for(unsigned floor=1;floor<=2;++floor){SelectedSceneInputs input;input.selection.cave="tutorial_1";input.selection.floor=floor;input.selection.version=3;
  for(unsigned i=0;i<6;++i){input.bytes[i]=file(std::string(argv[1])+"/"+sceneInputRole(input.selection,static_cast<SceneInput>(i)));input.selection.sha256[i]=hash(input.bytes[i]);}
  input.bytes[6]=file(std::string(argv[2])+"/floor"+std::to_string(floor)+"/p2-retail-room-census.json");input.selection.sha256[6]=hash(input.bytes[6]);
  input.bytes[7]=file(std::string(argv[3])+"/floor"+std::to_string(floor)+"/p2-retail-water-census.json");input.selection.sha256[7]=hash(input.bytes[7]);
  input.bytes[8]=file(std::string(argv[4])+"/floor"+std::to_string(floor)+"/p2-retail-floor-parameters.json");input.selection.sha256[8]=hash(input.bytes[8]);
  std::string error;assert(parseFloorPlan(input.bytes[0],input.selection.sha256[0],input.plan,error));
  SourceRoomCensus census;assert(parseSourceRoomCensus(input,census,error));assert(census.rooms.size()==(floor==1?3:1));
  SourceWaterInputs water;assert(parseSourceWaterInputs(input,census,water,error));assert(water.units.size()==census.units.size());
  SourceFloorParameters parameters;assert(parseSourceFloorParameters(input,census,water,parameters,error));
  assert(!parameters.hasHiddenCollision&&parameters.hiddenCollisionValue==0&&parameters.parameters.at("f013")=="0"&&parameters.firstFloor==floor&&parameters.lastFloor==floor);
  auto legacy=input;legacy.selection.version=2;legacy.bytes[8].clear();legacy.selection.sha256[8].clear();
  SourceRoomCensus legacyRooms;SourceWaterInputs legacyWater;
  assert(parseSourceRoomCensus(legacy,legacyRooms,error)&&parseSourceWaterInputs(legacy,legacyRooms,legacyWater,error));
  assert(census.units.size()==(floor==1?2:1));
  unsigned vertices=0,triangles=0;for(const auto& room:census.rooms){const auto& unit=census.units.at(room.unit);vertices+=unit.vertexBits.size();triangles+=unit.triangles.size();}
  assert(vertices==(floor==1?238u:800u));assert(triangles==(floor==1?352u:1414u));
  SourceRoomGeometry geometry;assert(adoptSourceRoomGeometry(census,geometry,error));
  assert(geometry.vertices.size()==vertices&&geometry.triangles.size()==triangles&&geometry.rooms.size()==census.rooms.size());
  unsigned offset=0,triangleOffset=0;
  for(unsigned i=0;i<census.rooms.size();++i){const auto& room=census.rooms[i];const auto& unit=census.units[room.unit];
   assert(geometry.rooms[i].roomIndex==i&&geometry.rooms[i].unit==room.unit);
   assert(geometry.rooms[i].matrix[3]==room.translation[0]&&geometry.rooms[i].matrix[11]==room.translation[2]);
   for(unsigned t=0;t<unit.triangles.size();++t){const auto& actual=geometry.triangles[triangleOffset+t];
    assert(actual.roomIndex==i&&actual.mapcode==unit.mapcodes[t]);for(unsigned k=0;k<3;++k)assert(actual.abc[k]==unit.triangles[t][k]+offset);}
   offset+=unit.vertexBits.size();triangleOffset+=unit.triangles.size();}
  if(floor==1)assert(geometry.rooms[2].matrix[2]>0&&geometry.rooms[2].matrix[2]<1e-6f);
  auto alteredCensus=census;alteredCensus.rooms[0].roomIndex=999;auto retainedGeometry=geometry;
  assert(!adoptSourceRoomGeometry(alteredCensus,retainedGeometry,error)&&retainedGeometry.vertices==geometry.vertices&&retainedGeometry.triangles.size()==geometry.triangles.size());
  unsigned negatives=0;auto refuse=[&](const SelectedSceneInputs& bad){auto retained=census;
   assert(!parseSourceRoomCensus(bad,retained,error));assert(!error.empty());assert(retained.sha256==census.sha256&&retained.rooms.size()==census.rooms.size()&&retained.units[0].gridBytes==census.units[0].gridBytes);++negatives;};
  auto bad=input;bad.selection.version=1;refuse(bad);
  bad=input;bad.selection.sha256[6]=std::string(64,'0');refuse(bad);
  bad=input;bad.bytes[6].pop_back();bad.selection.sha256[6]=hash(bad.bytes[6]);refuse(bad);
  bad=input;bad.bytes[6]+=std::string(1024*1024,' ');bad.selection.sha256[6]=hash(bad.bytes[6]);refuse(bad);
  for(unsigned i=0;i<6;++i){bad=input;bad.bytes[i]+="tamper";refuse(bad);}
  bad=input;bad.plan.catalogSha256=std::string(64,'0');refuse(bad);
  bad=input;bad.plan.floor=floor==1?2:1;refuse(bad);
  bad=input;bad.plan.layoutSha256=std::string(64,'0');refuse(bad);
  for(const char* label:{"quarter_turn","archive_sha256","source_unit_bounds_f32_bits","triangles_abc","native_ready"}){
   bad=input;const auto at=bad.bytes[6].find(label);assert(at!=std::string::npos);bad.bytes[6][at]='X';bad.selection.sha256[6]=hash(bad.bytes[6]);refuse(bad);}
  std::cout<<"floor="<<floor<<" source census PASS rooms="<<census.rooms.size()<<" vertices="<<vertices<<" triangles="<<triangles<<" refusals="<<negatives<<"; no native owner/physics grant\n";
  unsigned waterNegatives=0;auto waterRefuse=[&](const SelectedSceneInputs& changed){auto retained=water;
   assert(!parseSourceWaterInputs(changed,census,retained,error));assert(!error.empty());
   assert(retained.sha256==water.sha256&&retained.units[0].raw==water.units[0].raw);++waterNegatives;};
  bad=input;bad.bytes[7].clear();waterRefuse(bad);
  bad=input;bad.selection.sha256[7]=std::string(64,'0');waterRefuse(bad);
  bad=input;bad.bytes[7].pop_back();bad.selection.sha256[7]=hash(bad.bytes[7]);waterRefuse(bad);
  bad=input;bad.selection.version=1;waterRefuse(bad);
  bad=input;bad.bytes[6]+="tamper";waterRefuse(bad);
  bad=input;bad.bytes[7]+=std::string(65536,' ');bad.selection.sha256[7]=hash(bad.bytes[7]);waterRefuse(bad);
  for(const char* label:{"count","bytes_base64","room_census_sha256","runtime_known_dry","quarter_turn"}){
   bad=input;const auto at=bad.bytes[7].find(label);assert(at!=std::string::npos);bad.bytes[7][at]='X';bad.selection.sha256[7]=hash(bad.bytes[7]);waterRefuse(bad);}
  std::cout<<"floor="<<floor<<" water input PASS count0 units="<<water.units.size()<<" refusals="<<waterNegatives<<"; runtime known-dry unavailable\n";
  unsigned parameterNegatives=0;auto parameterRefuse=[&](const SelectedSceneInputs& changed){auto retained=parameters;
   assert(!parseSourceFloorParameters(changed,census,water,retained,error));assert(!error.empty());
   assert(retained.sha256==parameters.sha256&&retained.sourceBytes==parameters.sourceBytes&&retained.parameters==parameters.parameters);++parameterNegatives;};
  parameterRefuse(legacy);
  bad=input;bad.bytes[8].clear();parameterRefuse(bad);
  bad=input;bad.selection.sha256[8]=std::string(64,'0');parameterRefuse(bad);
  bad=input;bad.bytes[8].pop_back();bad.selection.sha256[8]=hash(bad.bytes[8]);parameterRefuse(bad);
  bad=input;bad.bytes[8]+=std::string(65536,' ');bad.selection.sha256[8]=hash(bad.bytes[8]);parameterRefuse(bad);
  for(unsigned index:{6u,7u}){bad=input;bad.bytes[index]+="tamper";parameterRefuse(bad);}
  for(const char* label:{"f013","has_hidden_collision","first_floor","bytes_base64","room_census_sha256","water_census_sha256"}){
   bad=input;const auto at=bad.bytes[8].find(label);assert(at!=std::string::npos);bad.bytes[8][at]='X';bad.selection.sha256[8]=hash(bad.bytes[8]);parameterRefuse(bad);}
  std::cout<<"floor="<<floor<<" original parameter PASS explicitf013=0 refusals="<<parameterNegatives<<"; no trace/contact grant\n";
 }
}
