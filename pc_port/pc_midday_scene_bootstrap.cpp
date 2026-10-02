#include "pc_midday_scene_bootstrap.h"
#include <algorithm>
namespace pc_midday {
namespace {
SceneBootstrap runtime;
bool valid(const SceneBootstrap& s,std::string& e){
    if(s.initialField<0||s.initialField>int32_t(MaxActors)||
       (s.redsReady&&!s.redsQueued)||
       (!s.initialColorRegistered&&(s.colorGranted[0]||s.colorGranted[1]||s.colorGranted[2]))){
        e="invalid scene bootstrap logical state";return false;
    }
    return true;
}
void put(Bytes& b,uint32_t x){for(unsigned i=0;i<4;++i)b.push_back(uint8_t(x>>(i*8)));}
uint32_t get(const Bytes& b,size_t i){uint32_t x=0;for(unsigned n=0;n<4;++n)x|=uint32_t(b[i+n])<<(8*n);return x;}
}
SceneBootstrap& sceneBootstrap(){return runtime;}
bool encodeSceneBootstrap(const SceneBootstrap& s,Bytes& out,std::string& e){
    if(!valid(s,e))return false;
    Bytes b={'P','C','B','O','O','T','0','1'};put(b,1);
    for(bool value:{s.colorGranted[0],s.colorGranted[1],s.colorGranted[2],s.initialColorRegistered,s.redsQueued,s.redsReady,s.gameplayReady})b.push_back(value?1:0);
    put(b,uint32_t(s.initialField));out=std::move(b);e.clear();return true;
}
bool decodeSceneBootstrap(const Bytes& b,SceneBootstrap& out,std::string& e){
    const Bytes magic={'P','C','B','O','O','T','0','1'};
    if(b.size()!=23||!std::equal(magic.begin(),magic.end(),b.begin())||get(b,8)!=1){e="bootstrap framing/version mismatch";return false;}
    SceneBootstrap s;bool* fields[]={&s.colorGranted[0],&s.colorGranted[1],&s.colorGranted[2],&s.initialColorRegistered,&s.redsQueued,&s.redsReady,&s.gameplayReady};
    for(unsigned i=0;i<7;++i){if(b[12+i]>1){e="bootstrap noncanonical boolean";return false;}*fields[i]=b[12+i]!=0;}
    uint32_t field=get(b,19);if(field>MaxActors){e="bootstrap field count outside actor bound";return false;}s.initialField=int32_t(field);
    if(!valid(s,e))return false;
    out=s;e.clear();return true;
}
bool applySceneBootstrap(SceneBootstrap& staged,const Bytes& b,const RestoreGate& g,std::string& e){
    if(!g.freshProcess||!g.paused||!g.zeroInput||!g.birthEffectsSuppressed||!g.rewardsSuppressed||!g.rngDrawsSuppressed||!g.audioVoicesSuppressed){e="bootstrap apply requires complete paused fresh-stage fence";return false;}
    SceneBootstrap saved;if(!decodeSceneBootstrap(b,saved,e))return false;
    staged=saved;e.clear();return true;
}
}
