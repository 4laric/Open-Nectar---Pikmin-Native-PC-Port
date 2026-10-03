#pragma once
#include "pc_p2_original_captain_damage.h"
#include <string>

namespace p2original { namespace captain {
enum class SectionKind { Ground, Cave };
// The actual original section owner supplies current mode and finalization.
// Prepared resources, native P1 section flags and tool controls cannot implement
// this contract. Identity is the canonical live source LoadedScene object.
class DownSection {
public:
 virtual ~DownSection()=default;
 virtual const LoadedScene& scene()const=0;
 virtual SectionKind kind()const=0;
 virtual bool preflight(Navi&,std::string&)const=0;
 // MovieStart selects the downed actor/camera and knockout title. This must
 // not informDeath or select the survivor before the finishing callback.
 virtual bool movieStarted(Navi&,std::string&)=0;
 // Source STB +MovieCommand 0. Ground does nothing; cave second-down invokes
 // actual panic/treasure loss using source manager death census, once.
 virtual bool movieCommand(Navi&,unsigned command,std::string&)=0;
 virtual bool cameraCommand(Navi&,unsigned command,std::string&)=0;
 virtual bool actorCommand(Navi&,unsigned command,std::string&)=0;
 virtual bool beginFinishing(Navi&,std::string&)=0;
 // Root commits its durable dead-bit only after actual laydown and manager
 // death accounting/squad release; implementations must deduplicate writes.
 virtual bool laydownAndInformDeath(Navi&,std::string&)=0;
 // After that commit: survivor selection, or real source ground day-end /
 // cave CaptainsDown result. Both-down is distinct from regular day completion.
 virtual bool finished(Navi&,bool bothDown,std::string&)=0;
 virtual bool inactive(Navi&,std::string&)=0;
};
} }
p2original::captain::DownSection* pc_p2_original_captain_down_section();
bool pc_p2_original_captain_down_preflight(const Navi*,std::string&);
bool pc_p2_original_captain_down_begin(Navi*,std::string&);
// Actual MoviePlayer update calls this once with actual engine delta.
void pc_p2_original_captain_down_update(MoviePlayer*,float deltaSeconds);
bool pc_p2_original_captain_down_demo(p2original::captain::Demo&);
bool pc_p2_original_captain_down_dead_bits(unsigned&);
