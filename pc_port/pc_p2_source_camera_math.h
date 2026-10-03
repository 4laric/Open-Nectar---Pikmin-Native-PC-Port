#pragma once
#include <cstddef>
#include <cstdint>

namespace p2original { namespace cameraMath {
// Source-semantic host math only. These values grant no Stage/camera ownership.
// Matrix storage is Dolphin Mtx: three rows, four columns (not Matrix4f fields).
struct Vec { float x,y,z; };
struct Matrix { float row[3][4]; };
struct Plane { Vec normal; float offset; };
struct Frustum { Plane plane[6]; };
struct Projection { float viewAngle,aspect,nearDistance,farDistance; };
struct Bounds { float left,top,right,bottom; };
struct Viewport { std::uint8_t flags; Bounds bounds2; const Frustum* camera; };

// Camera::getPosition uses the genuine JST camera position when JST is running.
// On refusal the previous output remains unchanged.
bool derive(const Matrix&,const Projection&,const Vec* runningJstPosition,Frustum&) noexcept;
bool sphereVisible(const Frustum&,const Vec&,float radius) noexcept;
bool viewable(const Viewport&) noexcept;
float aspect(const Bounds&) noexcept;

// Port of the one process-static volatile ParticleMgr::disableCulling. Source
// reset/start/setViewport do not write it; no named source write was found in
// the available .cpp/.h/.s corpus. There is deliberately no invented setter.
// Reading this state authenticates neither a camera nor a selected Scene.
bool processDisableCulling() noexcept;

// ParticleMgr::setViewport asserts 0 <= count < 4. All referenced viewports,
// rather than the current draw slot or active count, participate in the test.
// Caller must supply the actual process-global disableCulling state. A bool
// argument is not a substitute for installing that owner at the native boundary.
// Missing camera/invalid input refuses and preserves the previous result.
bool cull(const Viewport* const*,std::size_t count,bool disableCulling,
          const Vec&,float radius,bool& culled) noexcept;
} }
