#pragma once
#include "pc_p2_original_number_rigid.h"
#include <array>
namespace p2originalnumber { namespace motion {
using Vec3=rigid::Vec3;
struct Triangle { std::array<Vec3,3> vertices; Vec3 normal; float offset=0; };
struct Contact { Vec3 normal, point; float overlap=0; };
enum class Intersection { Miss, Hit, Invalid };
// GPVE01 Kando aiConstants.txt, SHA256 0bf964d8d4c975ef021d83180a3c81e6264c9bdd18d75035af4a48995a5a41a5.
constexpr float gravity=560.0f;
// Sys::Triangle::intersect (InsidePlane / IntersectPlane), including ordered
// source edge/endpoint tests. Plane normal/offset are actual source geometry.
Intersection intersect(const Triangle&,Vec3 center,float radius,bool hard,Contact&)noexcept;
bool restitution(Vec3 velocity,Vec3 normal,float factor,Vec3& out)noexcept;
// ShapeMapMgr permits 16; source RoomMapMgr permits 8. Actual scene provider
// selects the map family; this is never inferred from a numeric model size.
// Literal zero-radius point traces are valid (actual FakePiki initializer).
// Numeric birth profiles separately require their positive source radii.
bool stepCount(float dt,float speed,float radius,unsigned& count,float& step,unsigned maximum=16)noexcept;
bool beginSimple(Vec3 velocity,float dt,bool picked,bool alwaysCarried,bool previousFloor,Vec3& out)noexcept;
bool finishSimple(Vec3 velocity,Vec3 normal,float dt,bool picked,bool alwaysCarried,Vec3& out)noexcept;
// RoomMapMgr fallback AFTER all cave map substeps. Flags are actual floor state,
// never an inferred replacement floor. Caller delivers actual fallback callback.
bool hiddenFloor(rigid::Trace&,bool actualHiddenCollision,bool actualFloor,Contact&,bool& applied)noexcept;
}}
