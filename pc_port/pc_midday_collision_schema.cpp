#include "pc_midday_collision.h"
#include <functional>
namespace pc_midday {
bool collision_schema(const ActorFields&f,const std::string&prefix,std::vector<FieldSchema>&out,std::string&e){
 auto bad=[&](const char*m){if(e.empty())e=m;return false;};const auto p=prefix.empty()?"":prefix+".";
 auto number=[&](const std::string&k,ScalarKind type,u64&v){auto it=f.find(p+k);if(it==f.end()||it->second.category!=FieldCategory::Scalar||it->second.scalar!=type)return false;v=it->second.bits;return true;};
 u64 count=0,cap=0;if(!number("count",ScalarKind::U16,count)||!number("capacity",ScalarKind::U16,cap)||cap>1024||count>cap)return bad("collision allocation bounds");
 std::vector<FieldSchema>s;auto val=[&](const std::string&k,ScalarKind t){s.push_back(FieldSchema::value((p+k).c_str(),t));};
 auto identity=f.find(p+"identity");if(identity==f.end())return bad("collision identity missing");
 s.push_back(FieldSchema::ref((p+"identity").c_str(),RefKind::CollInfo,false,"CollInfo",ReferenceOwnership::ActorSubobject));
 val("count",ScalarKind::U16);val("capacity",ScalarKind::U16);val("defaultStorage",ScalarKind::Bool);s.push_back(FieldSchema::ref((p+"shape").c_str(),RefKind::Shape,true,"Shape",ReferenceOwnership::Content));
 std::vector<std::vector<int>> edges(count);
 for(u64 i=0;i<count;++i){auto q="part."+std::to_string(i)+".";
 val(q+"id",ScalarKind::U32);val(q+"radius",ScalarKind::F32);for(auto x:{"x","y","z"})val(q+"center."+x,ScalarKind::F32);val(q+"update",ScalarKind::Bool);val(q+"stick",ScalarKind::Bool);val(q+"type",ScalarKind::U8);
 u64 type=0;if(!number(q+"type",ScalarKind::U8,type)||type>6)return bad("collision part type");
 for(auto k:{"next","child"}){val(q+k,ScalarKind::S16);u64 bits=0;if(!number(q+k,ScalarKind::S16,bits))return bad("collision edge missing");int index=static_cast<s16>(bits);if(index< -1||index>=int(count))return bad("collision edge bounds");if(index>=0)edges[i].push_back(index);}
 for(int r=0;r<4;++r)for(int c=0;c<4;++c)val(q+"matrix."+std::to_string(r)+"."+std::to_string(c),ScalarKind::F32);
 s.push_back(FieldSchema::ref((p+q+"descriptor").c_str(),RefKind::ObjCollInfo,false,"ObjCollInfo",ReferenceOwnership::Content));
 auto parent=f.find(p+q+"parent");if(parent==f.end()||parent->second.target.owner!=identity->second.target.owner||parent->second.target.resource!=identity->second.target.resource||parent->second.target.slot!=identity->second.target.slot)return bad("collision part parent is not canonical collider");
 s.push_back(FieldSchema::ref((p+q+"parent").c_str(),RefKind::CollInfo,false,"CollInfo",ReferenceOwnership::ActorSubobject));
 s.push_back(FieldSchema::ref((p+q+"updater").c_str(),RefKind::CollPartUpdater,true,"CollPartUpdater",ReferenceOwnership::AnyLive));
 }
 std::vector<int>mark(count);std::function<bool(int)>visit=[&](int i){if(mark[i]==1)return false;if(mark[i]==2)return true;mark[i]=1;for(int j:edges[i])if(!visit(j))return false;mark[i]=2;return true;};for(u64 i=0;i<count;++i)if(!visit(i))return bad("cyclic collision hierarchy");
 out.insert(out.end(),s.begin(),s.end());return true;
}
}
