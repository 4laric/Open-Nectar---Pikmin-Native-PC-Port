#include "pc_p2_retail_rooms.h"
#include <map>
#include <stdexcept>
#include <cstring>
#include <algorithm>

namespace p2retail { namespace {
struct Json {
 enum Kind {Object,Array,String,Number,False,True,Null} kind=Null;
 std::map<std::string,Json> object;
 std::vector<Json> array;
 std::string string;
 double number=0;
 const Json& at(const char* key)const{
  if(kind!=Object)throw std::runtime_error("room JSON expected object");
  const auto found=object.find(key);
  if(found==object.end())throw std::runtime_error("room JSON missing field");
  return found->second;
 }
 bool text(const std::string& value)const{return kind==String&&string==value;}
 bool numeric(double value)const{return kind==Number&&number==value;}
};
class JsonReader {
 const std::string& bytes;std::size_t pos=0;unsigned nodes=0;
 void space(){while(pos<bytes.size()&&(bytes[pos]==' '||bytes[pos]=='\n'||bytes[pos]=='\r'||bytes[pos]=='\t'))++pos;}
 bool take(char ch){space();if(pos==bytes.size()||bytes[pos]!=ch)return false;++pos;return true;}
 void require(char ch){if(!take(ch))throw std::runtime_error("room JSON framing");}
 std::string text(){
  require('"');std::string out;
  while(pos<bytes.size()){
   unsigned char ch=bytes[pos++];
   if(ch=='"')return out;
   if(ch<32||ch>=127)throw std::runtime_error("room JSON non-ASCII string");
   if(ch=='\\'){
    if(pos==bytes.size())throw std::runtime_error("room JSON escape");
    ch=bytes[pos++];
    if(ch!='"'&&ch!='\\'&&ch!='/')throw std::runtime_error("room JSON unsupported escape");
   }
   out.push_back(char(ch));
   if(out.size()>1024*1024)throw std::runtime_error("room JSON string bound");
  }
  throw std::runtime_error("room JSON unterminated string");
 }
 Json value(unsigned depth){
  if(depth>16||++nodes>100000)throw std::runtime_error("room JSON nesting/node bound");
  space();if(pos==bytes.size())throw std::runtime_error("room JSON truncated");
  Json next;
  if(bytes[pos]=='{'){
   ++pos;next.kind=Json::Object;if(take('}'))return next;
   do{const auto key=text();require(':');if(!next.object.emplace(key,value(depth+1)).second)
    throw std::runtime_error("room JSON duplicate key");}while(take(','));
   require('}');return next;
  }
  if(bytes[pos]=='['){
   ++pos;next.kind=Json::Array;if(take(']'))return next;
   do{next.array.push_back(value(depth+1));}while(take(','));require(']');return next;
  }
  if(bytes[pos]=='"'){next.kind=Json::String;next.string=text();return next;}
  for(const auto& literal:std::array<std::pair<const char*,Json::Kind>,3>{{{"false",Json::False},{"true",Json::True},{"null",Json::Null}}}){
   const std::string token=literal.first;
   if(bytes.compare(pos,token.size(),token)==0){pos+=token.size();next.kind=literal.second;return next;}
  }
  const auto begin=pos;
  if(bytes[pos]=='-')++pos;
  if(pos==bytes.size()||bytes[pos]<'0'||bytes[pos]>'9')throw std::runtime_error("room JSON number");
  if(bytes[pos]=='0')++pos;
  else while(pos<bytes.size()&&bytes[pos]>='0'&&bytes[pos]<='9')++pos;
  if(pos<bytes.size()&&bytes[pos]=='.'){
   ++pos;const auto digits=pos;
   while(pos<bytes.size()&&bytes[pos]>='0'&&bytes[pos]<='9')++pos;
   if(pos==digits)throw std::runtime_error("room JSON fraction");
  }
  if(pos<bytes.size()&&(bytes[pos]=='e'||bytes[pos]=='E')){
   ++pos;if(pos<bytes.size()&&(bytes[pos]=='+'||bytes[pos]=='-'))++pos;
   const auto digits=pos;
   while(pos<bytes.size()&&bytes[pos]>='0'&&bytes[pos]<='9')++pos;
   if(pos==digits)throw std::runtime_error("room JSON exponent");
  }
  std::istringstream numeric(bytes.substr(begin,pos-begin));numeric.imbue(std::locale::classic());
  if(!(numeric>>next.number)||!std::isfinite(next.number))throw std::runtime_error("room JSON finite number");
  next.kind=Json::Number;return next;
 }
public:
 explicit JsonReader(const std::string& data):bytes(data){}
 Json read(){if(bytes.empty()||bytes.size()>1024*1024)throw std::runtime_error("room JSON byte bound");
  auto next=value(0);space();if(pos!=bytes.size())throw std::runtime_error("room JSON trailing bytes");return next;}
};
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
std::string hash(const std::string& s){unsigned char d[32];pc_netplay_sha::sha256(s.data(),s.size(),d);return pc_netplay_sha::hex(d,32);}
const std::string& text(const Json& j){check(j.kind==Json::String,"room expected string");return j.string;}
unsigned integer(const Json& j,unsigned max=65535){check(j.kind==Json::Number&&j.number>=0&&j.number<=max&&std::floor(j.number)==j.number,"room integer bound");return unsigned(j.number);}
const std::vector<Json>& array(const Json& j,std::size_t n){check(j.kind==Json::Array&&j.array.size()==n,"room array size");return j.array;}
std::uint32_t u32(const std::string& raw,std::size_t p){check(p<=raw.size()&&raw.size()-p>=4,"room raw bounds");return
 (std::uint32_t(static_cast<unsigned char>(raw[p]))<<24)|(std::uint32_t(static_cast<unsigned char>(raw[p+1]))<<16)|
 (std::uint32_t(static_cast<unsigned char>(raw[p+2]))<<8)|static_cast<unsigned char>(raw[p+3]);}
float floating(std::uint32_t bits){float f;static_assert(sizeof(f)==sizeof(bits),"binary32 required");std::memcpy(&f,&bits,4);check(std::isfinite(f),"room nonfinite binary32");return f;}
std::uint32_t bits(const Json& j){const auto& s=text(j);check(s.size()==8,"room float bits length");std::uint32_t n=0;
 for(char c:s){check((c>='0'&&c<='9')||(c>='a'&&c<='f'),"room float bits grammar");n=(n<<4)|unsigned(c<='9'?c-'0':c-'a'+10);}floating(n);return n;}
std::string base64(const std::string& s){
 check(!s.empty()&&s.size()%4==0&&s.size()<=1024*1024,"room base64 bound");std::string out;out.reserve(s.size()/4*3);
 auto digit=[](char c)->unsigned{if(c>='A'&&c<='Z')return c-'A';if(c>='a'&&c<='z')return c-'a'+26;if(c>='0'&&c<='9')return c-'0'+52;
  if(c=='+')return 62;
  if(c=='/')return 63;
  throw std::runtime_error("room base64 character");};
 for(std::size_t i=0;i<s.size();i+=4){const unsigned a=digit(s[i]),b=digit(s[i+1]);out.push_back(char((a<<2)|(b>>4)));
  if(s[i+2]=='='){check(i+4==s.size()&&s[i+3]=='='&&(b&15)==0,"room base64 padding");continue;}
  const unsigned c=digit(s[i+2]);out.push_back(char((b<<4)|(c>>2)));
  if(s[i+3]=='='){check(i+4==s.size()&&(c&3)==0,"room base64 padding");continue;}
  out.push_back(char((c<<6)|digit(s[i+3])));
 }return out;
}
void bounds(const Json& j,const std::string& raw,std::size_t at,std::array<std::uint32_t,6>& out){
 for(unsigned axis=0;axis<3;++axis){out[axis]=bits(array(j.at("min"),3)[axis]);out[axis+3]=bits(array(j.at("max"),3)[axis]);
  if(!raw.empty())check(out[axis]==u32(raw,at+axis*4)&&out[axis+3]==u32(raw,at+(axis+3)*4),"room bounds differ from raw");
  check(floating(out[axis])<=floating(out[axis+3]),"room bounds inverted");}
}
SourceRoomUnit unit(const Json& j){
 SourceRoomUnit out;out.name=text(j.at("name"));out.archiveMember=text(j.at("archive_member"));out.archiveSha256=text(j.at("archive_sha256"));
 check(hex64(out.archiveSha256)&&out.archiveMember=="user/Mukki/mapunits/arc/"+out.name+"/texts.szs","room archive binding");
 for(const char* kind:{"grid","mapcode"}){const auto& record=j.at(kind);auto raw=base64(text(record.at("bytes_base64")));
  check(text(record.at("member"))==out.archiveMember+"/"+kind+".bin"&&hash(raw)==text(record.at("sha256")),"room raw member binding");
  (std::string(kind)=="grid"?out.gridBytes:out.mapcodeBytes)=std::move(raw);}
 const auto& raw=out.gridBytes;const unsigned nv=u32(raw,0);check(nv>0&&nv<=65535&&integer(j.at("vertex_count"))==nv,"room vertex count");
 const auto& vs=array(j.at("vertex_f32_bits"),nv);out.vertexBits.reserve(nv);
 std::array<std::uint32_t,6> extrema{};
 for(unsigned i=0;i<nv;++i){std::array<std::uint32_t,3> v{};const auto& row=array(vs[i],3);
  for(unsigned axis=0;axis<3;++axis){v[axis]=u32(raw,4+i*12+axis*4);check(bits(row[axis])==v[axis],"room raw vertex differs");
   if(i==0||floating(v[axis])<floating(extrema[axis]))extrema[axis]=v[axis];
   if(i==0||floating(v[axis])>floating(extrema[axis+3]))extrema[axis+3]=v[axis];}
  out.vertexBits.push_back(v);}
 const std::size_t first=4+nv*12;const unsigned nt=u32(raw,first);check(nt>0&&nt<=65535&&integer(j.at("triangle_count"))==nt,"room triangle count");
 const auto& ts=array(j.at("triangles_abc"),nt);const auto& codes=array(j.at("mapcode_u8"),nt);
 check(out.mapcodeBytes.size()==nt+4&&u32(out.mapcodeBytes,0)==nt,"room raw mapcode count");
 for(unsigned i=0;i<nt;++i){std::array<unsigned,3> triangle{};const auto& abc=array(ts[i],3);
  for(unsigned k=0;k<3;++k){triangle[k]=u32(raw,first+4+i*76+k*4);check(triangle[k]<nv&&integer(abc[k])==triangle[k],"room A/B/C differs");}
  check(triangle[0]!=triangle[1]&&triangle[1]!=triangle[2]&&triangle[0]!=triangle[2],"room repeated triangle vertex");
  for(unsigned k=0;k<16;++k)floating(u32(raw,first+4+i*76+12+k*4));
  const auto code=static_cast<unsigned char>(out.mapcodeBytes[i+4]);check(integer(codes[i],255)==code,"room mapcode differs");
  out.triangles.push_back(triangle);out.mapcodes.push_back(code);}
 const auto tail=first+4+std::size_t(nt)*76;
 bounds(j.at("source_unit_bounds_f32_bits"),raw,tail,out.sourceBounds);
 bounds(j.at("vertex_bounds_f32_bits"),{},0,out.vertexBounds);check(out.vertexBounds==extrema,"room vertex bounds differ");
 const auto& header=j.at("source_divider_header");const auto nx=u32(raw,tail+24),nz=u32(raw,tail+28);
 check(nx>0&&nz>0&&nx<=1000000&&nz<=1000000&&std::uint64_t(nx)*nz<=1000000&&
  integer(header.at("max_x"),1000000)==nx&&integer(header.at("max_z"),1000000)==nz&&
  bits(header.at("scale_x_f32_bits"))==u32(raw,tail+32)&&bits(header.at("scale_z_f32_bits"))==u32(raw,tail+36)&&
  floating(u32(raw,tail+32))>0&&floating(u32(raw,tail+36))>0,"room source divider header");
 check(integer(j.at("source_table_bytes"),1024*1024)==tail&&integer(j.at("retained_acceleration_bytes"),1024*1024)==raw.size()-tail,"room raw tail binding");
 return out;
}
}
bool parseSourceRoomCensus(const SelectedSceneInputs& selected,SourceRoomCensus& out,std::string& error){
 try{
  check(selected.selection.version==2&&selected.selection.floor>=1&&selected.selection.floor<=2,"room selected version/floor");
  const auto& raw=selected.bytes[6];const auto digest=hash(raw);
  // Independent qualified profile pins authenticate the raw member provenance;
  // the self-described archive hashes alone are never a trust root.
  static constexpr const char* pins[]={"d304c81845331f77232d2de25a894ea9a5013711dabbca2ec12adbff9d84edd5","9f0ed34a4faa9a73cc559b8edd3dbf244430d9c2480b715daf04e7ee9c182e2c"};
  check(raw.size()<=1024*1024&&digest==selected.selection.sha256[6]&&digest==pins[selected.selection.floor-1],"room selected/profile digest");
  const auto doc=JsonReader(raw).read();const auto& plan=selected.plan;
  check(plan.cave==selected.selection.cave&&plan.floor==selected.selection.floor&&
   plan.layoutSha256==selected.selection.sha256[0]&&plan.geometrySha256==selected.selection.sha256[1]&&
   plan.routesSha256==selected.selection.sha256[2],"room retained plan differs");
  check(doc.at("schema").numeric(1)&&doc.at("policy").text("authored-emergence-room-census/1")&&
   doc.at("cave").text(selected.selection.cave)&&doc.at("floor").numeric(selected.selection.floor)&&
   doc.at("layout_sha256").text(plan.layoutSha256)&&doc.at("cave_source_sha256").text(plan.sourceSha256)&&
   doc.at("catalog_sha256").text(plan.catalogSha256)&&doc.at("geometry_sha256").text(selected.selection.sha256[1])&&
   doc.at("routes_sha256").text(selected.selection.sha256[2])&&doc.at("start_sha256").text(selected.selection.sha256[3])&&
   doc.at("pool").at("sha256").text(selected.selection.sha256[4]),"room selected bindings");
  for(unsigned i=0;i<6;++i)check(hash(selected.bytes[i])==selected.selection.sha256[i],"room retained selected buffer changed");
  check(doc.at("native_ready").kind==Json::False&&doc.at("hidden_collision_provided").kind==Json::False&&
   doc.at("plat_lifecycle_provided").kind==Json::False,"room input authority boundary");
  SourceRoomCensus next;next.sha256=digest;
  const unsigned floor=selected.selection.floor;const auto& units=array(doc.at("units"),floor==1?2:1);
  for(const auto& record:units)next.units.push_back(unit(record));
  const auto& rooms=array(doc.at("rooms"),floor==1?3:1);
  for(unsigned i=0;i<rooms.size();++i){const auto& r=rooms[i];SourceRoomInstance instance;
   instance.iteration=integer(r.at("iteration"));instance.roomIndex=integer(r.at("room_index"));instance.quarterTurn=integer(r.at("quarter_turn"),3);
   check(instance.iteration==i&&instance.roomIndex==i,"room declared order/index");
   const auto name=text(r.at("unit"));auto found=std::find_if(next.units.begin(),next.units.end(),[&](const SourceRoomUnit& u){return u.name==name;});
   check(found!=next.units.end(),"room declared unit missing");instance.unit=unsigned(found-next.units.begin());
   const auto& translation=array(r.at("translation"),3);
   for(unsigned axis=0;axis<3;++axis){check(translation[axis].kind==Json::Number&&std::fabs(translation[axis].number)<=100000,"room translation bound");instance.translation[axis]=float(translation[axis].number);}
   const auto& source=r.at("source_make_one_room");
   for(const char* label:{"centre_x","centre_z","direction_degrees"})check(source.at(label).kind==Json::Number,"room transform arguments");
   instance.centreX=float(source.at("centre_x").number);instance.centreZ=float(source.at("centre_z").number);instance.directionDegrees=float(source.at("direction_degrees").number);
   check(instance.translation[0]==instance.centreX*170&&instance.translation[1]==0&&instance.translation[2]==instance.centreZ*170&&
    instance.directionDegrees==-90.0f*instance.quarterTurn,"room source transform binding");
   next.rooms.push_back(instance);
  }
  out=std::move(next);error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}
}
bool parseSourceWaterInputs(const SelectedSceneInputs& selected,const SourceRoomCensus& rooms,SourceWaterInputs& out,std::string& error){
 try{
  check(selected.selection.version==2&&selected.selection.floor>=1&&selected.selection.floor<=2,"water selected version/floor");
  const auto& raw=selected.bytes[7];const auto digest=hash(raw);
  static constexpr const char* pins[]={"31f212070f0e182dcfe5cbc45cbc9f0ba01634d07f594f67d2ba33dfa8edd1fe","0e7b2ffbc7747e593c2d6e4f26620256b2f72a4f15462dcff5e5b41b76c7fdb2"};
  check(!raw.empty()&&raw.size()<=65536&&digest==selected.selection.sha256[7]&&digest==pins[selected.selection.floor-1],"water selected/profile digest");
  // Re-admit actual retained room inputs, not a caller-authored zero census.
  SourceRoomCensus actual;check(parseSourceRoomCensus(selected,actual,error)&&actual.sha256==rooms.sha256&&
   actual.units.size()==rooms.units.size()&&actual.rooms.size()==rooms.rooms.size(),"water actual room binding");
  const auto doc=JsonReader(raw).read();const auto& plan=selected.plan;
  check(doc.at("schema").numeric(1)&&doc.at("policy").text("authored-emergence-water-input/1")&&
   doc.at("cave").text(plan.cave)&&doc.at("floor").numeric(plan.floor)&&doc.at("room_census_sha256").text(rooms.sha256)&&
   doc.at("layout_sha256").text(plan.layoutSha256)&&doc.at("cave_source_sha256").text(plan.sourceSha256)&&
   doc.at("catalog_sha256").text(plan.catalogSha256)&&doc.at("geometry_sha256").text(selected.selection.sha256[1])&&
   doc.at("routes_sha256").text(selected.selection.sha256[2])&&doc.at("start_sha256").text(selected.selection.sha256[3])&&
   doc.at("pool").at("sha256").text(selected.selection.sha256[4]),"water selected binding");
  for(const char* flag:{"native_ready","runtime_known_dry","cached_actor_water_provided","runtime_lifecycle_provided"})
   check(doc.at(flag).kind==Json::False,"water runtime authority boundary");
  const auto& roomRecords=array(doc.at("rooms"),actual.rooms.size());
  for(unsigned i=0;i<actual.rooms.size();++i){const auto& expected=actual.rooms[i];const auto& r=roomRecords[i];
   check(r.at("iteration").numeric(expected.iteration)&&r.at("room_index").numeric(expected.roomIndex)&&
    r.at("quarter_turn").numeric(expected.quarterTurn)&&r.at("unit").text(actual.units[expected.unit].name),"water room order binding");
   const auto& translation=array(r.at("translation"),3);
   for(unsigned axis=0;axis<3;++axis)check(translation[axis].numeric(expected.translation[axis]),"water room translation");
   const auto& source=r.at("source_make_one_room");check(source.at("centre_x").numeric(expected.centreX)&&
    source.at("centre_z").numeric(expected.centreZ)&&source.at("direction_degrees").numeric(expected.directionDegrees),"water source transform");
  }
  SourceWaterInputs next;next.sha256=digest;next.roomCensusSha256=actual.sha256;
  const auto& units=array(doc.at("units"),actual.units.size());
  for(unsigned i=0;i<units.size();++i){const auto& j=units[i];const auto& u=actual.units[i];
   check(j.at("name").text(u.name)&&j.at("archive_member").text(u.archiveMember)&&j.at("archive_sha256").text(u.archiveSha256)&&
    j.at("member").text(u.archiveMember+"/waterbox.txt"),"water original archive binding");
   const auto literal=base64(text(j.at("bytes_base64")));
   check(literal.size()==31&&hash(literal)=="d3bae0cb09b8f968c888dc6930e517f0c61d35e3c4541cbd0a9aa688a180615c"&&
    j.at("sha256").text(hash(literal)),"water raw empty-profile binding");
   std::istringstream lines(literal);std::string line,clean;
   while(std::getline(lines,line)){clean+=line.substr(0,line.find('#'));clean+=' ';}
   std::istringstream tokens(clean);std::string a,b,c,d,extra;
   check(bool(tokens>>a>>b>>c>>d)&&a=="0"&&b=="{"&&c=="0"&&d=="}"&&!(tokens>>extra),"water source envelope/count");
   check(j.at("version").numeric(0)&&j.at("count").numeric(0)&&array(j.at("boxes"),0).empty(),"water literal count differs");
   next.units.push_back({u.name,literal,0,0});
  }
  out=std::move(next);error.clear();return true;
 }catch(const std::exception& e){error=e.what();return false;}
}
}
