#pragma once
#include "pc_p2_original_captain_throw.h"
#include <array>
#include <functional>
namespace p2original {namespace captain {namespace attachments {
using actions::Vec3;
struct CreatureHandle {Creature* actor=nullptr;std::uint64_t lifetime=0;};
struct BombHandle {Creature* actor=nullptr;std::uint64_t lifetime=0;};
// Stable state-owned source Matrixf values (row-major 3x4). Concrete capture
// owner retains a real SDK capture matrix bound to this address/lifetime and
// applies the source Creature/Bomb capture callbacks, not a P1 held-item shim.
struct CaptureMatrix {std::array<float,12> values{};};
struct BombFrame {BombHandle handle;bool sourceBomb=false;const CaptureMatrix* capturedBy=nullptr;};
struct Frame {Vec3 position;float face=0,delta=0,stickX=0,stickY=0;bool controller=false,pressedA=false,pressedB=false;unsigned stickCount=0;};
enum class Sound {PickupBomb,Throw};
class AttachmentSource {
public:
 virtual ~AttachmentSource()=default;
 virtual const LoadedScene& scene()const=0;
 virtual bool frame(const Navi&,Frame&,std::string&)const=0;
 virtual bool control(Navi&,std::string&)=0;
 // Actual releasePikis return value is ignored by Stuck, but source CPlate,
 // receiver transitions and successful-release actor timer remain genuine.
 virtual bool releasePikis(Navi&,bool& released,std::string&)=0;
 virtual bool forEachSticker(Navi&,const std::function<bool(std::optional<CreatureHandle>)>&,std::string&)=0;
 virtual bool sticker(const Navi&,CreatureHandle,std::string&)const=0;
 virtual bool randomFloat(float&,std::string&)=0; // actual source RNG draw
 virtual bool flick(Navi&,CreatureHandle,float knockback,float damage,float angle,bool& accepted,std::string&)=0;
 virtual bool bomb(BombHandle,BombFrame&,std::string&)const=0;
 virtual bool sound(Navi&,Sound,std::string&)=0;
 virtual bool startCapture(Navi&,BombHandle,const CaptureMatrix*,std::string&)=0;
 virtual bool updateCapture(Navi&,BombHandle,const CaptureMatrix& localRotation,std::string&)=0;
 virtual bool bombVelocity(BombHandle,Vec3,std::string&)=0;
 virtual bool endCapture(BombHandle,std::string&)=0;
};
// Arg itself is mandatory. Its bomb pointer may be null, as retail permits.
bool beginCarryBomb(Navi*,std::optional<BombHandle>,std::string&);
} void registerStuckCarryBombStates(NaviStateMachine&);
}}
p2original::captain::attachments::AttachmentSource* pc_p2_original_captain_attachment_source(const Navi*);
bool pc_p2_original_captain_stuck_bomb_preflight(Navi*,p2original::captain::StateId,std::string&);
