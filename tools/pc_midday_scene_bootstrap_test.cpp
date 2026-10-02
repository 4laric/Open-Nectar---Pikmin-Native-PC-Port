#include "pc_midday_scene_bootstrap.h"
#include <iostream>
#include <stdexcept>
using namespace pc_midday;
int n=0;void check(bool ok,const char* why){++n;if(!ok)throw std::runtime_error(why);}
int main(){try{
    std::string e;Bytes original,wire,after;
    check(encodeSceneBootstrap(sceneBootstrap(),original,e),"actual native bootstrap defaults");
    SceneBootstrap staged=sceneBootstrap();
    SceneBootstrap saved;saved.colorGranted[0]=saved.colorGranted[2]=true;saved.initialColorRegistered=true;saved.redsQueued=saved.redsReady=saved.gameplayReady=true;saved.initialField=17;
    check(encodeSceneBootstrap(saved,wire,e)&&wire.size()==23,"named bounded bootstrap record");
    SceneBootstrap decoded;check(decodeSceneBootstrap(wire,decoded,e)&&decoded.initialField==17&&decoded.redsReady&&decoded.colorGranted[2],"bootstrap exact typed roundtrip");
    RestoreGate gate{true,true,true,true,true,true,true};
    for(unsigned f=0;f<7;++f){auto bad=gate;bool* fields[]={&bad.freshProcess,&bad.paused,&bad.zeroInput,&bad.birthEffectsSuppressed,&bad.rewardsSuppressed,&bad.rngDrawsSuppressed,&bad.audioVoicesSuppressed};*fields[f]=false;
        check(!applySceneBootstrap(staged,wire,bad,e)&&encodeSceneBootstrap(staged,after,e)&&after==original,"fence refusal preserves whole runtime singleton");}
    for(unsigned f=0;f<6;++f){auto bad=wire;if(f==0)bad.pop_back();if(f==1)bad[8]=2;if(f==2)bad[12]=2;if(f==3)bad[21]=2;if(f==4)bad[16]=0;if(f==5)bad[15]=0;
        check(!applySceneBootstrap(staged,bad,gate,e)&&encodeSceneBootstrap(staged,after,e)&&after==original,"malformed/inconsistent bootstrap refuses atomically");}
    check(!applySceneBootstrap(sceneBootstrap(),wire,gate,e)&&encodeSceneBootstrap(sceneBootstrap(),after,e)&&after==original,"direct live-runtime destination refuses under otherwise full fence");
    check(applySceneBootstrap(staged,wire,gate,e)&&encodeSceneBootstrap(staged,after,e)&&after==wire,"staged bootstrap apply exact");
    // Pending automatic withdrawal is valid: retain queued target, do not finish it.
    saved.redsReady=false;check(encodeSceneBootstrap(saved,wire,e)&&applySceneBootstrap(staged,wire,gate,e)&&!staged.redsReady&&staged.redsQueued&&staged.initialField==17,"pending queued withdrawal retained");
    check(encodeSceneBootstrap(sceneBootstrap(),after,e)&&after==original,"successful staging never publishes live bootstrap global");
    std::cout<<"PASS "<<n<<" actual bootstrap module controls; no scene/gameplay resumed\n";return 0;
}catch(const std::exception& x){std::cerr<<"FAIL "<<n<<": "<<x.what()<<"\n";return 1;}}
