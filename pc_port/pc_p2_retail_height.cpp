#include "pc_p2_retail_height.h"
#include "pc_p2_original_room_inverse.h"
#include <cstring>
#include <cmath>
#include <stdexcept>
namespace p2retail {
namespace H=p2originalnumber::roomHeight;
namespace {
float decode(std::uint32_t bits){float value;std::memcpy(&value,&bits,4);if(!std::isfinite(value))throw std::runtime_error("source height nonfinite bound");return value;}
}
bool adoptSourceHeightInputs(const SourceRoomCensus& census,const SourceRoomGeometry& geometry,std::unique_ptr<SourceHeightInputs>& out,std::string& error){
 try{
  if(census.sha256.empty()||geometry.censusSha256!=census.sha256||census.rooms.empty()||geometry.rooms.size()!=census.rooms.size())
   throw std::runtime_error("source height room adoption binding");
  auto next=std::make_unique<SourceHeightInputs>();next->units.reserve(census.units.size());
  for(const auto& unit:census.units){
   if(unit.triangles.empty()||unit.sourcePlaneBits.size()!=unit.triangles.size()||!unit.sourceGrid.maxX||!unit.sourceGrid.maxZ||
      unit.sourceGrid.cells.size()!=std::uint64_t(unit.sourceGrid.maxX)*unit.sourceGrid.maxZ)throw std::runtime_error("source height original unit storage unavailable");
   H::UnitGrid grid;grid.minimum={decode(unit.sourceBounds[0]),decode(unit.sourceBounds[1]),decode(unit.sourceBounds[2])};
   grid.maximum={decode(unit.sourceBounds[3]),decode(unit.sourceBounds[4]),decode(unit.sourceBounds[5])};
   grid.maxX=unit.sourceGrid.maxX;grid.maxZ=unit.sourceGrid.maxZ;grid.cells=unit.sourceGrid.cells;
   for(unsigned t=0;t<unit.sourcePlaneBits.size();++t)grid.triangles.push_back({unit.sourcePlaneBits[t],&unit.sourcePlaneBits[t]});
   for(const auto& cell:grid.cells)for(unsigned index:cell)if(index>=grid.triangles.size())throw std::runtime_error("source height original cell index invalid");
   next->units.push_back(std::move(grid));
  }
  for(unsigned r=0;r<census.rooms.size();++r){const auto& source=census.rooms[r];const auto& matrix=geometry.rooms[r];
   if(source.roomIndex!=r||source.iteration!=r||matrix.roomIndex!=r||matrix.unit!=source.unit||source.unit>=next->units.size())
    throw std::runtime_error("source height actual room order unavailable");
   H::Room room;room.unit=&next->units[source.unit];room.roomIndex=int(r);
   if(!p2originalnumber::roomInverse::inverse(matrix.matrix,room.inverse,error))return false;
   H::Bounds bounds;H::Sphere sphere;
   if(!H::roomBounds({room.unit->minimum,room.unit->maximum},matrix.matrix,bounds,sphere,error))return false;
   room.sphereCenter=sphere.center;room.sphereRadius=sphere.radius;
   next->transformedBounds.push_back(bounds);next->rooms.push_back(room);
  }
  out=std::move(next);error.clear();return true;
 }catch(const std::exception& exception){error=exception.what();return false;}
}
}
