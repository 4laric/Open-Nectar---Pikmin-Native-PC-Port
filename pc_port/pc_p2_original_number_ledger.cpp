#include "pc_p2_original_number_ledger.h"
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>
namespace p2originalnumber {
namespace {
using namespace p2originalresource;
bool fail(std::string& e,const char* text){e=text;return false;}
bool hex(const std::string& s){if(s.size()!=64)return false;for(char c:s)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;return true;}
bool finite(const P2EggVec3& v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
bool scalarSame(float a,float b){static_assert(sizeof(float)==sizeof(std::uint32_t),"32 bit producer float payload required");std::uint32_t x=0,y=0;std::memcpy(&x,&a,sizeof(x));std::memcpy(&y,&b,sizeof(y));return x==y;}
bool vectorSame(const P2EggVec3& a,const P2EggVec3& b){return scalarSame(a.x,b.x)&&scalarSame(a.y,b.y)&&scalarSame(a.z,b.z);}
bool payloadSame(const ChildOutcome& a,const ChildOutcome& b){return a.identity==b.identity&&a.kind==b.kind&&vectorSame(a.position,b.position)&&vectorSame(a.velocity,b.velocity)&&a.pelletColor==b.pelletColor&&a.mititeCount==b.mititeCount&&scalarSame(a.facing,b.facing);}
}
Ledger::Ledger(std::string catalog):mCatalog(std::move(catalog)){}
bool Ledger::validBirth(const Receipt& r)const{
 using namespace p2originalresource;const auto& c=r.birthPayload;
 return hex(mCatalog)&&c.identity.source.fingerprint==mCatalog&&validChildIdentity(c.identity)&&c.identity.slot==0&&c.identity.ancestry.empty()&&
 (r.rootSourceType==16||r.rootSourceType==37)&&(c.kind==ChildKind::PelletOne||c.kind==ChildKind::PelletFive)&&
 c.pelletColor>=0&&c.pelletColor<=2&&!c.mititeCount&&finite(c.position)&&finite(c.velocity)&&std::isfinite(c.facing)&&c.attempted&&!c.born&&!c.consumed&&!r.consumed;
}
bool Ledger::preflightBirth(const p2originalresource::ChildOutcome& c,unsigned root,BirthPlan& out,std::string& e)const{
 Receipt r{c,root,false};if(!validBirth(r)||mReceipts.size()>=retainedLimit||mReceipts.count(c.identity)||mNextHandle==std::numeric_limits<std::uint64_t>::max())return fail(e,"invalid/replayed numeric pending birth or retained capacity exhausted");
 BirthPlan candidate{std::move(r)};out=std::move(candidate);e.clear();return true;
}
bool Ledger::bindBirth(const BirthPlan& plan,const void* pellet,std::uint64_t& out,std::string& e){
 if(!pellet||mBindings.count(pellet)||mNextHandle==std::numeric_limits<std::uint64_t>::max())return fail(e,"null/reused numeric Pellet binding or handle capacity exhausted");
 BirthPlan checked;if(plan.receipt.consumed||!preflightBirth(plan.receipt.birthPayload,plan.receipt.rootSourceType,checked,e))return false;
 const auto id=checked.receipt.birthPayload.identity;
 auto inserted=mReceipts.emplace(id,std::move(checked.receipt));
 try{mBindings.emplace(pellet,Binding{id,mNextHandle});}catch(...){mReceipts.erase(inserted.first);throw;}
 out=mNextHandle++;e.clear();return true;
}
bool Ledger::owns(const void* p)const noexcept{return mBindings.find(p)!=mBindings.end();}
bool Ledger::handle(const void* p,std::uint64_t& out)const noexcept{auto b=mBindings.find(p);if(b==mBindings.end())return false;out=b->second.handle;return true;}
bool Ledger::journalMatches(const Receipt& r,const p2originalresource::ContentsRecord& j,unsigned root,bool consumed)const{
 using namespace p2originalresource;
 if(root!=r.rootSourceType||!j.complete||!(j.source==r.birthPayload.identity.source)||j.children.size()!=1)return false;
 const auto& c=j.children.front();
 const auto type=r.birthPayload.kind==ChildKind::PelletOne?P2EggDropType::OnePellets:P2EggDropType::FivePellets;
 return j.type==type&&payloadSame(c,r.birthPayload)&&c.attempted&&c.born&&c.consumed==consumed;
}
QueryResult Ledger::query(const void* p,std::uint64_t h,const p2originalresource::ContentsRecord& j,unsigned root,Receipt& out,std::string& e)const{
 auto b=mBindings.find(p);if(b==mBindings.end()){e.clear();return QueryResult::Missing;}
 auto r=mReceipts.find(b->second.identity);
 if(!h||b->second.handle!=h||r==mReceipts.end()||!journalMatches(r->second,j,root,r->second.consumed)){e="labelled numeric body lifetime/journal/payload unavailable";return QueryResult::Unavailable;}
 Receipt candidate=r->second;out=std::move(candidate);e.clear();return QueryResult::Present;
}
bool Ledger::prepareConsume(const void* p,std::uint64_t h,const p2originalresource::ContentsRecord& j,unsigned root,ConsumePlan& out,std::string& e)const{
 Receipt r;if(query(p,h,j,root,r,e)!=QueryResult::Present)return false;
 if(r.consumed)return fail(e,"numeric retained receipt already consumed");
 ConsumePlan candidate{p,h,std::move(r)};out=std::move(candidate);e.clear();return true;
}
bool Ledger::commitConsumed(const ConsumePlan& plan,const p2originalresource::ContentsRecord& j,unsigned root,std::string& e){
 auto b=mBindings.find(plan.pellet);
 if(b==mBindings.end()||!plan.handle||b->second.handle!=plan.handle)return fail(e,"numeric consume plan lost its live Pellet binding");
 auto r=mReceipts.find(b->second.identity);
 if(r==mReceipts.end()||r->second.consumed||!validBirth(plan.receipt)||r->second.rootSourceType!=plan.receipt.rootSourceType||
    !payloadSame(r->second.birthPayload,plan.receipt.birthPayload)||!journalMatches(r->second,j,root,true))return fail(e,"numeric consume plan/journal mismatch or replay");
 r->second.consumed=true;e.clear();return true;
}
bool Ledger::retire(const void* p,std::uint64_t h,std::string& e){
 auto b=mBindings.find(p);if(b==mBindings.end()){e.clear();return true;}
 if(!h||b->second.handle!=h)return fail(e,"stale numeric Pellet retirement");
 mBindings.erase(b);e.clear();return true;
}
bool Ledger::unload(std::string& e)const{if(!mBindings.empty())return fail(e,"numeric receipts still own live Pellet bodies");e.clear();return true;}
bool Ledger::freshSession(const std::string& catalog,std::string& e){
 if(!hex(catalog))return fail(e,"numeric fresh-session catalog fingerprint invalid");
 if(!unload(e))return false;
 std::string candidate=catalog;mCatalog.swap(candidate);mReceipts.clear();e.clear();return true;
}
Report Ledger::report()const{Report r;r.catalogFingerprint=mCatalog;r.liveBodies=mBindings.size();for(const auto& receipt:mReceipts)r.receipts.push_back(receipt.second);return r;}
}
