#include "pc_p2_original_number_grid.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace p2originalnumber { namespace motion {
namespace {
bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
bool fail(std::string& e,const char* text){e=text;return false;}
}
bool buildCaveGrid(const std::vector<Vec3>& vertices,const std::vector<std::array<unsigned,3>>& triangles,CaveGrid& out,std::string& e){
 if(vertices.empty()||vertices.size()>1048576||triangles.empty()||triangles.size()>1048576)return fail(e,"source combined collision table bound invalid");
 CaveGrid grid;grid.minimum=grid.maximum=vertices.front();
 for(const auto v:vertices){
  if(!finite(v))return fail(e,"source combined collision vertex nonfinite");
  grid.minimum.x=std::min(grid.minimum.x,v.x);grid.minimum.y=std::min(grid.minimum.y,v.y);grid.minimum.z=std::min(grid.minimum.z,v.z);
  grid.maximum.x=std::max(grid.maximum.x,v.x);grid.maximum.y=std::max(grid.maximum.y,v.y);grid.maximum.z=std::max(grid.maximum.z,v.z);
 }
 for(const auto& tri:triangles)for(const auto index:tri)if(index>=vertices.size())return fail(e,"source combined collision triangle index invalid");
 grid.minimum.x-=10;grid.minimum.y-=10;grid.minimum.z-=10;
 grid.maximum.x+=10;grid.maximum.y+=10;grid.maximum.z+=10;
 const float extentX=std::fabs(grid.maximum.x-grid.minimum.x),extentZ=std::fabs(grid.maximum.z-grid.minimum.z);
 if(!finite(grid.minimum)||!finite(grid.maximum)||!std::isfinite(extentX)||!std::isfinite(extentZ)||
    extentX>=float(std::numeric_limits<int>::max())||extentZ>=float(std::numeric_limits<int>::max()))return fail(e,"source combined collision extent invalid");
 // Preserve BOTH source casts: truncate extent to int, convert back to f32,
 // divide by 64, then truncate the result to int before the 48-cell cap.
 const int x=int(float(int(extentX))/64.0f),z=int(float(int(extentZ))/64.0f);
 if(x<=0||z<=0)return fail(e,"source combined collision has zero cell dimension");
 grid.countX=unsigned(std::min(48,x));grid.countZ=unsigned(std::min(48,z));
 grid.scaleX=extentX/float(grid.countX);grid.scaleZ=extentZ/float(grid.countZ);
 grid.cells.resize(grid.countX*grid.countZ);
 for(unsigned ix=0;ix<grid.countX;++ix)for(unsigned iz=0;iz<grid.countZ;++iz){
  const float minX=grid.minimum.x+float(ix)*grid.scaleX,maxX=minX+grid.scaleX;
  const float minZ=grid.minimum.z+float(iz)*grid.scaleZ,maxZ=minZ+grid.scaleZ;
  auto& list=grid.cells[iz+ix*grid.countZ];
  for(unsigned i=0;i<triangles.size();++i){
   float loX=12800000.0f,loZ=12800000.0f,hiX=-12800000.0f,hiZ=-12800000.0f;
   for(const auto index:triangles[i]){const auto v=vertices[index];loX=std::min(loX,v.x);loZ=std::min(loZ,v.z);hiX=std::max(hiX,v.x);hiZ=std::max(hiZ,v.z);}
   // Source Triangle::intersect(BoundBox2d): projected AABB overlap, inclusive.
   if(!(maxX<loX||hiX<minX||maxZ<loZ||hiZ<minZ)&&list.size()<1024)list.push_back(i);
  }
 }
 out=std::move(grid);e.clear();return true;
}
bool caveCandidates(const CaveGrid& grid,Vec3 p,float radius,std::vector<unsigned>& out,std::string& e){
 if(!finite(p)||!std::isfinite(radius)||radius<0||!finite(grid.minimum)||!finite(grid.maximum)||
    !grid.countX||grid.countX>48||!grid.countZ||grid.countZ>48||grid.cells.size()!=grid.countX*grid.countZ||
    !std::isfinite(grid.scaleX)||grid.scaleX<=0||!std::isfinite(grid.scaleZ)||grid.scaleZ<=0)return fail(e,"source cave candidate scope invalid");
 const float a=((p.x-radius)-grid.minimum.x)/grid.scaleX,b=((p.z-radius)-grid.minimum.z)/grid.scaleZ;
 const float c=((p.x+radius)-grid.minimum.x)/grid.scaleX,d=((p.z+radius)-grid.minimum.z)/grid.scaleZ;
 if(!std::isfinite(a)||!std::isfinite(b)||!std::isfinite(c)||!std::isfinite(d))return fail(e,"source cave candidate extent nonfinite");
 const auto cell=[](float v,unsigned count){return v<0?0u:v>=float(count)?count-1:unsigned(int(v));};
 const unsigned minX=cell(a,grid.countX),minZ=cell(b,grid.countZ),maxX=cell(c,grid.countX),maxZ=cell(d,grid.countZ);
 std::vector<unsigned> candidate;
 for(unsigned x=minX;x<=maxX;++x)for(unsigned z=minZ;z<=maxZ;++z){
  const auto& list=grid.cells[z+x*grid.countZ];if(list.size()>1024)return fail(e,"source cave candidate list exceeds source cap");
  candidate.insert(candidate.end(),list.begin(),list.end());
 }
 out=std::move(candidate);e.clear();return true;
}
}}
