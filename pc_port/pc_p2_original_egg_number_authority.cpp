#include "pc_p2_original_egg_number_authority.h"
#include "teki.h"
#include <cstdio>
#include <cmath>
#include <cstdlib>
namespace p2originalresource {
EggNumberAuthority::EggNumberAuthority(p2original::egg::Native& native,const NumberRootAuthority& roots)
 :mNative(native),mJournal(native.contents(),roots,pc_p2_original_number_resources){}
EggNumberAuthority::~EggNumberAuthority(){
 if(mJournal.active()||!pc_p2_original_number_authority_idle(this)){
  std::fprintf(stderr,"numeric producer authority destroyed during scope or with live borrowers\n");std::abort();
 }
}
bool EggNumberAuthority::beginContents(const p2original::egg::Host& host,const SourceIdentity& root,unsigned type,
 const P2EggConfig& config,const P2EggVec3& origin,const EggContents& journal,std::string& e){
 if(&journal!=&mNative.contents()||!host.creature||mNative.provider().lookup(host.creature)!=&host||
    !mNative.owns(host.creature)||host.staged||!std::isfinite(host.health)||host.health>0||host.contentsGenerated||host.killRequested||type!=(host.dependent?16u:37u)){
  e="numeric scope is not actual owned Egg destruction";return false;
 }
 const auto actualPosition=host.creature->getPosition();
 if(!exactNumberFloat(origin.x,actualPosition.x)||!exactNumberFloat(origin.y,actualPosition.y)||!exactNumberFloat(origin.z,actualPosition.z)){e="numeric scope origin differs from actual native body";return false;}
 SourceIdentity actual;if(!mNative.identity(static_cast<BTeki*>(host.creature),actual,e))return false;
 if(!(actual==root)){e="numeric scope differs from actual native source identity";return false;}
 return mJournal.begin(root,type,config,origin,e);
}
void EggNumberAuthority::endContents()noexcept{mJournal.end();}
bool EggNumberAuthority::pending(const ChildOutcome& child,unsigned& type,ContentsRecord& out,std::string& e){return mJournal.pending(child,type,out,e);}
bool EggNumberAuthority::completed(const ChildIdentity& id,unsigned& type,ContentsRecord& out,std::string& e){return mJournal.completed(id,type,out,e);}
bool EggNumberAuthority::consumeAccepted(const ChildIdentity& id,std::string& e){return mJournal.consumeAccepted(id,e);}
}
