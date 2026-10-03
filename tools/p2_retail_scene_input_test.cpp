#include "pc_p2_retail_scene_input.h"
#include <cassert>

int main(){
 using namespace p2retail;
 const std::string hash(64,'a');
 const std::string text="P2_RETAIL_DEVELOPMENT_FLOOR_1 tutorial_1 1\nplan "+hash+"\ngeometry "+hash+
  "\nroutes "+hash+"\nstart "+hash+"\npool "+hash+"\nlayout "+hash+"\n";
 DevelopmentFloor selected;std::string error;
 assert(parseDevelopmentFloor(text,selected,error));
 assert(selected.version==1&&sceneInputRole(selected,SceneInput::Rooms).empty());
 auto successor=text;successor.replace(successor.find("FLOOR_1"),7,"FLOOR_2");
 successor+="room-census "+hash+"\nwater-census "+hash+"\n";
 DevelopmentFloor newer;assert(parseDevelopmentFloor(successor,newer,error));
 assert(newer.version==2&&newer.sha256[6]==hash);
 assert(sceneInputRole(newer,SceneInput::Rooms)=="p2-original/retail-caves/tutorial_1/floor1/room-census.json");
 assert(sceneInputRole(newer,SceneInput::Water)=="p2-original/retail-caves/tutorial_1/floor1/water-census.json");
 assert(!parseDevelopmentFloor(successor.substr(0,successor.find("water-census")),newer,error));
 auto third=successor;third.replace(third.find("FLOOR_2"),7,"FLOOR_3");third+="floor-parameters "+hash+"\n";
 DevelopmentFloor parameters;assert(parseDevelopmentFloor(third,parameters,error)&&parameters.version==3&&parameters.sha256[8]==hash);
 assert(sceneInputRole(parameters,SceneInput::Parameters)=="p2-original/retail-caves/tutorial_1/floor1/floor-parameters.json");
 assert(sceneInputRole(newer,SceneInput::Parameters).empty());
 assert(!parseDevelopmentFloor(third.substr(0,third.find("floor-parameters")),parameters,error));
 assert(!parseDevelopmentFloor(successor+"floor-parameters "+hash+"\n",parameters,error));
 auto missing=successor.substr(0,successor.find("room-census"));
 assert(!parseDevelopmentFloor(missing,newer,error)&&newer.version==2&&newer.sha256[6]==hash);
 assert(!parseDevelopmentFloor(text+"room-census "+hash+"\n",newer,error));
 assert(sceneInputRole(selected,SceneInput::Start)=="p2-original/retail-caves/tutorial_1/floor1/start.json");
 selected.floor=2;
 assert(sceneInputRole(selected,SceneInput::Pool)=="p2-original/retail-caves/tutorial_1/floor2/unit-pool.txt");
 assert(sceneInputRole(selected,SceneInput::Layout)=="p2-original/retail-caves/tutorial_1/floor2/start-layout.txt");
 auto refuses=[&](const std::string& bad){
  DevelopmentFloor retained=selected;
  assert(!parseDevelopmentFloor(bad,retained,error));assert(!error.empty());
  assert(retained.cave==selected.cave&&retained.floor==2&&retained.sha256==selected.sha256);
 };
 refuses("");refuses(std::string(1025,' '));refuses(text+"extra");
 auto bad=text;bad.replace(bad.find("tutorial_1"),10,"forest_1");refuses(bad);
 bad=text;bad.replace(bad.find("tutorial_1"),10,"../escape");refuses(bad);
 bad=text;bad.replace(bad.find("tutorial_1 1"),12,"tutorial_1 0");refuses(bad);
 bad=text;bad.replace(bad.find("tutorial_1 1"),12,"tutorial_1 3");refuses(bad);
 bad=text;bad.replace(bad.find("plan "),5,"start ");refuses(bad);
 bad=text;bad[bad.find(hash)]='A';refuses(bad);
 bad=text;bad.erase(bad.rfind("layout"));refuses(bad);
 selected.floor=3;assert(sceneInputRole(selected,SceneInput::Start).empty());
 selected.floor=1;selected.cave="forest_1";assert(sceneInputRole(selected,SceneInput::Start).empty());
}
