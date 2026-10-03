#include "pc_p2_original_egg_number_journal.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
namespace p2originalresource {
bool exactNumberFloat(float a,float b)noexcept{
 static_assert(sizeof(float)==sizeof(std::uint32_t),"source float requires32 bits");
 std::uint32_t left=0,right=0;std::memcpy(&left,&a,sizeof(left));std::memcpy(&right,&b,sizeof(right));return left==right;
}
namespace {
bool fail(std::string& e,const char* message){e=message;return false;}
bool finite(const P2EggVec3& v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
bool numeric(const ChildOutcome& c){return validChildIdentity(c.identity)&&c.identity.ancestry.empty()&&c.identity.slot==0&&
 (c.kind==ChildKind::PelletOne||c.kind==ChildKind::PelletFive)&&c.pelletColor>=0&&c.pelletColor<3&&
 finite(c.position)&&finite(c.velocity)&&exactNumberFloat(c.facing,0.0f)&&c.mititeCount==0&&c.attempted;}
bool same(const ChildOutcome& a,const ChildOutcome& b){return a.identity==b.identity&&a.kind==b.kind&&
 exactNumberFloat(a.position.x,b.position.x)&&exactNumberFloat(a.position.y,b.position.y)&&exactNumberFloat(a.position.z,b.position.z)&&
 exactNumberFloat(a.velocity.x,b.velocity.x)&&exactNumberFloat(a.velocity.y,b.velocity.y)&&exactNumberFloat(a.velocity.z,b.velocity.z)&&
 a.pelletColor==b.pelletColor&&a.mititeCount==b.mititeCount&&exactNumberFloat(a.facing,b.facing)&&
 a.attempted==b.attempted&&a.born==b.born&&a.consumed==b.consumed;}
bool matchingType(const ContentsRecord& r,const ChildOutcome& c){return
 (r.type==P2EggDropType::OnePellets&&c.kind==ChildKind::PelletOne)||
 (r.type==P2EggDropType::FivePellets&&c.kind==ChildKind::PelletFive);}
}
NumberJournalAuthority::NumberJournalAuthority(EggContents& journal,const NumberRootAuthority& roots,Admission admission)
 :mJournal(journal),mRoots(roots),mAdmission(std::move(admission)){}
bool NumberJournalAuthority::begin(const SourceIdentity& root,unsigned type,const P2EggConfig& config,const P2EggVec3& origin,std::string& e){
 if(mScope)return fail(e,"numeric producer scope is already active");
 if(!validChildIdentity({root,0})||!finite(origin)||(type!=16&&type!=37)||mJournal.find(root))return fail(e,"numeric producer scope is not fresh/authenticated");
 unsigned actual=0;if(!mRoots.resolve(root,actual,e))return false;
 if(actual!=type)return fail(e,"numeric producer root type differs from selected authority");
 ContentsRequirements required;if(!requirements(config,required,e))return false;
 if(!mAdmission)return fail(e,"numeric producer admission is not installed");
 if(!mAdmission(required,e))return false; // resources/retained limits BEFORE RNG
 mScope=Scope{root,type,required};e.clear();return true;
}
void NumberJournalAuthority::end()noexcept{mScope.reset();}
bool NumberJournalAuthority::pending(const ChildOutcome& child,unsigned& type,ContentsRecord& out,std::string& e)const{
 if(!mScope||!(child.identity.source==mScope->root)||!numeric(child)||child.born||child.consumed)return fail(e,"numeric pending callback is outside actual producer scope");
 if((child.kind==ChildKind::PelletOne&&!mScope->required.pelletOne)||(child.kind==ChildKind::PelletFive&&!mScope->required.pelletFive))return fail(e,"numeric pending kind was not admitted before producer RNG");
 unsigned actual=0;if(!mRoots.resolve(child.identity.source,actual,e))return false;
 if(actual!=mScope->type)return fail(e,"numeric pending root authority changed");
 const auto* row=mJournal.find(child.identity.source);
 if(!row||row->complete||!(row->source==child.identity.source)||row->children.size()!=1||!matchingType(*row,child)||!same(row->children.front(),child))return fail(e,"numeric callback differs from actual pending journal");
 ContentsRecord next=*row;out=std::move(next);type=actual;e.clear();return true;
}
bool NumberJournalAuthority::completed(const ChildIdentity& id,unsigned& type,ContentsRecord& out,std::string& e)const{
 if(!validChildIdentity(id)||!id.ancestry.empty()||id.slot!=0)return fail(e,"numeric completed identity is not a direct Egg child");
 unsigned actual=0;if(!mRoots.resolve(id.source,actual,e))return false;
 if(actual!=16&&actual!=37)return fail(e,"numeric completed root source is unsupported");
 const auto* row=mJournal.find(id.source);
 if(!row||!row->complete||!(row->source==id.source)||row->children.size()!=1)return fail(e,"numeric completed journal is missing/incomplete");
 const auto& child=row->children.front();
 if(!(child.identity==id)||!numeric(child)||!child.born||!matchingType(*row,child))return fail(e,"numeric completed journal has no matching born child");
 ContentsRecord next=*row;out=std::move(next);type=actual;e.clear();return true;
}
bool NumberJournalAuthority::consumeAccepted(const ChildIdentity& id,std::string& e){
 unsigned type=0;ContentsRecord row;if(!completed(id,type,row,e))return false;
 if(!mJournal.consume(id,e))return false;
 const auto* actual=mJournal.find(id.source);
 if(!actual||!actual->complete||actual->children.size()!=1||!(actual->children.front().identity==id)||!actual->children.front().born||!actual->children.front().consumed){
  std::fprintf(stderr,"numeric accepted producer journal changed after consumption\n");std::abort();
 }
 e.clear();return true;
}
}
