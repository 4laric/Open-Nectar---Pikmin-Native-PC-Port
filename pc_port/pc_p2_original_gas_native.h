#pragma once
#include "pc_p2_original_gas.h"
#include <functional>
class BTeki;
class Graphics;
struct Matrix4f;
namespace p2original { namespace gas {
// Source item/effect/sound backends belong to their course owners. Missing
// backends refuse admission; there is no P1 particle or captain-damage proxy.
class Services {
public:
 virtual ~Services()=default;
 virtual bool sourceEffectsAndSoundsReady(std::string&)=0;
 virtual bool gasEffect(Creature*,bool active,bool surface,std::string&)=0;
 virtual bool effectLod(Creature*,float nearDistance,float middleDistance,std::string&)=0;
 virtual bool sourceSound(Creature*,const char* name,std::string&)=0;
 virtual bool sourceFatalEffect(Creature*,std::string&)=0;
 virtual bool surfaceStory()const=0;
 virtual bool livingLinks(Position,void*&,void*&,std::string&)=0;
 virtual bool bridgeStage(void*,int&,std::string&)=0;
 virtual bool gateAlive(void*,bool&,std::string&)=0;
};
class Native {
public:
 explicit Native(Services&);
 ~Native();
 Native(const Native&)=delete; Native& operator=(const Native&)=delete;
 Provider& provider();
 bool owns(const Creature*)const;
 bool tick(BTeki*,float,std::string&);
 bool draw(BTeki*,Graphics&,const Matrix4f&,std::string&);
 // Optional observation AFTER actual Generator::informDeath and detach.
 // Must not record a second GroupCourse death.
 void onDeath(std::function<bool(Creature*,std::string&)>);
 void forget(BTeki*);
private:
 struct Impl; std::unique_ptr<Impl> m;
};
} }
bool pc_p2_original_gas_update(BTeki*);
bool pc_p2_original_gas_refresh(BTeki*,Graphics&);
// owned is independent of accepted: suppress host fallback even on rejection.
bool pc_p2_original_gas_damage(BTeki*,Creature* attacker,float damage,bool& accepted);
void pc_p2_original_gas_forget(BTeki*);
