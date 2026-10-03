// Constructor/disposal diagnostic using the real engine, never a Shape or
// Scene double. No game section, actor, World or Services is installed.
#include "Shape.h"
#include "Texture.h"
#include "system.h"
#include "sysNew.h"
#include "pc_p2_original_selected_shape_stream.h"
#include "gl/pc_gfx.h"
#include "netplay/pc_netplay_sha256.h"
#include <cstdio>
#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
unsigned checks=0;
void check(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
std::string digest(const std::string& bytes){unsigned char d[32];pc_netplay_sha::sha256(bytes.data(),bytes.size(),d);std::string out;for(auto b:d){out+="0123456789abcdef"[b>>4];out+="0123456789abcdef"[b&15];}return out;}
struct Input {std::string role,sha,bytes;};
std::vector<Input> inputs(const char* manifest){
 std::ifstream file(manifest);check(bool(file),"constructor input manifest unavailable");
 std::string line;std::getline(file,line);std::string prefix="p2-original/piki-bodies/red/";
 if(line.rfind("SHAPE171_SOURCE_BANK_INPUTS\t1\t",0)==0){
  std::istringstream header(line);std::string magic,version,species,receipt,extra;
  std::getline(header,magic,'\t');std::getline(header,version,'\t');std::getline(header,species,'\t');std::getline(header,receipt,'\t');
  check((species=="purple"||species=="white")&&receipt.size()==64&&std::all_of(receipt.begin(),receipt.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');})&&!std::getline(header,extra,'\t'),"source species manifest identity");
  prefix="p2-original/piki-bodies/"+species+"/";
 }else check(line.rfind("SHAPE171_CONSTRUCTOR_INPUTS\t1\t",0)==0,"constructor manifest schema");
 std::vector<Input> result;std::map<std::string,bool> seen;size_t total=0;
 while(std::getline(file,line)){
  std::istringstream fields(line);Input in;std::string size,path;std::getline(fields,in.role,'\t');std::getline(fields,in.sha,'\t');std::getline(fields,size,'\t');std::getline(fields,path,'\t');
  check(in.role.rfind(prefix,0)==0&&in.role.size()>4&&in.role.compare(in.role.size()-4,4,".mod")==0,"wrong model role");
  check(seen.emplace(in.role,true).second&&in.sha.size()==64&&!path.empty(),"duplicate/invalid model identity");
  std::ifstream model(path,std::ios::binary);check(bool(model),"selected model unavailable");
  in.bytes.assign(std::istreambuf_iterator<char>(model),{});check(in.bytes.size()==std::stoull(size)&&in.bytes.size()>=32&&in.bytes.size()<=8*1024*1024&&digest(in.bytes)==in.sha,"selected model bytes differ");
  total+=in.bytes.size();check(total<=128*1024*1024,"model bytes exceed bank budget");result.push_back(std::move(in));
 }
 check(result.size()==171,"selected model count differs");return result;
}
size_t gfxCount(){size_t count=0;auto* head=&gsys->mGfxobjInfo;for(auto* p=head->mNext;p!=head;p=p->mNext){check(p&&++count<20000,"broken native graphics list");}return count;}
bool noExternal(const Shape& s){
 if(s.mFallbackTexAttrCount||s.mAttrListMatCount||s.mTextureNameList||s.mLightGroup.mChild||s.mRouteGroup.mChild||s.mCollisionInfo.mChild)return false;
 for(int i=0;i<s.mTexAttrCount;++i)if((s.mTexAttrList[i].mTextureIndex&0x8000)||s.mTexAttrList[i].mTextureName)return false;
 return true;
}
struct Owner {
 PikiPcAllocationArena arena;std::vector<Shape*> shapes;bool busy=false;
 bool canDispose()const{return !busy&&!gsys->mIsRendering&&arena.canReleaseStorage();}
 bool dispose(){
  // Creating-thread/capture refusal occurs BEFORE any graphics mutation.
  if(!canDispose())return false;
  pc_gfx_forget_owned_native_storage([](const void* p,void* context){return static_cast<Owner*>(context)->arena.owns(p);},this);
  auto* head=&gsys->mGfxobjInfo;
  for(auto* info=head->mNext;info!=head;){auto* next=info->mNext;
   if(arena.owns(info)){
    if(info->mAttached){info->detach();info->mAttached=false;}
    if(info->mId.mId=='_tex'){auto* texture=static_cast<TexobjInfo*>(info)->mTexture;if(texture&&texture->mTexObj)pc_gfx_release_texture(texture->mTexObj);}
    info->remove();
   }info=next;
  }
  if(!arena.releaseStorage())return false;shapes.clear();return true;
 }
 void load(const std::vector<Input>& list,size_t count,int truncate=-1,bool probe=false){
  check(!busy&&shapes.empty()&&!arena.storage().liveBlocks,"resource owner already busy/retained");
  busy=true;Shape* prior=gsys->mCurrentShape;
  try{for(size_t i=0;i<count;++i){
   // The controlled corruption is a private copy. Original authenticated bytes
   // and identity list remain unchanged; it grants no resource admission.
   std::string damaged;if(int(i)==truncate)damaged=list[i].bytes.substr(0,list[i].bytes.size()/2);
   const auto& bytes=int(i)==truncate?damaged:list[i].bytes;
   Shape* shape=nullptr;
   {PikiPcAllocationCapture capture(arena);check(capture.valid(),"actual native capture refused");
    p2original::SelectedShapeStream stream(bytes,list[i].role.c_str());
    shape=new Shape;shape->mName=StdSystem::stringDup(list[i].role.c_str());gsys->mCurrentShape=shape;
    shape->read(stream);check(noExternal(*shape),"model requests unselected external resources");
    if(probe&&i==0){const auto blocks=arena.storage().liveBlocks;const auto graphics=gfxCount();PikiPcAllocationCapture nested(arena);check(!nested.valid()&&!dispose()&&arena.storage().liveBlocks==blocks&&gfxCount()==graphics,"nested capture/disposal mutated live graph");}
    shape->resolveTextureNames();shape->initialise();shape->initIni(false);shape->optimize();
    check(shape->mJointCount>0&&shape->mVertexCount>0&&shape->mVertexList&&shape->mJointList,"native constructor lacks actual model geometry");
   }
   // Owner bookkeeping is outside the resource capture so its capacity is not
   // released underneath the still-live C++ container during graph disposal.
   shapes.push_back(shape);
  }}catch(...){gsys->mCurrentShape=prior;busy=false;throw;}
  gsys->mCurrentShape=prior;busy=false;
 }
};
}
int main(int argc,char** argv){try{
 check(argc==2,"usage: source_shape171_runtime constructor-inputs.tsv");
 const auto selected=inputs(argv[1]);
 // Construct the real System. A real bounded App heap supplies the reader's
 // getFree bookkeeping; captured root/nested/aligned storage uses sysNew37.
 System system;std::vector<unsigned char> heap(1024*1024);system.mHeaps[SYSHEAP_App].init("shape171-diagnostic",AYU_STACK_GROW_DOWN,heap.data(),int(heap.size()));
 system.mIsRendering=false;system.mCurrentShape=nullptr;system.setTextureBase("","");system.setHeap(SYSHEAP_App);
 const auto baseline=piki_pc_allocation_stats();const auto initialGfx=gfxCount();
 Owner foreign;foreign.load(selected,1);const auto foreignStorage=foreign.arena.storage();const auto foreignGfx=gfxCount();
 size_t peakBytes=0;
 for(unsigned cycle=0;cycle<3;++cycle){Owner own;own.load(selected,171,-1,true);check(own.shapes.size()==171,"complete native model count");
  const auto storage=own.arena.storage();peakBytes=std::max(peakBytes,storage.liveBytes);check(storage.liveBlocks>171&&storage.liveBytes>0,"root/nested/aligned arena empty");
  const auto liveGfx=gfxCount();check(liveGfx>foreignGfx,"real texture registration absent");
  bool denied=false;std::thread other([&]{denied=!own.canDispose()&&!own.dispose()&&!own.arena.releaseStorage();});other.join();
  check(denied&&own.arena.storage().liveBlocks==storage.liveBlocks&&gfxCount()==liveGfx,"wrong thread changed native graph/storage");
  check(own.dispose()&&own.arena.storage().liveBlocks==0&&gfxCount()==foreignGfx,"native disposal did not return graphics plateau");
  check(foreign.arena.storage().liveBlocks==foreignStorage.liveBlocks,"foreign model storage changed");
  std::printf("SHAPE171_CYCLE cycle=%u models=171 peak_bytes=%zu gfx_plateau=%zu\n",cycle,storage.liveBytes,foreignGfx);
 }
 for(int partial:{0,85,170}){Owner own;bool refused=false;try{own.load(selected,size_t(partial+1),partial);}catch(const std::exception&){refused=true;}
  check(refused&&own.arena.storage().liveBlocks>0,"truncated native constructor did not retain partial owner");
  check(own.dispose()&&own.arena.storage().liveBlocks==0&&gfxCount()==foreignGfx,"partial constructor cleanup failed");
  std::printf("SHAPE171_PARTIAL index=%d cleanup=PASS\n",partial);
 }
 check(foreign.dispose()&&gfxCount()==initialGfx,"foreign real model final cleanup failed");
 const auto final=piki_pc_allocation_stats();
 // Vector/map capacity and first-use platform caches are outside resource tags;
 // resource/graphics zero observations are the actual per-cycle plateau proof.
 check(final.unknownFrees==baseline.unknownFrees,"native cleanup introduced unknown frees");
 std::printf("SHAPE171_NATIVE_CONSTRUCTORS_PASS models=171 cycles=3 partials=3 checks=%u peak_bytes=%zu gfx=%zu baseline_blocks=%zu final_blocks=%zu gameplay=UNTESTED stage=UNADMITTED\n",checks,peakBytes,initialGfx,baseline.liveBlocks,final.liveBlocks);return 0;
}catch(const std::exception& e){std::fprintf(stderr,"SHAPE171_NATIVE_CONSTRUCTORS_FAIL checks=%u reason=%s\n",checks,e.what());return 1;}}
