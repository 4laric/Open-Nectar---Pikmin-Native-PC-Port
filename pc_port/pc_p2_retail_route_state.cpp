#include "pc_p2_retail_route_state.h"
#include "pc_p2_original_number_room.h"
#include <algorithm>
#include <stdexcept>
namespace p2retail {
bool openSourceRoom(SourceRouteState& state,unsigned roomIndex,std::string& error){
 if(roomIndex>=state.visited.size()){error="source visit room unavailable";return false;}
 if(!state.visited[roomIndex]){
  for(auto& point:state.points)if(std::find(point.rooms.begin(),point.rooms.end(),roomIndex)!=point.rooms.end())
   point.flags=static_cast<unsigned char>(point.flags&~0x80u);
  state.visited[roomIndex]=true;
 }
 error.clear();return true;
}
bool adoptSourceRouteState(const SourceRoomCensus& census,const SourceRoomGeometry& geometry,const SourceRouteInputs& input,SourceRouteMap& map,std::unique_ptr<SourceRouteState>& out,std::string& error){
 try{
  if(!map.current(error))return false;
  if(input.roomCensusSha256!=census.sha256||geometry.censusSha256!=census.sha256||input.units.size()!=census.units.size()||
     input.roomIndices.size()!=census.rooms.size()||geometry.rooms.size()!=census.rooms.size()||input.points.empty()||input.points.size()>256)
   throw std::runtime_error("source route current construction binding");
  auto next=std::make_unique<SourceRouteState>();next->visited.assign(census.rooms.size(),false);next->points.reserve(input.points.size());
  for(const auto& record:input.points){
   if(!map.current(error))return false;
   if(record.createdRoom>=census.rooms.size())throw std::runtime_error("source route first-created room bound");
   const auto& room=census.rooms[record.createdRoom];
   if(room.unit>=input.units.size()||geometry.rooms[record.createdRoom].unit!=room.unit||geometry.rooms[record.createdRoom].roomIndex!=record.createdRoom||
      record.createdWaypoint>=input.units[room.unit].waypoints.size()||record.fromCount>8)throw std::runtime_error("source route first-created waypoint bound");
   const auto local=input.units[room.unit].waypoints[record.createdWaypoint].positionRadius;
   if(!map.beginRoomPrefix(record.createdRoom,error)||!map.current(error))return false;
   p2originalnumber::room::Vec3 position;
   if(!p2originalnumber::room::transformVertex(geometry.rooms[record.createdRoom].matrix,{local[0],local[1],local[2]},position,error))return false;
   if(record.door)position.y=0;
   else if(!map.minY(position,position.y,error))return false;
   if(!map.current(error))return false;
   SourceWayPoint point;point.position={position.x,position.y,position.z};point.radius=record.radius;
   point.fromCount=record.fromCount;point.from=record.fromLinks;point.rooms=record.rooms;next->points.push_back(std::move(point));
  }
  if(!map.finishRoomConstruction(error)||!map.current(error))return false;
  // Original makeInvertLinks calls linkable BEFORE reverse From lookup and
  // checks To capacity even when a reverse From would avoid an append.
  for(unsigned index=0;index<next->points.size();++index){const auto& point=next->points[index];
   for(unsigned slot=0;slot<point.fromCount;++slot){const int destination=point.from[slot];if(destination==-1)continue;
    if(!map.current(error))return false;
    if(destination<0||unsigned(destination)>=next->points.size())throw std::runtime_error("source route destination bound");
    auto& linked=next->points[unsigned(destination)];bool linkable=false;
    if(!p2originalnumber::roomHeight::linkable(map,{point.position[0],point.position[1],point.position[2]},
       {linked.position[0],linked.position[1],linked.position[2]},linkable,error)||!map.current(error))return false;
    if(!linkable)continue;
    if(linked.toCount>=8)throw std::runtime_error("source route inverse capacity");
    if(std::find(linked.from.begin(),linked.from.begin()+linked.fromCount,int(index))==linked.from.begin()+linked.fromCount)
     linked.to[linked.toCount++]=int(index);
   }
  }
  for(auto& point:next->points)point.flags=static_cast<unsigned char>(point.flags|0x80);
  if(!map.current(error))return false;
  out=std::move(next);error.clear();return true;
 }catch(const std::exception& exception){error=exception.what();return false;}
}
}
