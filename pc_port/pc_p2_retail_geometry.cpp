#include "pc_p2_retail_geometry.h"
#include <cstring>
#include <map>
#include <stdexcept>

namespace p2retail {namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Binary {
 const std::string& bytes;
 void bound(std::size_t offset,std::size_t count)const{require(offset<=bytes.size()&&count<=bytes.size()-offset,"retail MOD truncated");}
 std::uint32_t u32(std::size_t offset)const{bound(offset,4);std::uint32_t value=0;for(unsigned i=0;i<4;++i)value=(value<<8)|static_cast<unsigned char>(bytes[offset+i]);return value;}
 std::uint16_t u16(std::size_t offset)const{bound(offset,2);return (static_cast<unsigned char>(bytes[offset])<<8)|static_cast<unsigned char>(bytes[offset+1]);}
 double number(std::size_t offset)const{const auto bits=u32(offset);float value;std::memcpy(&value,&bits,4);require(std::isfinite(value),"retail MOD nonfinite");return value;}
};
std::string lf(const std::string& bytes){std::string out;for(std::size_t i=0;i<bytes.size();++i){
 if(bytes[i]=='\r'){require(i+1<bytes.size()&&bytes[i+1]=='\n',"retail INI invalid CR");continue;}out+=bytes[i];}return out;}
struct RoutePoint {std::array<double,3> position;double width;};
class Ini {
 std::istringstream input;
public:
 explicit Ini(const std::string& bytes):input(bytes){input.imbue(std::locale::classic());}
 std::string token(){std::string next;require(bool(input>>next),"retail INI truncated");return next;}
 void token(const char* expected){require(token()==expected,"retail INI framing");}
 double number(){const auto text=token();std::istringstream value(text);value.imbue(std::locale::classic());double result;std::string extra;
  require(bool(value>>result)&&!(value>>extra)&&std::isfinite(result),"retail INI finite number");return result;}
 unsigned integer(unsigned maximum){const auto value=number();require(value>=0&&value<=maximum&&std::floor(value)==value,"retail INI integer bound");return unsigned(value);}
 void end(){std::string extra;require(!(input>>extra),"retail INI trailing bytes");}
};
void routes(const std::string& bytes,GeometryFacts& facts){
 require(!bytes.empty()&&bytes.size()<=4*1024*1024,"retail INI size");Ini input(bytes);
 input.token("route");input.token("{");input.token("id");input.token("test");
 input.token("name");input.token("'P2");input.token("room");input.token("prototype'");
 input.token("colour");for(unsigned i=0;i<4;++i)(void)input.integer(255);
 std::map<unsigned,RoutePoint> points;std::set<std::pair<unsigned,unsigned>> links;
 std::string next;
 while((next=input.token())!="}"){
  if(next=="point"){
   input.token("{");input.token("index");const unsigned id=input.integer(10000);
   input.token("state");require(input.integer(1)==1,"retail route disabled point");input.token("pos");RoutePoint point;
   for(auto& value:point.position){value=input.number();require(std::fabs(value)<=100000,"retail route position bound");}
   input.token("width");point.width=input.number();require(point.width>0&&point.width<=10000,"retail route width");input.token("}");
   require(points.emplace(id,point).second,"retail route duplicate point");
  }else if(next=="link"){
   input.token("{");const auto from=input.integer(10000);const auto to=input.integer(10000);input.token("}");
   require(from!=to&&links.emplace(from,to).second,"retail route duplicate/self link");
  }else throw std::runtime_error("retail route unknown token");
 }
 input.end();require(!points.empty()&&points.size()<=10000,"retail route point count");
 for(unsigned i=0;i<points.size();++i)require(points.count(i)!=0,"retail route nonconsecutive native indices");
 std::map<unsigned,unsigned> degree;
 for(const auto& link:links){require(points.count(link.first)&&points.count(link.second),"retail route missing destination");
  require(++degree[link.first]<=8,"retail route native link capacity");}
 facts.routePoints=unsigned(points.size());facts.routeLinks=unsigned(links.size());
}
std::string digest(const std::string& bytes){unsigned char hash[32];pc_netplay_sha::sha256(bytes.data(),bytes.size(),hash);return pc_netplay_sha::hex(hash,32);}
}
bool parseRetailGeometry(const SelectedSceneInputs& inputs,GeometryFacts& out,std::string& error){
 try{
  const auto& bytes=inputs.bytes[1];Binary binary{bytes};GeometryFacts next;
  require(bytes.size()<=64*1024*1024,"retail MOD size");
  std::map<unsigned,std::pair<std::size_t,std::size_t>> chunks;std::size_t cursor=0;bool ended=false;
  while(cursor<bytes.size()){
   require(cursor%32==0,"retail MOD chunk alignment");const auto tag=binary.u32(cursor);const auto length=binary.u32(cursor+4);
   require(length>=24&&(length+8)%32==0,"retail MOD chunk length");binary.bound(cursor+8,length);
   require(chunks.emplace(tag,std::make_pair(cursor,length)).second,"retail MOD duplicate chunk");
   cursor+=8+length;if(tag==0xffff){ended=true;break;}
   require(chunks.size()<=64,"retail MOD chunk count");
  }
  require(ended&&chunks.count(0)&&chunks.count(0x10)&&chunks.count(0x50)&&chunks.count(0x60)&&chunks.count(0x100)&&chunks.count(0x110),"retail MOD required chunks");
  const auto vertex=chunks.at(0x10).first;next.vertices=binary.u32(vertex+8);
  require(next.vertices>0&&next.vertices<=1000000&&std::size_t(next.vertices)*12<=chunks.at(0x10).second-24,"retail MOD vertex count");
  for(unsigned i=0;i<next.vertices*3;++i)require(std::fabs(binary.number(vertex+32+std::size_t(i)*4))<=100000,"retail MOD vertex extent");
  const auto collision=chunks.at(0x100).first;next.triangles=binary.u32(collision+8);
  require(next.triangles>0&&next.triangles<=32767&&binary.u32(collision+12)==1&&
   std::size_t(next.triangles)*40+56<=chunks.at(0x100).second,"retail MOD collision count/room");
  for(unsigned i=0;i<next.triangles;++i){const auto record=collision+64+std::size_t(i)*40;
   for(unsigned j=0;j<3;++j)require(binary.u32(record+4+j*4)<next.vertices,"retail MOD collision vertex index");
   require(binary.u16(record+16)==0,"retail MOD collision room index");
   for(unsigned j=0;j<3;++j){const auto neighbour=binary.u16(record+18+j*2);require(neighbour==65535||neighbour<next.triangles,"retail MOD collision adjacency");}
   double norm=0;for(unsigned j=0;j<3;++j){const double value=binary.number(record+24+j*4);norm+=value*value;}
   require(std::fabs(norm-1)<.001&&std::fabs(binary.number(record+36))<=200000,"retail MOD collision plane");
  }
  const auto grid=chunks.at(0x110).first;const auto gridSize=binary.number(grid+56);
  const auto x=binary.u32(grid+60),z=binary.u32(grid+64),groups=binary.u32(grid+68);
  require(gridSize==64&&x>0&&z>0&&x<=4096&&z<=4096&&groups==1&&binary.u16(grid+72)==0&&binary.u16(grid+74)==next.triangles,"retail MOD source grid dimensions");
  require(68+std::size_t(next.triangles)*4+std::size_t(x)*z*4<=chunks.at(0x110).second,"retail MOD source grid bounds");
  for(unsigned i=0;i<6;++i)(void)binary.number(grid+32+i*4);
  for(unsigned i=0;i<next.triangles;++i)require(binary.u32(grid+76+std::size_t(i)*4)==i,"retail MOD source grid triangle mapping");
  for(std::size_t i=0;i<std::size_t(x)*z;++i)require(binary.u32(grid+76+std::size_t(next.triangles)*4+i*4)==0,"retail MOD source grid cell mapping");
  require(binary.u32(chunks.at(0x60).first+8)==1,"retail MOD static native joint");
  require(binary.u32(chunks.at(0x50).first+8)>0,"retail MOD missing native render mesh");
  const auto actualRoutes=lf(inputs.bytes[2]);require(lf(bytes.substr(cursor))==actualRoutes,"retail MOD embedded/selected INI mismatch");routes(actualRoutes,next);
  const unsigned floor=inputs.selection.floor;require(inputs.selection.cave=="tutorial_1"&&(floor==1||floor==2),"retail geometry unsupported floor");
  static constexpr const char* modelHashes[]={"3eeacfa55ef129b2738caa4e821d3d77d61dcfce4072482dca319765c6c1bd1e","9e5b4992c4ad38331092805185504e7f3d129bc18998dc6bacf682a0ad8f63bf"};
  static constexpr const char* routeHashes[]={"2464821bafcf7ef5d04bb3d2f7d5f44703f31cd29fefcb4c71d7c1dbdb1e2d73","39958fac61db34f248f19bb486455be24d751ff951d96f332ce3ffa10265e00c"};
  require(digest(bytes)==modelHashes[floor-1]&&digest(inputs.bytes[2])==routeHashes[floor-1]&&
   inputs.plan.geometrySha256==modelHashes[floor-1]&&inputs.plan.routesSha256==routeHashes[floor-1],"retail geometry authenticated source mapping");
  out=next;error.clear();return true;
 }catch(const std::runtime_error& failure){error=failure.what();return false;}
}
}
