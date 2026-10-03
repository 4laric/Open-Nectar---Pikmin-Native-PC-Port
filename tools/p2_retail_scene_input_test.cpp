#include "pc_p2_retail_scene_input.h"
#include <cassert>

int main(){
 using namespace p2retail;
 const std::string hash(64,'a');
 const std::string text="P2_RETAIL_DEVELOPMENT_FLOOR_1 tutorial_1 1\nplan "+hash+"\ngeometry "+hash+
  "\nroutes "+hash+"\nstart "+hash+"\npool "+hash+"\nlayout "+hash+"\n";
 DevelopmentFloor selected;std::string error;
 assert(parseDevelopmentFloor(text,selected,error));
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
