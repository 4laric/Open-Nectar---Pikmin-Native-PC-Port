#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef ERROR
#undef near
#undef far
#undef small
#endif
#include "sysNew.h"
#include "system.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <thread>

// Standalone allocation controls use the real sysNew.cpp. No System/renderer
// is constructed; any unexpected ordinary heap/debug path must fail loudly.
System* gsys = nullptr;
Stream* sysCon = nullptr;
void System::halt(immut char*, int, immut char*) { std::abort(); }
void* AyuStack::push(int) { std::abort(); }
void Stream::print(immut char*, ...) { std::abort(); }

static unsigned checks=0;
static void require(bool value,const char* message){
    ++checks;
    if(!value){std::fprintf(stderr,"ALLOCATION_ARENA_FAIL %u %s\n",checks,message);std::exit(1);}
}
int main(){
    auto* ordinary=piki_pc_alloc(19);
    PikiPcAllocationArena owner,other;
    require(!owner.owns(ordinary),"ordinary allocation not adopted");
    require(!owner.endCapture(),"unstarted end refused");
    void* root=nullptr;void* aligned=nullptr;void* worker=nullptr;
    {
        PikiPcAllocationCapture capture(owner);
        require(capture.valid()&&owner.capturing(),"actual opt-in capture");
        require(!owner.beginCapture()&&!other.beginCapture(),"nested capture refused");
        root=piki_pc_alloc(32);
        aligned=::operator new[](73,PIKI_ALIGNED(32));
        require((reinterpret_cast<uintptr_t>(aligned)&31)==0,"real native aligned route");
        require(owner.owns(root)&&owner.owns(aligned),"root and native geometry are owned");
        require(owner.owns(static_cast<char*>(root)+31),"interior source subobject owned");
        require(!owner.owns(static_cast<char*>(root)+32),"one-past extent refused");
        require(!owner.owns(ordinary)&&!other.owns(root),"foreign graph refused");
        require(!owner.canReleaseStorage()&&!owner.releaseStorage(),"capture-time preflight/disposal refused");
        const auto live=owner.storage();
        require(live.liveBlocks==2&&live.liveBytes>=105,"actual nested allocation census");
        std::thread background([&]{
            require(!owner.beginCapture()&&!owner.endCapture()&&!owner.canReleaseStorage()&&!owner.releaseStorage(),"foreign thread cannot change owner");
            worker=piki_pc_alloc(47);
        });
        background.join();
        require(!owner.owns(worker),"background allocation remains untagged");
        piki_pc_free(root);root=nullptr;
        require(owner.storage().liveBlocks==1,"normal free retires tagged root");
        try{piki_pc_alloc(std::numeric_limits<size_t>::max());require(false,"overflow must throw");}
        catch(const std::bad_alloc&){require(owner.storage().liveBlocks==1,"allocation failure preserves exact graph census");}
    }
    require(!owner.capturing(),"exception-safe capture ends");
    require(owner.canReleaseStorage(),"actual creator stopped-capture preflight admits");
    const auto ownedBefore=owner.storage();
    std::thread wrongCleanup([&]{require(!owner.canReleaseStorage(),"wrong thread stopped-capture preflight refuses");});wrongCleanup.join();
    require(owner.storage().liveBlocks==ownedBefore.liveBlocks&&owner.storage().liveBytes==ownedBefore.liveBytes,"wrong thread preflight preserves graph before any gfx disposal");
    const auto before=piki_pc_allocation_stats();
    require(owner.releaseStorage(),"owned graph storage disposal");
    const auto after=piki_pc_allocation_stats();
    require(after.liveBlocks+1==before.liveBlocks&&after.liveBytes<before.liveBytes&&after.unknownFrees==before.unknownFrees,"only actual owned graph freed");
    require(owner.storage().liveBlocks==0&&!owner.owns(aligned),"no surviving owner ranges");
    require(owner.releaseStorage(),"repeat disposal inert");
    require(!owner.owns(worker)&&!owner.owns(ordinary),"foreign survivors preserved");
    piki_pc_free(worker);piki_pc_free(ordinary);
    {
        PikiPcAllocationCapture again(owner);require(again.valid(),"same-thread capture reopen");
        root=piki_pc_alloc(8);
    }
    require(owner.owns(root)&&owner.releaseStorage(),"new graph generation owned and disposed");
    PikiPcAllocationArena endedThread;
    std::thread creator([&]{
        PikiPcAllocationCapture capture(endedThread);
        require(capture.valid(),"worker's actual first capture admitted");
    });
    creator.join();
    std::thread successor([&]{
        require(!endedThread.beginCapture()&&!endedThread.releaseStorage(),"later physical thread cannot inherit dead creator authority");
    });
    successor.join();
    {
        PikiPcAllocationArena empty;
        require(empty.beginCapture(),"empty temporary capture admitted");
    }
    require(other.beginCapture()&&other.endCapture(),"arena destruction cannot leave ordinary thread capture active");
    std::printf("ALLOCATION_ARENA_PASS %u actual sysNew controls; renderer not constructed\n",checks);
}
