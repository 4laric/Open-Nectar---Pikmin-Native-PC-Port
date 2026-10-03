#include "pc_p2_original_room_height.h"
#include "pc_p2_original_room_inverse.h"
#include "pc_p2_original_number_triangle.h"
#include <cmath>
#include <cstring>
#include <limits>

// Original research HEAD632af937: geometry.cpp1638/2876/3570,
// gameMapParts.cpp4284/4903, routeMgr.cpp426; source audit25. Local raw
// planes and original serialized lists are distinct from combined movement.
namespace p2originalnumber { namespace roomHeight {
namespace {
float add(float a,float b){volatile float r=a+b;return r;}
float sub(float a,float b){volatile float r=a-b;return r;}
float mul(float a,float b){volatile float r=a*b;return r;}
float divide(float a,float b){volatile float r=a/b;return r;}
float fused(float a,float b,float c){float r;if(!triangle::sourceFma(a,b,c,r))return std::numeric_limits<float>::quiet_NaN();return r;}
bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
bool fail(std::string& e,const char* s){e=s;return false;}
bool environment(std::string& e){float r;return triangle::sourceFma(0,0,0,r)||fail(e,"source height arithmetic environment unavailable");}
float value(std::uint32_t bits){float out;std::memcpy(&out,&bits,4);return out;}
bool cell(float position,float origin,float scale,unsigned count,unsigned& out,std::string& e){
 const float coordinate=divide(sub(sub(position,0.f),origin),scale);
 // Never invoke undefined host float-to-int conversion. The supported source
 // query domain excludes invalid fctiwz operands, rather than guessing FPSCR.
 if(!std::isfinite(coordinate)||double(coordinate)<-2147483648.||double(coordinate)>=2147483648.)return fail(e,"source height grid conversion unavailable");
 const int index=static_cast<int>(coordinate);
 out=index<0?0:unsigned(index)>=count?count-1:unsigned(index);return true;
}
bool candidateCell(const UnitGrid& unit,Vec3 local,unsigned& out,std::string& e){
 if(!unit.maxX||!unit.maxZ||unit.maxX>unsigned(std::numeric_limits<int>::max())||unit.maxZ>unsigned(std::numeric_limits<int>::max())
    ||std::uint64_t(unit.maxX)*unit.maxZ!=unit.cells.size()||!finite(unit.minimum)||!finite(unit.maximum))return fail(e,"source height original unit grid unavailable");
 // GridDivider::read overwrites the serialized scale values with these.
 const float sx=divide(std::fabs(sub(unit.maximum.x,unit.minimum.x)),float(unit.maxX));
 const float sz=divide(std::fabs(sub(unit.maximum.z,unit.minimum.z)),float(unit.maxZ));
 if(!std::isfinite(sx)||!std::isfinite(sz)||sx<=0||sz<=0)return fail(e,"source height original unit scales invalid");
 unsigned x,z;if(!cell(local.x,unit.minimum.x,sx,unit.maxX,x,e)||!cell(local.z,unit.minimum.z,sz,unit.maxZ,z,e))return false;
 const std::uint64_t index=std::uint64_t(z)+std::uint64_t(x)*unit.maxZ;
 if(index>std::numeric_limits<unsigned>::max())return fail(e,"source height original cell index unavailable");
 out=unsigned(index);return true;
}
}
bool transformBounds(const Bounds& source,const room::Matrix3x4& matrix,Bounds& out,std::string& e){
 if(!environment(e)||!finite(source.minimum)||!finite(source.maximum))return fail(e,"source bounding box unavailable");
 std::array<Vec3,8> corners;
 for(unsigned i=0;i<8;++i){const Vec3 p{i&1?source.maximum.x:source.minimum.x,i&2?source.maximum.y:source.minimum.y,i&4?source.maximum.z:source.minimum.z};
  if(!room::transformVertex(matrix,p,corners[i],e))return false;
 }
 Bounds b{{32768,32768,32768},{-32768,-32768,-32768}};
 for(const auto p:corners){
  if(p.x<b.minimum.x)b.minimum.x=p.x;
  if(p.y<b.minimum.y)b.minimum.y=p.y;
  if(p.z<b.minimum.z)b.minimum.z=p.z;
  if(p.x>b.maximum.x)b.maximum.x=p.x;
  if(p.y>b.maximum.y)b.maximum.y=p.y;
  if(p.z>b.maximum.z)b.maximum.z=p.z;
 }
 out=b;e.clear();return true;
}
bool boundSphere(const Bounds& b,Sphere& out,std::string& e){
 if(!environment(e)||!finite(b.minimum)||!finite(b.maximum))return fail(e,"source sphere bounding box unavailable");
 Sphere s;s.center={mul(add(b.minimum.x,b.maximum.x),.5f),mul(add(b.minimum.y,b.maximum.y),.5f),mul(add(b.minimum.z,b.maximum.z),.5f)};
 if(!finite(s.center))return fail(e,"source sphere midpoint overflow");
 const auto distance=[&](Vec3 v,float& result){const Vec3 d{sub(s.center.x,v.x),sub(s.center.y,v.y),sub(s.center.z,v.z)};
  const float squared=fused(d.z,d.z,fused(d.x,d.x,mul(d.y,d.y)));
  if(!std::isfinite(squared))return false;
  if(squared<=0){result=0;return true;}
  return triangle::sourceSqrt(squared,result);
 };
 float lo,hi;if(!distance(b.minimum,lo)||!distance(b.maximum,hi))return fail(e,"source bounding sphere qdist3 arithmetic refused");
 s.radius=lo>hi?lo:hi;out=s;e.clear();return true;
}
bool roomBounds(const Bounds& source,const room::Matrix3x4& matrix,Bounds& bounds,Sphere& sphere,std::string& e){
 Bounds b;if(!transformBounds(source,matrix,b,e))return false;
 Bounds flat=b;flat.minimum.y=0;flat.maximum.y=0;Sphere s;if(!boundSphere(flat,s,e))return false;
 bounds=b;sphere=s;e.clear();return true;
}
bool insideXZ(const std::array<std::uint32_t,16>& bits,Vec3& out,bool& inside,std::string& e){
 if(!environment(e)||!finite(out))return fail(e,"source height point arithmetic unavailable");
 std::array<float,16> p;for(unsigned i=0;i<16;++i){p[i]=value(bits[i]);if(!std::isfinite(p[i]))return fail(e,"source height literal plane nonfinite");}
 if(p[1]<=0){inside=false;e.clear();return true;}
 Vec3 point=out;point.y=divide(sub(p[3],fused(p[0],point.x,mul(p[2],point.z))),p[1]);
 if(!finite(point))return fail(e,"source height projection arithmetic refused");
 bool accepted=true;
 for(unsigned edge=0;edge<3;++edge){const unsigned i=4+edge*4;const float distance=sub(fused(point.z,p[i+2],fused(point.x,p[i],mul(point.y,p[i+1]))),p[i+3]);
  if(!std::isfinite(distance))return fail(e,"source height edge arithmetic refused");
  if(distance>0){accepted=false;break;}
 }
 out=point;inside=accepted;e.clear();return true;
}
bool query(const std::vector<Room>& rooms,Owner& owner,Query& out,std::string& e){
 if(!environment(e)||!owner.current(rooms,e))return false;
 if(!finite(out.position)||!finite(out.normal)||!std::isfinite(out.minY)||!std::isfinite(out.maxY))return fail(e,"source room height query nonfinite");
 Hidden hidden;if(!owner.hidden(hidden,e)||!owner.current(rooms,e))return false;
 if(hidden.enabled&&!hidden.sentinel.original)return fail(e,"source room hidden floor sentinel unavailable");
 Query q=out;q.maxY=328000;
 for(std::size_t ri=0;ri<rooms.size();++ri){
  if(!owner.current(rooms,e))return false;
  const auto room=rooms[ri];
  if(!room.unit||!finite(room.sphereCenter)||!std::isfinite(room.sphereRadius)||room.sphereRadius<0)return fail(e,"source height actual room unavailable");
  const float dx=sub(room.sphereCenter.x,q.position.x),dz=sub(room.sphereCenter.z,q.position.z);
  const float distance=fused(dx,dx,mul(dz,dz)),radiusSquared=mul(room.sphereRadius,room.sphereRadius);
  if(!std::isfinite(distance)||!std::isfinite(radiusSquared))return fail(e,"source height room gate arithmetic refused");
  if(distance>radiusSquared)continue;
  Vec3 local;if(!room::transformVertex(room.inverse,q.position,local,e))return false;
  if(!owner.current(rooms,e))return false;
  unsigned cellIndex;if(!candidateCell(*room.unit,local,cellIndex,e))return false;
  const auto indices=room.unit->cells[cellIndex];
  for(unsigned index:indices){
   if(!owner.current(rooms,e))return false;
   if(index>=room.unit->triangles.size())return fail(e,"source height original triangle index invalid");
   const auto tri=room.unit->triangles[index];if(!tri.original)return fail(e,"source height original triangle identity unavailable");
   bool inside;if(!insideXZ(tri.planeBits,local,inside,e))return false;
   if(!inside)continue;
   bool selected=false;
   if(q.maxY>local.y){q.maxY=local.y;selected=q.updateOnNewMaxY;}
   if(q.minY<local.y){q.minY=local.y;selected=selected||!q.updateOnNewMaxY;}
   if(selected){
    Vec3 normal{value(tri.planeBits[0]),value(tri.planeBits[1]),value(tri.planeBits[2])};
    if(!roomInverse::normalTranspose(room.inverse,normal,q.normal,e))return false;
    q.triangle={tri.original,index,room.roomIndex};
   }
  }
 }
 if(hidden.enabled&&q.minY<0){q.minY=0;q.maxY=0;q.triangle=hidden.sentinel;}
 else if(!q.triangle.original){q.minY=0;q.maxY=0;}
 if(!owner.current(rooms,e))return false;
 out=q;e.clear();return true;
}
bool minY(const std::vector<Room>& rooms,Owner& owner,Vec3 point,float& out,std::string& e){Query q;q.position=point;q.updateOnNewMaxY=false;if(!query(rooms,owner,q,e))return false;out=q.minY;return true;}
bool linkable(HeightProvider& provider,Vec3 a,Vec3 b,bool& out,std::string& e){
 if(!environment(e)||!finite(a)||!finite(b))return fail(e,"source route link endpoints unavailable");
 Vec3 sep{sub(b.x,a.x),0,sub(b.z,a.z)};if(!finite(sep))return fail(e,"source route link separation overflow");
 float previous=a.y;
 for(float t=0;t<=1;t=add(t,.1f)){
  const Vec3 point{add(a.x,mul(sep.x,t)),add(a.y,mul(sep.y,t)),add(a.z,mul(sep.z,t))};
  if(!finite(point))return fail(e,"source route link sample overflow");
  float height;if(!provider.minY(point,height,e))return false;
  const float delta=sub(previous,height);if(!std::isfinite(height)||!std::isfinite(delta))return fail(e,"source route link height arithmetic refused");
  if(std::fabs(delta)>25){out=false;e.clear();return true;}
  previous=height;
 }
 out=true;e.clear();return true;
}
}}
