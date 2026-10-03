// Isolated software-cache control includes the actual renderer TU to exercise
// its private registry. No GL device or engine is constructed by this test.
#include "../pc_port/gl/pc_gfx.cpp"
#include <stdexcept>
#include "sysNew.h"
#include "system.h"
#include <thread>
System* gsys=nullptr;Stream* sysCon=nullptr;
void System::halt(immut char*,int,immut char*){std::abort();}
void* AyuStack::push(int){std::abort();}
void Stream::print(immut char*,...){std::abort();}
#undef main
static unsigned checks=0;
static void require(bool b,const char* text){++checks;if(!b)throw std::runtime_error(text);}
struct NativeRange {const unsigned char* first;size_t size;};
static bool owned(const void* p,void* context){auto& range=*static_cast<NativeRange*>(context);const auto n=reinterpret_cast<uintptr_t>(p),lo=reinterpret_cast<uintptr_t>(range.first);return n>=lo&&n-lo<range.size;}
int main(){try{
PikiPcAllocationArena arena;unsigned char* a=nullptr;{PikiPcAllocationCapture capture(arena);require(capture.valid(),"real native arena capture");a=static_cast<unsigned char*>(piki_pc_alloc(64));}
unsigned char b[64]{};NativeRange range{a,64};
ResidentMesh ownList;ownList.list=a+1;
ResidentMesh ownArray;ownArray.list=b+1;ownArray.arrays[2].base=a+2;
ResidentMesh foreign;foreign.list=b+3;foreign.arrays[2].base=b+4;
sResidentMeshes.emplace(a+1,ownList);sResidentMeshes.emplace(b+1,ownArray);sResidentMeshes.emplace(b+3,foreign);
sDynamicVertexRanges.emplace_back(uintptr_t(a),uintptr_t(a)+64);sDynamicVertexRanges.emplace_back(uintptr_t(b),uintptr_t(b)+64);
sVtxArrays[1]={a+3,12};sVtxArrays[2]={b+3,8};
bool foreignAdmitted=true;std::thread foreignThread([&]{foreignAdmitted=arena.canReleaseStorage();if(foreignAdmitted)pc_gfx_forget_owned_native_storage(owned,&range);});foreignThread.join();
require(!foreignAdmitted&&arena.storage().liveBlocks==1&&sResidentMeshes.size()==3&&sDynamicVertexRanges.size()==2&&sVtxArrays[1].base==a+3,"wrong physical thread refuses BEFORE graphics and storage mutation");
require(arena.canReleaseStorage(),"actual creating thread preflight admits");
pc_gfx_forget_owned_native_storage(nullptr,&range);require(sResidentMeshes.size()==3,"no predicate is inert");
pc_gfx_forget_owned_native_storage(owned,&range);require(sResidentMeshes.size()==1&&sResidentMeshes.count(b+3),"only owned list/array mesh evicted");require(sDynamicVertexRanges.size()==1&&sDynamicVertexRanges[0].first==uintptr_t(b),"foreign dynamic registration retained");require(sVtxArrays[1].base==nullptr&&sVtxArrays[1].stride==0,"owned current GX array cleared");require(sVtxArrays[2].base==b+3&&sVtxArrays[2].stride==8,"foreign current GX array retained");
pc_gfx_forget_owned_native_storage(owned,&range);require(sResidentMeshes.size()==1&&sDynamicVertexRanges.size()==1,"repeat owned disposal inert");
require(arena.releaseStorage()&&arena.storage().liveBlocks==0,"owned storage freed only after scoped gfx cleanup");
std::printf("P2_OWNED_GFX_CACHE PASS %u (actual software registry; no GL/runtime)\n",checks);return 0;
}catch(const std::exception&e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}

