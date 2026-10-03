#pragma once
#include "pc_p2_original_number_triangle.h"
#include "pc_p2_original_number_grid.h"
#include <cstdint>
namespace p2originalnumber { namespace roomTrace {
using Vec3=rigid::Vec3;
struct TriangleInput { std::array<unsigned,3> abc{}; const void* original=nullptr; int roomIndex=-1; std::uint8_t mapCode=0; };
struct ContactIdentity { const void* original=nullptr; unsigned index=0; int roomIndex=-1; std::uint8_t mapCode=0; };
struct PreparedTriangle { triangle::Vertices vertices; triangle::Geometry geometry; ContactIdentity identity; };
// Preparation is pure data and confers no geometry/scene/body authority.
struct Mesh { std::vector<PreparedTriangle> triangles; motion::CaveGrid grid; };
bool prepare(const std::vector<Vec3>& orderedVertices,const std::vector<TriangleInput>& orderedABC,Mesh&,std::string&);
struct Hidden { bool enabled=false; ContactIdentity sentinel; };
class Owner {
public:
 virtual ~Owner()=default;
 // Actual scene/body/lifetime validation. No permissive default implementation.
 virtual bool current(const Mesh&,std::string& error)=0;
 // Actual owner's current floor flag AND actual sentinel. Unavailable refuses
 // before tracing effects; enabled requires a non-null original sentinel.
 virtual bool hidden(const Mesh&,Hidden&,std::string& error)=0;
};
struct Move {
 rigid::Trace sphere;
 float floorThreshold=.6f,wallThreshold=.70710677f;
 bool hasFloor=false,hasWall=false;
 ContactIdentity floor,wall;
 Vec3 floorNormal,wallNormal,baseSpherePosition,unusedNormal;
 int roomIndex=-1;
 std::uint8_t mapCode=0; // source traceMove_new deliberately does not assign it
};
class Callback {
public:
 virtual ~Callback()=default;
 // Ordinary: roomIndex assigned, BEFORE floor/wall/response/correction.
 // Hidden: exceptional source phase AFTER pop/response/floor assignment.
 // Source references permit callback mutation: ordinary classification and
 // restitution use mutated normal, position correction uses pre-callback normal.
 virtual bool contact(const ContactIdentity&,Vec3& point,Vec3& normal,const Move&,std::string&)=0;
};
struct Report { unsigned substeps=0,candidates=0,contacts=0; bool hidden=false; };
// RoomMap map-only max8 substeps, grid cell repeats and ABC edge order retained.
// Does not invent platform tracing or native Shape/Pellet/world ownership.
// State/report failure atomic; callback external effects cannot be rolled back.
bool traceMap(const Mesh&,Owner&,Move&,float dt,Callback*,Report&,std::string&);
} }
