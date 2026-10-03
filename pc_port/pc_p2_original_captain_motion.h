#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
class Navi;class Graphics;
namespace p2original { namespace captain {
// Actual Game::IPikiAnims identities, never P1 PaniMotion IDs.
enum class Motion:unsigned {Asibumi=1,Damage=4,Dead=5,Fue=10,Getup=14,Jhit=22,Jkoke=23,Nigeru=28,Run2=29,Walk=30,Wait=31,Throw=33,ThrowWait=34};
struct MotionState {Motion motion=Motion::Wait;float frame=0;std::uint64_t generation=0;bool finishing=false,complete=false;};
enum class SourceResource {Parameters,AnimRegistry,Collision,DownStudio,DownAnimation};
struct SourceParameters {
 float maximumHealth=0,moveSpeed=0,neutralStick=0,cursorStick=0;
 std::string rawSourceSha;
};
// Owned by the actual source Course. prepare reads descriptor-selected exact
// bank/source/event/model bytes. Each roster actor has independent geometry,
// motion clock and generation; source switching never touches P1 animators.
class SourceBank {
public:
 SourceBank();~SourceBank();
 bool prepare(std::string&);
 bool parameters(SourceParameters&,std::string&)const;
 // Retained exact descriptor-selected source bytes; no ambient path getter.
 bool sourceBytes(SourceResource,std::string& out,std::string&)const;
 // Actual s03_dead1.bck presentation; Studio timing/events remain the movie
 // lifecycle owner's independently authenticated DownStudio clock.
 bool startDownMovie(Navi*,std::string&);
 // Studio owner supplies its actual source frame, independently of FSM motion.
 bool updateDownMovie(Navi*,float actualStudioFrame,std::string&);
 // Called only after actual native bootstrap/reset has completed. Rebind or
 // wrong/duplicate Native NaviMgr slot pointers are refused.
 bool bindRoster(Navi* olimar,Navi* louie,std::string&);
 bool supports(Navi*,Motion,std::string&)const;
 bool start(Navi*,Motion,std::string&);
 // Only genuine locomotion transitions preserve the bound source frame.
 bool startPreservingFrame(Navi*,Motion,std::string&);
 // Amount is actual source animation frames chosen by the source FSM owner.
 // Strict authored keys emit when key.frame < int(timer); END1000 once.
 // The callback may change source motion/state. Generation changes stop old
 // delivery immediately, including an old loop/end after the callback.
 bool advance(Navi*,float amount,const std::function<bool(int)>& emit,std::string&);
 bool finish(Navi*,std::string&); // stop repeating the authored loop at its END
 bool state(const Navi*,MotionState&,std::string&)const;
 bool refresh(Navi*,Graphics&,std::string&);
 void forget(Navi*);
 bool ready()const;
 const std::string& fingerprint()const;
private:
 struct Impl;std::unique_ptr<Impl> m;
};
} }
// Canonical current source Course owns this bank; absent until genuine selected
// resources and the actual two-body bootstrap have both completed.
p2original::captain::SourceBank* pc_p2_original_captain_source_bank();
