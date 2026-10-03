#pragma once
#include <string>
class Shape;
namespace p2original {
// Real selected Stage resource ownership only. No actor/readiness/world grant.
bool prepareWhiteShapes(std::string& error);
// Returns an installed selected-role shape only while its actual owned Stage
// context and immutable selection remain current. No filesystem/cache fallback.
Shape* whiteShape(const std::string& selectedRole) noexcept;
bool whiteShapesOwned() noexcept;
bool whiteShapesCanRelease(std::string& error);
// All source bodies/material consumers must retire before this call. Actual
// Stage invokes it before context/map/heap reuse; it also cleans partial loads.
bool releaseWhiteShapes(std::string& error);
bool whiteShapesRetired(std::string& error);
}
