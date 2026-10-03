#include "pc_p2_retail_start.h"
#include <map>
#include <stdexcept>

namespace p2retail { namespace {
struct Json {
 enum Kind {Object,Array,String,Number,False,True,Null} kind=Null;
 std::map<std::string,Json> object;
 std::vector<Json> array;
 std::string string;
 double number=0;
 const Json& at(const char* key)const{
  if(kind!=Object)throw std::runtime_error("start JSON expected object");
  const auto found=object.find(key);
  if(found==object.end())throw std::runtime_error("start JSON missing field");
  return found->second;
 }
 bool text(const std::string& value)const{return kind==String&&string==value;}
 bool numeric(double value)const{return kind==Number&&number==value;}
};
class JsonReader {
 const std::string& bytes;std::size_t pos=0;unsigned nodes=0;
 void space(){while(pos<bytes.size()&&(bytes[pos]==' '||bytes[pos]=='\n'||bytes[pos]=='\r'||bytes[pos]=='\t'))++pos;}
 bool take(char ch){space();if(pos==bytes.size()||bytes[pos]!=ch)return false;++pos;return true;}
 void require(char ch){if(!take(ch))throw std::runtime_error("start JSON framing");}
 std::string text(){
  require('"');std::string out;
  while(pos<bytes.size()){
   unsigned char ch=bytes[pos++];
   if(ch=='"')return out;
   if(ch<32||ch>=127)throw std::runtime_error("start JSON non-ASCII string");
   if(ch=='\\'){
    if(pos==bytes.size())throw std::runtime_error("start JSON escape");
    ch=bytes[pos++];
    if(ch!='"'&&ch!='\\'&&ch!='/')throw std::runtime_error("start JSON unsupported escape");
   }
   out.push_back(char(ch));
   if(out.size()>4096)throw std::runtime_error("start JSON string bound");
  }
  throw std::runtime_error("start JSON unterminated string");
 }
 Json value(unsigned depth){
  if(depth>16||++nodes>2048)throw std::runtime_error("start JSON nesting/node bound");
  space();if(pos==bytes.size())throw std::runtime_error("start JSON truncated");
  Json next;
  if(bytes[pos]=='{'){
   ++pos;next.kind=Json::Object;if(take('}'))return next;
   do{const auto key=text();require(':');if(!next.object.emplace(key,value(depth+1)).second)
    throw std::runtime_error("start JSON duplicate key");}while(take(','));
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
  if(pos==bytes.size()||bytes[pos]<'0'||bytes[pos]>'9')throw std::runtime_error("start JSON number");
  if(bytes[pos]=='0')++pos;
  else while(pos<bytes.size()&&bytes[pos]>='0'&&bytes[pos]<='9')++pos;
  if(pos<bytes.size()&&bytes[pos]=='.'){
   ++pos;const auto digits=pos;
   while(pos<bytes.size()&&bytes[pos]>='0'&&bytes[pos]<='9')++pos;
   if(pos==digits)throw std::runtime_error("start JSON fraction");
  }
  if(pos<bytes.size()&&(bytes[pos]=='e'||bytes[pos]=='E')){
   ++pos;if(pos<bytes.size()&&(bytes[pos]=='+'||bytes[pos]=='-'))++pos;
   const auto digits=pos;
   while(pos<bytes.size()&&bytes[pos]>='0'&&bytes[pos]<='9')++pos;
   if(pos==digits)throw std::runtime_error("start JSON exponent");
  }
  std::istringstream numeric(bytes.substr(begin,pos-begin));numeric.imbue(std::locale::classic());
  if(!(numeric>>next.number)||!std::isfinite(next.number))throw std::runtime_error("start JSON finite number");
  next.kind=Json::Number;return next;
 }
public:
 explicit JsonReader(const std::string& data):bytes(data){}
 Json read(){if(bytes.empty()||bytes.size()>65536)throw std::runtime_error("start JSON byte bound");
  auto next=value(0);space();if(pos!=bytes.size())throw std::runtime_error("start JSON trailing bytes");return next;}
};
void require(bool condition,const char* error){if(!condition)throw std::runtime_error(error);}
std::array<double,3> vector(const Json& json){
 require(json.kind==Json::Array&&json.array.size()==3,"start vector framing");
 std::array<double,3> next{};
 for(unsigned i=0;i<3;++i){require(json.array[i].kind==Json::Number&&std::fabs(json.array[i].number)<=100000,"start vector coordinate");next[i]=json.array[i].number;}
 return next;
}
std::string digest(const std::string& bytes){unsigned char value[32];pc_netplay_sha::sha256(bytes.data(),bytes.size(),value);return pc_netplay_sha::hex(value,32);}
class SourceTokens {
 std::istringstream input;
 static std::string strip(const std::string& raw){
  std::string text;bool comment=false;
  for(char ch:raw){if(ch=='\n'){comment=false;text+='\n';}else if(ch=='#')comment=true;
   else if(!comment){require(ch!=0,"source text NUL");text+=ch;}}
  return text;
 }
public:
 explicit SourceTokens(const std::string& raw):input(strip(raw)){require(raw.size()<=1024*1024,"source text size");input.imbue(std::locale::classic());}
 std::string token(){std::string value;require(bool(input>>value),"source text truncated");require(value.size()<=128,"source token size");return value;}
 void token(const char* expected){require(token()==expected,"source text framing");}
 double number(){const auto value=token();std::istringstream parser(value);parser.imbue(std::locale::classic());double result;std::string extra;
  require(bool(parser>>result)&&!(parser>>extra)&&std::isfinite(result),"source finite number");return result;}
 unsigned integer(unsigned maximum){const auto value=number();require(value>=0&&value<=maximum&&std::floor(value)==value,"source integer bound");return unsigned(value);}
 void end(){std::string extra;require(!(input>>extra),"source text trailing bytes");}
};
struct Door {unsigned id,direction,offset,waypoint;struct Link{double distance;unsigned door,teki;};std::vector<Link> links;};
struct Unit {std::string name;unsigned x,z,kind,flag0,flag1;std::vector<Door> doors;};
std::map<std::string,Unit> pool(const std::string& raw){
 SourceTokens tokens(raw);const auto count=tokens.integer(256);require(count>0,"empty source pool");std::map<std::string,Unit> units;
 for(unsigned i=0;i<count;++i){
  tokens.token("{");require(tokens.integer(1)==1,"source pool version");Unit unit;unit.name=tokens.token();
  for(char ch:unit.name)require((ch>='a'&&ch<='z')||(ch>='0'&&ch<='9')||ch=='_',"source pool unit name");
  unit.x=tokens.integer(128);unit.z=tokens.integer(128);require(unit.x&&unit.z,"source pool dimensions");
  unit.kind=tokens.integer(2);unit.flag0=tokens.integer(1);unit.flag1=tokens.integer(1);
  const auto doors=tokens.integer(64);std::set<unsigned> ids;
  for(unsigned d=0;d<doors;++d){
   Door door;door.id=tokens.integer(doors?doors-1:0);require(ids.insert(door.id).second,"duplicate source door");
   door.direction=tokens.integer(3);door.offset=tokens.integer(127);door.waypoint=tokens.integer(10000);
   require(door.offset<((door.direction%2)?unit.z:unit.x),"source door offset");
   const auto links=tokens.integer(64);std::set<unsigned> linked;
   for(unsigned l=0;l<links;++l){Door::Link link;link.distance=tokens.number();require(link.distance>=0&&link.distance<=100000,"source door distance");
    link.door=tokens.integer(doors?doors-1:0);link.teki=tokens.integer(1);
    require(link.door!=door.id&&linked.insert(link.door).second,"source door link identity");door.links.push_back(link);}
   unit.doors.push_back(std::move(door));
  }
  tokens.token("}");require(units.emplace(unit.name,std::move(unit)).second,"duplicate source pool unit");
 }
 tokens.end();return units;
}
struct Slot {unsigned type,minimum,maximum;std::array<double,3> position;double angle,radius;};
std::vector<Slot> layout(const std::string& raw){
 SourceTokens tokens(raw);const auto count=tokens.integer(10000);require(count>0,"empty source layout");std::vector<Slot> slots;
 for(unsigned i=0;i<count;++i){tokens.token("{");Slot slot;slot.type=tokens.integer(8);
  for(auto& value:slot.position){value=tokens.number();require(std::fabs(value)<=100000,"source slot position");}
  slot.angle=tokens.number();slot.radius=tokens.number();require(slot.angle>=0&&slot.angle<360&&slot.radius>=0&&slot.radius<=100000,"source slot angle/radius");
  slot.minimum=tokens.integer(10000);slot.maximum=tokens.integer(10000);require(slot.minimum<=slot.maximum,"source slot population");
  tokens.token("}");slots.push_back(slot);
 }
 tokens.end();return slots;
}
void unitDefinition(const Json& json,const Unit& unit){
 require(json.at("name").text(unit.name)&&json.at("kind").numeric(unit.kind),"start/source unit kind");
 const auto& cells=json.at("cells");const auto& flags=json.at("flags");
 require(cells.kind==Json::Array&&cells.array.size()==2&&cells.array[0].numeric(unit.x)&&cells.array[1].numeric(unit.z),"start/source unit cells");
 require(flags.kind==Json::Array&&flags.array.size()==2&&flags.array[0].numeric(unit.flag0)&&flags.array[1].numeric(unit.flag1),"start/source unit flags");
 const auto& doors=json.at("doors");require(doors.kind==Json::Array&&doors.array.size()==unit.doors.size(),"start/source unit doors");
 for(unsigned i=0;i<unit.doors.size();++i){const auto& actual=unit.doors[i];const auto& copied=doors.array[i];
  require(copied.at("id").numeric(actual.id)&&copied.at("direction").numeric(actual.direction)&&copied.at("offset").numeric(actual.offset)&&copied.at("waypoint").numeric(actual.waypoint),"start/source door identity");
  const auto& links=copied.at("links");require(links.kind==Json::Array&&links.array.size()==actual.links.size(),"start/source door links");
  for(unsigned l=0;l<actual.links.size();++l){const auto& link=actual.links[l];const auto& record=links.array[l];
   require(record.at("distance").numeric(link.distance)&&record.at("door").numeric(link.door)&&record.at("enemy_flag").numeric(link.teki),"start/source door link values");}
 }
}
} // namespace

bool parseSourceStart(const SelectedSceneInputs& inputs,SourceStart& out,std::string& error){
 try{
  require(inputs.selection.cave=="tutorial_1"&&(inputs.selection.floor==1||inputs.selection.floor==2),"unsupported source start");
  const auto root=JsonReader(inputs.bytes[3]).read();const auto units=pool(inputs.bytes[4]);const auto slots=layout(inputs.bytes[5]);
  require(root.at("schema").numeric(1)&&root.at("cave").text(inputs.plan.cave)&&root.at("floor").numeric(inputs.plan.floor)&&
   root.at("catalog_sha256").text(inputs.plan.catalogSha256)&&root.at("cave_source_sha256").text(inputs.plan.sourceSha256)&&
   root.at("layout_sha256").text(inputs.plan.layoutSha256),"start/selected plan provenance");
  require(root.at("native_ready").kind==Json::False&&root.at("gameplay_accepted").kind==Json::False,"start cannot grant native/gameplay authority");
  require(root.at("rule").text("retail-story-no-demo-navi-init/1")&&
   root.at("rule_sha256").text("e2fdaccdb0e411270d85370779a068dd81eb4136c2e6c1273585a0ee8711cddb"),"source start rule pin");
  static constexpr const char* ruleFiles[]={"include/Game/Cave/Info.h","include/Game/mapParts.h",
   "src/plugProjectKandoU/baseGameSection.cpp","src/plugProjectKandoU/gameMapParts.cpp","src/plugProjectKandoU/mapMgr.cpp",
   "src/plugProjectNishimuraU/MapCreator.cpp","src/plugProjectNishimuraU/MapNode.cpp",
   "src/plugProjectNishimuraU/RandMapMgr.cpp","src/plugProjectNishimuraU/RandMapScore.cpp"};
  static constexpr const char* ruleHashes[]={"8d7b76ef45810f1093dfb8ee19e89d8b6dc123ae3885609afa9f01476033aa4f",
   "159445505a92e9ab604cbfdf888ed7ba36b57f64438854a8a1af9260bae3d3bb",
   "ffc3063e1d088a2806cf391c66b30812af83414cd925e4a303e22244b26fa21f",
   "267df97dceba6034a657d728e38892c273790059b29e25ee60efac26d333fd49",
   "c59a737276448722efb47702d947944e1ff5767176a058668acd06fe99f127fc",
   "e33c58655016b23ecc617132df7653ef98483216163f19c2759c41c894327c0a",
   "0fd0279e351dc4bd99bedd52aa227a007ea23b9cd01a1d23ad04cb06ba253ca3",
   "ec28ff1b934e7e1a38f20e2f7820d0033b5109108b4c0ec064eb759fc47a183e",
   "9f721aa295878db4066fb1fb7a08f76feaf2c9c42658c3d14241f2138e0697de"};
  const auto& sourceRules=root.at("rule_sources");require(sourceRules.kind==Json::Object&&sourceRules.object.size()==9,"source start rule files");
  for(unsigned i=0;i<9;++i)require(sourceRules.at(ruleFiles[i]).text(ruleHashes[i]),"source start authentic rule bytes");
  const auto& prerequisites=root.at("prerequisites");
  require(prerequisites.kind==Json::Array&&prerequisites.array.size()==3&&
   prerequisites.array[0].text("story non-versus")&&prerequisites.array[1].text("actual RoomMapMgr with null demo matrix")&&
   prerequisites.array[2].text("actual zero-existing-Navi initialization branch"),"source start initialization prerequisites");
  require(root.at("ground_query").text("actual MapMgr getMinY at map_start, before horizontal offsets")&&
   root.at("facing").text("roundAng(actual MapMgr getMapRotation), not Pod yaw"),"source start grounding/facing rule");
  const auto& captains=root.at("captains");require(captains.kind==Json::Array&&captains.array.size()==2,"source start two captains");
  for(unsigned i=0;i<2;++i){const auto& captain=captains.array[i];require(captain.at("id").numeric(i)&&
   captain.at("ground_y_offset").numeric(SourceStart::groundOffset)&&captain.at("x_offset").numeric(SourceStart::captainX[i])&&
   captain.at("z_offset").numeric(SourceStart::captainZ[i]),"source captain initialization offsets");}
  const auto& poolProof=root.at("pool");require(poolProof.at("sha256").text(digest(inputs.bytes[4])),"start/source raw pool hash");
  const auto& slotProof=root.at("slot");require(slotProof.at("sha256").text(digest(inputs.bytes[5]))&&
   slotProof.at("unit").numeric(0)&&slotProof.at("index").numeric(0)&&slotProof.at("spawn_type").numeric(7),"start/source raw slot identity");
  const auto& transform=slotProof.at("transform");require(transform.kind==Json::Array&&transform.array.size()==3&&
   transform.array[0].kind==Json::String&&transform.array[1].numeric(0)&&vector(transform.array[2])==std::array<double,3>{0,0,0},"source start authored unit transform");
  SourceStart next;next.unit=transform.array[0].string;const auto found=units.find(next.unit);
  require(found!=units.end(),"source start unit absent from actual pool");unitDefinition(slotProof.at("unit_definition"),found->second);
  const auto& actual=slots[0];require(actual.type==7&&actual.minimum==1&&actual.maximum==1&&actual.radius==0,"source start actual BaseGen slot");
  const auto& local=slotProof.at("local");require(local.at("type").numeric(actual.type)&&local.at("min").numeric(actual.minimum)&&
   local.at("max").numeric(actual.maximum)&&local.at("angle").numeric(actual.angle)&&local.at("radius").numeric(actual.radius)&&
   vector(local.at("position"))==actual.position,"start/source raw layout semantics");
  next.slotPosition=vector(slotProof.at("global_position"));require(next.slotPosition==actual.position&&
   slotProof.at("global_yaw_degrees").numeric(actual.angle),"source start global transform");
  require(inputs.plan.pod.unit==0&&inputs.plan.pod.slot==0&&inputs.plan.pod.x==float(actual.position[0])&&
   inputs.plan.pod.y==float(actual.position[1])&&inputs.plan.pod.z==float(actual.position[2])&&inputs.plan.pod.yawDegrees==float(actual.angle),"source FIXNODE_Pod/selected plan mapping");
  next.mapStart=vector(root.at("map_start"));auto expected=next.slotPosition;expected[1]+=50;
  require(next.mapStart==expected,"source MapMgr start height rule");
  const unsigned floor=inputs.selection.floor;
  static constexpr const char* poolHashes[]={"502ea171d1c81c72f47982172684f3915d87c15c2e983c66f6de56589f9a8f96",
   "f5f44677b8489cabbef40bd532f465a37a57e08c0ae76db22f734d729925d521"};
  static constexpr const char* layoutHashes[]={"0fbc14f707751aa254d018f5f1788efa1d001db2133a4bce90555a0fec6e37d0",
   "7621d9df2e41379fcc9af6f80f14196664b440dc8bbe0e77c3a68123cba86a26"};
  require(digest(inputs.bytes[4])==poolHashes[floor-1]&&digest(inputs.bytes[5])==layoutHashes[floor-1],"source start authentic retail pool/layout bytes");
  require(next.unit==(floor==1?"room_north_tutorial_1_snow":"room_purple14x14_snow")&&
   poolProof.at("member").text(floor==1?"user/Mukki/mapunits/units/1_units_north_tutorial_snow.txt":"user/Mukki/mapunits/units/1_units_purple_snow.txt")&&
   slotProof.at("member").text("user/Mukki/mapunits/arc/"+next.unit+"/texts.szs/layout.txt"),"source start retail member mapping");
  out=std::move(next);error.clear();return true;
 }catch(const std::runtime_error& failure){error=failure.what();return false;}
}
} // namespace p2retail
