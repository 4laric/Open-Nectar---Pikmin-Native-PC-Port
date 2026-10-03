#pragma once
#include "pc_p2_original_number_room.h"
#include <cstdint>
#include <vector>
namespace p2originalnumber { namespace roomHeight {
using Vec3=room::Vec3;
struct Bounds { Vec3 minimum,maximum; };
struct Sphere { Vec3 center; float radius=0; };
// Original serialized bounding box, eight-corner SDK transform and source
// SHORT_FLOAT_MAX sentinels. Sphere uses the distinct unguarded qdist3 order.
bool transformBounds(const Bounds&,const room::Matrix3x4&,Bounds&,std::string&);
bool boundSphere(const Bounds&,Sphere&,std::string&);
// Room _190: retain full transformed bounds, then zero both Y endpoints in
// a separate copy before makeBoundSphere. No vertex-bounds replacement.
bool roomBounds(const Bounds&,const room::Matrix3x4&,Bounds&,Sphere&,std::string&);
struct Identity { const void* original=nullptr; unsigned index=0; int roomIndex=-1; };
// Literal grid.bin face, AB, BC, CA planes, each (nx,ny,nz,offset).
// Preparation does not reconstruct planes or confer geometry authority.
struct RawTriangle { std::array<std::uint32_t,16> planeBits{}; const void* original=nullptr; };
struct UnitGrid {
 Vec3 minimum,maximum;
 unsigned maxX=0,maxZ=0;
 std::vector<RawTriangle> triangles;
 std::vector<std::vector<unsigned>> cells; // original z+x*maxZ; duplicates retained
};
struct Room {
 const UnitGrid* unit=nullptr;
 room::Matrix3x4 inverse{}; // actual source PSMTXInverse bits, never ideal rotation
 Vec3 sphereCenter; float sphereRadius=0; // actual transformed serialized bbox _190
 int roomIndex=-1;
};
struct Query {
 Vec3 position;
 bool updateOnNewMaxY=true,getFullInfo=false;
 const void* table=nullptr;
 Identity triangle;
 float maxY=128000,minY=-128000;
 Vec3 normal{0,1,0};
};
struct Hidden { bool enabled=false; Identity sentinel; };
class Owner {
public:
 virtual ~Owner()=default;
 // Authenticate the actual ordered room/unit storage before borrowed reads.
 // Construction ownership and runtime body ownership are separate contracts.
 virtual bool current(const std::vector<Room>&,std::string&)=0;
 virtual bool hidden(Hidden&,std::string&)=0;
};
// On a valid edge miss, source insideXZ still publishes projected point.y.
// ny<=0 leaves point unchanged. Arithmetic refusal preserves both outputs.
bool insideXZ(const std::array<std::uint32_t,16>&,Vec3& point,bool& inside,std::string&);
// RoomMapMgr contract only; this is NOT GridDivider::getCurrTri/getMinY.
// Preserves incoming selector/minY/triangle/normal and unused table/fullInfo.
// Outputs unchanged on ownership/arithmetic refusal; genuine no-hit is success.
bool query(const std::vector<Room>&,Owner&,Query&,std::string&);
bool minY(const std::vector<Room>&,Owner&,Vec3,float&,std::string&);
class HeightProvider {
public:
 virtual ~HeightProvider()=default;
 virtual bool minY(Vec3,float&,std::string&)=0; // actual available source query
};
// Ten source binary32 samples; no-hit zero is valid, unavailable query refuses.
bool linkable(HeightProvider&,Vec3 a,Vec3 b,bool&,std::string&);
}}
