#include "pc_midday_strong_storage.h"
#include "SmartPtr.h"
#include <cstdlib>
#include <iostream>
using namespace pc_midday;
static void check(bool b,const char*m){if(!b){std::cerr<<m<<"\n";std::exit(1);}}
struct Target {int callbacks=0;void addCnt(){++callbacks;}void subCnt(){++callbacks;}};
struct Owner {SmartPtr<Target> member[2];};
struct Visitor:StrongStorageVisitor {
 Owner& owner;int calls=0;bool reject=false;
 explicit Visitor(Owner&o):owner(o){}
 bool visit(const char*key,const StrongStorageSlot&s,std::string&e)override{
  check(std::string(key)=="outer.inner.slot","prefix lost");check(s.owner==&owner&&std::string(s.ownerType)=="Owner"&&std::string(s.member)=="member","owner metadata lost");
  check(s.index==calls&&s.storage==&owner.member[calls]&&s.target==owner.member[calls].mPtr,"actual storage identity lost");++calls;
  if(reject){e="refused";return false;}return true;
 }
};
struct Decoder:ActorArchive {
 Mode m;void* value;int refs=0;Decoder(Mode mode,void*v):m(mode),value(v){}
 Mode mode()const override{return m;}double clock_now()const override{return 0;}
 bool scalar(const char*,ScalarKind,void*)override{return true;}
 bool reference(const char*,RefKind k,void*&v)override{check(k==RefKind::Creature,"wrong role");++refs;v=value;return true;}
 bool handle(const char*,RefKind,u32&)override{return true;}bool fail(const char*)override{return false;}
};
int main(){Owner owner;Target target;owner.member[0].mPtr=&target;std::string e;Visitor visitor(owner);StrongStorageArchive raw(visitor,e);PrefixArchive outer(raw,"outer"),inner(outer,"inner");
 check(inner.strongRef("slot",owner.member[0],&owner,"Owner","member",0),"nonnull observation failed");
 check(inner.strongRef("slot",owner.member[1],&owner,"Owner","member",1),"null storage omitted");
 check(visitor.calls==2&&target.callbacks==0&&owner.member[0].mPtr==&target&&!owner.member[1].mPtr,"observer mutated pointers/counts");
 int ordinary=7;void* ptr=&target;check(raw.scalar("ordinary",ScalarKind::S32,&ordinary)&&raw.reference("ordinary",RefKind::Creature,ptr)&&ordinary==7&&ptr==&target&&visitor.calls==2,"ordinary field observed/mutated");
 Decoder validate(Mode::Validate,nullptr);check(validate.strongRef("x",owner.member[0],&owner,"Owner","member",0)&&owner.member[0].mPtr==&target&&validate.refs==1,"Validate wrote live");
 Decoder apply(Mode::Apply,nullptr);check(apply.strongRef("x",owner.member[0],&owner,"Owner","member",0)&&!owner.member[0].mPtr&&target.callbacks==0,"Apply callback/default delegation wrong");
 check(!raw.strongRef("bad",owner.member[0],nullptr,"Owner","member",0),"invalid metadata accepted");
 Visitor refusing(owner);refusing.reject=true;StrongStorageArchive failure(refusing,e);PrefixArchive p(failure,"outer"),q(p,"inner");check(!q.strongRef("slot",owner.member[0],&owner,"Owner","member",0),"visitor refusal swallowed");
 std::cout<<"8 strong storage bridge controls PASS (native SmartPtr storage, no gameplay)\n";
}
