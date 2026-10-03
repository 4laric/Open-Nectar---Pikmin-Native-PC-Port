#pragma once
#include <string>

// Retail Navi::makeVelocity/reviseController and FakePiki::updateWalkAnimation.
// Engine-free policy only: caller owns real controller/camera, displacement,
// scene/state admission, equipment authority and authored animation banks.
namespace p2original { namespace captain { namespace control {
struct Vec2 { float x=0, z=0; };
struct Vec3 { float x=0, y=0, z=0; };
struct Values {
 float moveSpeed, rushBootSpeed, maxHealth, justiceReduction, wideWhistleRadius;
 float angleDegrees, neutralStick, cursorMovementStick, cursorLookTime, clampStick;
 float stepStart, walkStart, runStart, escapeStart;
 float walkMin, walkMax, runMin, runMax, escapeMin, escapeMax;
};
class Params {
public:
 const Values& values() const { return data_; }
 bool authenticated() const { return valid_; }
private:
 Values data_{}; bool valid_=false;
 friend bool parseParameters(const std::string&, Params&, std::string&);
};
// Exact GPVE01 user/Abe/piki/naviParms.txt. Legal bytes stay private.
const char* parameterSha256();
bool parseParameters(const std::string& bytes, Params&, std::string& error);
bool reviseStick(const Params&, Vec2 input, Vec2& output, std::string& error);
struct CameraBasis { Vec3 side, up, view; };
struct Input {
 bool hasController=false, activityButton=false;
 float stickX=0, stickY=0, deltaTime=0;
 Vec2 cursorOffset;
};
struct RuntimeControl { float sceneAnimationTimer=0, faceDirection=0; bool moveRotation=true; };
struct VelocityOutput { Vec2 targetVelocity, whistleDirection; };
// Emits authored base speed. Apply pc_p2_equipment_speed(baseSpeed) exactly
// once at the authenticated runtime integration boundary (not per axis).
bool makeVelocity(const Params&, const Input&, const CameraBasis&, RuntimeControl&, VelocityOutput&, std::string& error);
enum class Motion { Unsupported, Wait, Step, Walk, Run, Escape };
const char* authoredClip(Motion);
struct AnimationState { Motion bound=Motion::Wait, pending=Motion::Wait; unsigned pendingUpdates=0; float playbackSpeed=30; };
struct AnimationOutput {
 Motion motion=Motion::Unsupported;
 float playbackSpeed=0;
 bool transition=false, preserveFrame=false, listener=false;
};
// Caller applies bound/self animator handling and exact source key events.
// JKOKE self animator is rejected just as the source assertion requires.
bool updateWalkAnimation(const Params&, Vec2 displacement, float deltaTime,
 float faceDirection, float faceDirectionOffset, bool selfIsJKoke,
 AnimationState&, AnimationOutput&, std::string& error);
}}}
