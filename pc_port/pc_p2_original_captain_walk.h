#pragma once
#include "pc_p2_original_captain_control.h"
#include "pc_p2_original_captain_damage.h"
#include <cstdint>
#include <optional>
#include <vector>
namespace p2original { namespace captain { namespace walk {
using control::Vec3;
enum class AI { Control=0, Wait=1, Animation=2, Escape=3, Attack=4 };
enum class Motion { None, Walk, Wait, Step, Yawn, LookBack, LookAround, Jump, Exercise };
const char* authoredClip(Motion);
struct Candidate {
 std::uint64_t identity=0;
 Vec3 position;
 float sphereRadius=0;
 bool alive=false, teki=false, living=false, emotionNonzero=false, navi=false, self=false;
 // Must come from source EnemyInfo mBitterDrops, not enemy family guesses.
 std::optional<bool> weakBitterDrop;
};
struct State {
 AI ai=AI::Control;
 float idleTimer=3;
 std::uint64_t target=0;
 Vec3 initialPosition;
 std::optional<bool> escapeCCW;
 std::optional<std::uint8_t> escapeTimer;
 std::uint8_t collisionTimer=0;
 std::optional<Motion> animation;
 // Retail Walk::init does NOT initialize mDismissTimer. First X-down or
 // inactive-X path establishes it; never serialize an invented zero.
 std::optional<std::uint8_t> dismissTimer;
};
struct Actor {
 bool alive=false, movieActor=false, hasController=false;
 Vec3 position;
 float faceDirection=0, sceneAnimationTimer=0;
 unsigned slot=0;
};
struct World {
 bool movieFlagActive=false, demoInactive=false, storyMode=false,
 activeActor=false, softPaused=false, multiplayer=false,
 switchUnlocked=false, napsackReceipt=false, frozen=false, debtPaid=false, versusMode=false;
};
struct OtherCaptain { std::uint64_t identity=0; bool alive=false; StateId state=StateId::Walk; bool needYChangeMotion=false; };
struct Buttons { bool aDown=false,bDown=false,xDown=false,xHeld=false,yDown=false,upDown=false,downDown=false; };
struct Onion { std::uint64_t identity=0; bool isPod=false; };
struct Frame {
 // Missing producer data is refusal. The adapter authenticates these against
 // the actual selected source scene, State pointer, actor and live providers.
 std::optional<Actor> actor;
 std::optional<World> world;
 std::optional<Buttons> buttons;
 std::optional<unsigned> stickCount;
 // Null onion is a completed source checkOnyon query (not missing query).
 bool onionQueryComplete=false;
 std::optional<Onion> onion;
 std::optional<OtherCaptain> other;
 float deltaTime=0;
 // Exact result of the first source control call, preflighted before commands
 // apply. An entry-time timer is insufficient for the >9 idle branch.
 std::optional<float> postControlSceneAnimationTimer;
 // Exact CellIterator sphere100 iteration order and narrowphase membership.
 // Do NOT replace it by nearest target selection or a fabricated enemy list.
 std::optional<std::vector<Candidate>> cellCandidates;
 std::optional<Candidate> currentTarget;
 std::optional<bool> assertIdleMotion;
 // Actual source RNG samples in consumption order, finite [0,1].
 std::vector<float> randomValues;
};
enum class Kind { StartMotion, MakeVelocity, MakeCStick, Rappa, FindNextThrowPiki,
 AddVelocity, SetFace, TurnTo, TransitSelf, RequestActionButton, RequestDismiss,
 TogglePlayer, ChangeVoice, WhistleSelfPreserveParties, TransitOtherChange, IdleVoice, JumpLandVoice };
struct Command {
 explicit Command(Kind value) : kind(value) {}
 Kind kind; StateId state=StateId::Walk;
 std::uint64_t target=0; Vec3 vector; float scalar=0;
 Motion motion=Motion::None;
 // Dope argument: bitter=true / spicy=false. Movie/party flags are not inferred.
 bool bitter=false;
};
enum class Stage { None, ActionButton, Dismiss };
struct Output { std::vector<Command> commands; Stage continuation=Stage::None; };
bool init(const Actor&, State&, Output&, std::string& error);
bool step(const control::Params&, const Frame&, State&, Output&, std::string& error);
// A branch always returns after procActionButton, even when no throw occurs.
bool resumeActionButton(bool actuallyHandled, std::optional<bool> sourceThrowable, Output&, std::string& error);
// Continue X-held/napsack/Y gates AFTER the real source releasePikis result.
bool resumeDismiss(const Frame&, bool actuallyReleased, State&, Output&, std::string& error);
bool keyEventEnd(State&, Output&, std::string& error);
bool jumpKey200(const Frame&, Output&, std::string& error);
struct Collision {
 std::uint64_t identity=0;
 bool honey=false, yellowHoney=false, absorbable=false;
 bool teki=false, captured=false, alive=false, sourceEnemyBomb=false;
 std::optional<control::Vec2> controllerStick;
};
bool collision(const Frame&, const Collision&, State&, Output&, std::string& error);
void wallHit(State&);
// Capture uses these finite fields only; identity/motion must be rebound to
// the real source actor/bank. Runtime saves must refuse unresolved continuations
// or absent dismissTimer; restore after Navi reset and before source callbacks.
bool checkpointValid(const State&, std::string& error);
}}}
