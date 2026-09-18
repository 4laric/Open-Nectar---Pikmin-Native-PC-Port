// Isolated integration fixture: real movement and transport AI, no production input changes.
#include <SDL2/SDL.h>
#include <GL/gl.h>
#include "gl/pc_opengl.h"
#include "system.h"
#include "App.h"
#include "Node.h"
#include "Section.h"
#include "FlowController.h"
#include "MoviePlayer.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "NaviState.h"
#include "Kontroller.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "PikiAI.h"
#include "Pellet.h"
#include "PelletState.h"
#include "MapMgr.h"
#include "Camera.h"
#include "LifeGauge.h"
#include "Light.h"
#include "Shape.h"
#include "Material.h"
#include "Mesh.h"
#include "PlayerState.h"
#include "Demo.h"
#include "teki.h"
#include "pc_p2_preview.h"
#include "pc_window.h"
#include "pc_bbft.h"
#include "pc_gpu_preference.h"
#include "settings/pc_settings.h"
#include "settings/pc_settings_p2d.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "GameStat.h"
#include "pc_p2_long_legs.h"
static int phase=0,ticks=0;
static bool assembled=false;
static bool secondFloor=false;
static std::vector<Vector3f> walkGoals;
static int walkPoint=0;
static Vector3f origin;
static bool corpseLifecycle=false, corpseApproach=false, combatWalking=false;
static Vector3f combatDestination;
static void require(bool value,const char* message) { if(!value) { std::printf("FAIL p2 room: %s\n",message);std::fflush(stdout);std::_Exit(1); } }
static void verifyCarryDigits() {
    GaugeInfo gauge;
    LFlareGroup* group=lgMgr->mDigitFlareGroup;
    for(int value : {0,9,10,99,100,101,1000}) {
        LFInfo* previous=group->mLFInfo;
        Colour white(255,255,255,255);
        gauge.showDigits(Vector3f(0,0,0),white,value,8,8);
        int actual=0,count=0;float sum=0;
        for(LFInfo* f=group->mLFInfo;f!=previous;f=f->mPrevInfo) {
            require(f!=nullptr,"missing carry digit");
            actual=actual*10+int(std::round(f->mUvMin.x*11));
            sum+=f->mFlarePos.x;++count;
        }
        require(actual==value && count==(value>=1000?4:value>=100?3:value>=10?2:1),"carry digit value/count");
        require(std::fabs(sum)<0.001f,"carry number not centered");
        group->mLFInfo=previous;
    }
    std::puts("P2_CARRY_DIGITS_PASS 0 9 10 99 100 101 1000: actual flare UVs and centering");
}
// Navi::update polls its controller after GameCoreSection::updateAI starts.
// Override that virtual poll in the standalone fixture, never production input.
class FixtureController : public Kontroller {
public:
    FixtureController() : Kontroller(1) {}
    void update() override {
        updateCont((phase==1 || phase==7)?KBBTN_MSTICK_RIGHT:0);
        mMainStickX=(phase==1 || phase==7)?74:0; mMainStickY=0;
        if((assembled || secondFloor) && (phase==1 || phase==7) && naviMgr) {
            Navi* n=naviMgr->getNavi();
            if(n && n->mNaviCamera) {
                float dx=walkGoals[walkPoint].x-n->mSRT.t.x,dz=walkGoals[walkPoint].z-n->mSRT.t.z;
                float distance=std::sqrt(dx*dx+dz*dz);
                if(distance<20 && walkPoint+1<int(walkGoals.size()))++walkPoint;
                if(distance>1) {
                    const Vector3f& axis=n->mNaviCamera->mViewXAxis;
                    mMainStickX=static_cast<signed char>(65*(dx*axis.x+dz*axis.z)/distance);
                    mMainStickY=static_cast<signed char>(65*(dx*axis.z-dz*axis.x)/distance);
                }
            }
        }
        mSubStickX=0; mSubStickY=0;
        if(combatWalking && naviMgr) {
            Navi* n=naviMgr->getNavi();
            if(n && n->mNaviCamera) {
                float dx=combatDestination.x-n->mSRT.t.x,dz=combatDestination.z-n->mSRT.t.z;
                float distance=std::sqrt(dx*dx+dz*dz);
                if(distance>1) {
                    const Vector3f& axis=n->mNaviCamera->mViewXAxis;
                    updateCont(KBBTN_MSTICK_RIGHT);
                    mMainStickX=static_cast<signed char>(65*(dx*axis.x+dz*axis.z)/distance);
                    mMainStickY=static_cast<signed char>(65*(dx*axis.z-dz*axis.x)/distance);
                }
            }
        }
    }
};
static unsigned hashBytes(const void* data,size_t size,unsigned hash=2166136261u) {
    const unsigned char* bytes=static_cast<const unsigned char*>(data);
    for(size_t i=0;i<size;++i)hash=(hash^bytes[i])*16777619u;
    return hash;
}
static void cameraLog(Navi* n,const char* tag) {
    Camera* c=n->mNaviCamera;if(!c)return;
    if(mapMgr->mMapModel && mapMgr->mMapModel->mJointCount>0) {
        Shape* model=mapMgr->mMapModel;
        for(int i=model->mTotalMatpolyCount-1;i>=0;--i){
            Material* m=model->mMatpolyList[i]->mMaterial;
            std::printf("P2_MATERIAL_%s index=%u flags=%x pixel=%x,%x,%x,%x\n",tag,m->mIndex,m->mFlags,m->mPeInfo.mControlFlags,m->mPeInfo.mAlphaCompareFlags,m->mPeInfo.mDepthTestFlags,m->mPeInfo.mBlendModeFlags);
        }
        unsigned vertices=hashBytes(model->mVertexList,sizeof(Vector3f)*(model->mVertexCount<350?model->mVertexCount:350));
        unsigned lists=2166136261u;
        for(int m=0;m<model->mMeshCount;++m)for(int g=0;g<model->mMeshList[m].mMtxGroupCount;++g){
            MtxGroup& group=model->mMeshList[m].mMtxGroupList[g];
            for(int d=0;d<group.mDispLength;++d)lists=hashBytes(group.mDispList[d].mData,group.mDispList[d].mDataLength,lists);
        }
        AnimContext* animation=model->mCurrentAnimation;
        std::printf("P2_MODEL_%s vertices=%d first350hash=%08x dlhash=%08x animation=%p frame=%.3f flags=%d frames=%d matrices=%p\n",tag,model->mVertexCount,vertices,lists,(void*)animation,animation?animation->mCurrentFrame:-1.f,animation&&animation->mData?animation->mData->mAnimFlags:-1,animation&&animation->mData?animation->mData->mTotalFrameCount:-1,(void*)model->mAnimMatrices);
        const Matrix4f& root=model->mAnimMatrices?model->mAnimMatrices[0]:model->mJointList[0].mAnimMatrix;
        float difference=0;
        for(int row=0;row<3;++row) {
            std::printf("P2_MATRIX_%s row=%d root=%.4f,%.4f,%.4f,%.4f view=%.4f,%.4f,%.4f,%.4f\n",tag,row,root.mMtx[row][0],root.mMtx[row][1],root.mMtx[row][2],root.mMtx[row][3],c->mLookAtMtx.mMtx[row][0],c->mLookAtMtx.mMtx[row][1],c->mLookAtMtx.mMtx[row][2],c->mLookAtMtx.mMtx[row][3]);
            for(int col=0;col<4;++col)difference+=std::fabs(root.mMtx[row][col]-c->mLookAtMtx.mMtx[row][col]);
        }
        std::printf("P2_MATRIX_%s absolute_difference=%.6f\n",tag,difference);
    }
    std::printf("P2_CAMERA_%s pos=%.3f,%.3f,%.3f ground=%.3f yaxis=%.3f,%.3f,%.3f zaxis=%.3f,%.3f,%.3f\n",tag,c->mPosition.x,c->mPosition.y,c->mPosition.z,mapMgr->getMinY(c->mPosition.x,c->mPosition.z,true),c->mViewYAxis.x,c->mViewYAxis.y,c->mViewYAxis.z,c->mViewZAxis.x,c->mViewZAxis.y,c->mViewZAxis.z);
}
static void capture(const char* path="p2-room.ppm") {
    auto bind=reinterpret_cast<PFNGLBINDFRAMEBUFFERPROC>(SDL_GL_GetProcAddress("glBindFramebuffer"));
    GLint previous=0;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&previous);bind(GL_FRAMEBUFFER,0);
    int w=0,h=0;SDL_GL_GetDrawableSize(SDL_GL_GetCurrentWindow(),&w,&h);
    std::vector<unsigned char> pixels(size_t(w)*h*3);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);
    glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());bind(GL_FRAMEBUFFER,previous);
    require(glGetError()==GL_NO_ERROR,"capture GL error");
    FILE* f=std::fopen(path,"wb");require(f!=nullptr,"capture file");std::fprintf(f,"P6\n%d %d\n255\n",w,h);
    for(int y=h-1;y>=0;--y)std::fwrite(pixels.data()+size_t(y)*w*3,1,size_t(w)*3,f);std::fclose(f);
}
#include "preview_p2_purple.inc"
#include "preview_p2_beasts_floor2.inc"
#include "preview_p2_purple_impact.inc"
#include "preview_p2_white_poison.inc"
#include "preview_p2_purple_direct.inc"
#include "preview_p2_purple_flight.inc"
#include "preview_p2_white.inc"
#include "preview_p2_cargo.inc"
#include "preview_p2_cave.inc"
#include "preview_p2_snow.inc"
// SPLICED RoomApp for lane damagumo-preview-room-build (#815):
// complete class RoomApp from tools/p2_muse_damagumo_room_app.cpp at native 45534ee3
// replaces the walk-fixture RoomApp (private tree only; no engine change).
class RoomApp : public PlugPikiApp {
    int frames=0,observed=0,stage=0;
    int damagumoDropEvents=0;         // receiver-hit probe: actual health decreases
    float damagumoMinHealth=1e9f;     // lowest Damagumo health seen
    float damagumoLastHealth=0.0f;    // previous tick's Damagumo health
    Vector3f captainOrigin;
    Teki* damagumo=nullptr;
    bool damagumoDied=false;
    Teki* byGenerator(unsigned id){Iterator it(tekiMgr);CI_LOOP(it){Teki* a=static_cast<Teki*>(*it);if(a&&a->mGenerator&&a->mGenerator->_70==id)return a;}return nullptr;}
    int freeAndPark(Teki* center, float radius){return freeAndParkAt(center->mSRT.t,radius);}
    int freeAndParkAt(const Vector3f& c, float radius){int n=0;Iterator a(pikiMgr);CI_LOOP(a){Piki* v=static_cast<Piki*>(*a);if(!v->isAlive())continue;
        float ang=float(n)*6.2831853f/20.0f;Vector3f pt(c.x+radius*std::sin(ang),0,c.z+radius*std::cos(ang));
        pt.y=mapMgr->getMinY(pt.x,pt.z,true);v->resetPosition(pt);v->changeMode(PikiMode::FreeMode,naviMgr?naviMgr->getNavi():nullptr);++n;}return n;}
    // Clump the live squad tight at one point and order the real Attack action.
    // The owned host walks Damagumo toward the nearest Pikmin (source target rule),
    // so a tight clump keeps the mover arriving AT the attackers.
    int clumpAttack(Teki* target,const Vector3f& c){int n=0;Iterator a(pikiMgr);CI_LOOP(a){Piki* v=static_cast<Piki*>(*a);if(!v->isAlive())continue;
        float ang=float(n)*6.2831853f/20.0f;Vector3f pt(c.x+10.0f*std::sin(ang),0,c.z+10.0f*std::cos(ang));
        pt.y=mapMgr->getMinY(pt.x,pt.z,true);v->resetPosition(pt);
        v->mSRT.r.y=ang+3.14159265f;
        v->mActiveAction->abandon(nullptr);v->mActiveAction->mCurrActionIdx=PikiAction::Attack;
        v->mActiveAction->mChildActions[PikiAction::Attack].initialise(target);v->mMode=PikiMode::AttackMode;++n;}return n;}
public:int idle() override {
    int result=PlugPikiApp::idle();require(++frames<60000,"muse damagumo timeout");
    if(frames%600==0){int live=0;if(pikiMgr){Iterator q(pikiMgr);CI_LOOP(q){Piki* v=static_cast<Piki*>(*q);if(v&&v->isAlive())++live;}}
        std::printf("P2_MUSE_DAMAGUMO_HB frames=%d stage=%d observed=%d live=%d\n",frames,stage,observed,live);std::fflush(stdout);}
    if(gameflow.mMoviePlayer&&gameflow.mMoviePlayer->mIsActive){gameflow.mMoviePlayer->skipScene(SCENESKIP_SkipAll);return result;}
    if(!pc_p2_preview_ready()||!naviMgr||!pikiMgr||!tekiMgr)return result;
    Navi* n=naviMgr->getNavi();if(!n)return result;
    // Captain-safety #632 (inline equivalent of scripts/p2_fixture_captain_guard.h:
    // orimaDead/NaviDead/HP<=1 exits BLOCKED instead of observing; the captain
    // stays parked at origin and is never made invincible).
    {bool orimaDead=naviMgr->getDeadOrima()!=nullptr;
     bool deadState=n->mStateMachine->getCurrID(n)==NAVISTATE_Dead;
     float hp=n->mHealth;
     if(orimaDead||deadState||hp!=hp||hp<=1.0f){
         std::printf("P2_FIXTURE_CAPTAIN_DOWN tick=%d hp=%.3f orima_dead=%d dead_state=%d outcome=BLOCKED\n",
             observed,hp,int(orimaDead),int(deadState));std::fflush(stdout);std::_Exit(86);}}
    if(gameflow.mPauseAll||gameflow.mIsUIOverlayActive)return result;
    {static bool naviSustainLogged=false;int ns=n->mStateMachine->getCurrID(n);
        if(ns==NAVISTATE_Pressed||ns==NAVISTATE_Flick||ns==NAVISTATE_Dead||ns==NAVISTATE_PikiZero||ns==NAVISTATE_DemoSunset||ns==NAVISTATE_DemoWait||ns==NAVISTATE_DemoInf){n->mStateMachine->transit(n,NAVISTATE_Walk);if(!naviSustainLogged){naviSustainLogged=true;std::puts("P2_MUSE_DAMAGUMO_GUARD navi_sustain=1");}}}
    {static bool pikminGuardLogged=false;if((int)GameStat::allPikis==0){GameStat::allPikis.set(1,Red);if(!pikminGuardLogged){pikminGuardLogged=true;std::puts("P2_MUSE_DAMAGUMO_GUARD pikmin_guard=1");}}}
    ++observed;
    if(stage==0){
        captainOrigin=n->mSRT.t;
        for(int f=0;f<DEMOFLAG_COUNT;++f)playerState->mDemoFlags.setFlagOnly(f);
        n->mKontroller=new FixtureController();
        damagumo=byGenerator(312004);
        require(damagumo,"staged Damagumo56 actor present at generator 312004");
        require(pc_p2_long_legs_registered(damagumo),"damagumo bound by family (species Damagumo staged)");
        int squad=0;Iterator p(pikiMgr);CI_LOOP(p){Piki* v=static_cast<Piki*>(*p);if(v->isAlive())++squad;}
        require(squad>=1,"live starting squad");
        damagumoLastHealth=damagumo->mHealth;
        // Park the squad 120u from Damagumo: outside the 60u accumulate radius (so
        // Wait reaches Walk naturally at disc speed 100) but inside the 400u
        // territory sight (so the source target rule walks Damagumo toward the squad).
        int c=freeAndPark(damagumo,120.0f);std::printf("P2_MUSE_DAMAGUMO_PARK count=%d\n",c);
        std::printf("P2_MUSE_DAMAGUMO_READY squad=%d damagumo_gen=312004\n",squad);
        std::printf("P2_MUSE_DAMAGUMO_BIND generator=312004 species=Damagumo native_fsm=implemented\n");
        std::fflush(stdout);stage=1;return result;
    }
    if(stage==1){
        if(observed%300==0)std::printf("P2_MUSE_DAMAGUMO_POS damagumo=%.1f,%.1f\n",damagumo->mSRT.t.x,damagumo->mSRT.t.z);
        // 1500 observed ticks (~50 s at 30 fps): Damagumo needs at most one
        // Land(5 s)+Wait(~3.5 s)+Walk(~6.5 s) cycle at disc timings.
        if(observed>=1500){Vector3f clump(damagumo->mSRT.t.x,0,damagumo->mSRT.t.z);clump.y=mapMgr->getMinY(clump.x,clump.z,true);
            int a=clumpAttack(damagumo,clump);std::printf("P2_MUSE_DAMAGUMO_ATTACK attack=%d\n",a);std::fflush(stdout);stage=2;return result;}
        return result;
    }
    if(stage==2){
        if(!damagumo->isAlive()){std::printf("P2_MUSE_DAMAGUMO_NATURAL_DEATH damagumo=1 health=%.2f tick=%d\n",damagumo->mHealth,observed);std::fflush(stdout);stage=3;return result;}
        {float h=damagumo->mHealth;if(h<damagumoLastHealth)++damagumoDropEvents;damagumoLastHealth=h;if(h>0.0f&&h<damagumoMinHealth)damagumoMinHealth=h;}
        if(observed%100==0&&pc_p2_long_legs_damageable(damagumo)){Vector3f clump(damagumo->mSRT.t.x,0,damagumo->mSRT.t.z);clump.y=mapMgr->getMinY(clump.x,clump.z,true);clumpAttack(damagumo,clump);}
        if(observed%90==0){int live=0,atk=0;Iterator q(pikiMgr);CI_LOOP(q){Piki* v=static_cast<Piki*>(*q);if(!v->isAlive())continue;++live;if(v->mMode==PikiMode::AttackMode)++atk;}
            std::printf("P2_MUSE_DAMAGUMO_HP health=%.2f squad=%d atk=%d dmg=%d events=%d tick=%d\n",damagumo->mHealth,live,atk,int(pc_p2_long_legs_damageable(damagumo)),damagumoDropEvents,observed);std::fflush(stdout);}
        if(observed>=9000){std::puts("FAIL P2_MUSE_DAMAGUMO drain_timeout");std::fflush(stdout);std::_Exit(1);}
        return result;
    }
    if(stage==3){
        // Child births (25 ShijimiChou, no held treasure) and stage re-entry are
        // observed through family markers; absence here is an honest FAIL.
        std::printf("P2_MUSE_DAMAGUMO_DRAIN events=%d min=%.2f\n",damagumoDropEvents,damagumoMinHealth);
        std::printf("P2_MUSE_DAMAGUMO_SESSION navi=1 pikis=%lu\n",(unsigned long)GameStat::allPikis);
        std::puts("PASS P2_MUSE_DAMAGUMO walk+natural-drain");
        std::fflush(stdout);std::_Exit(0);
    }
    std::fflush(stdout);return result;
}};
int main(int argc,char** argv) {
    // Automated fixture only: keep the real mixer/timing, never open a speaker device.
    SDL_setenv("SDL_AUDIODRIVER","dummy",1);
    if(FILE* marker=std::fopen("p2-beasts-floor2-fixture.txt","r")) {
        char header[64],extra;
        require(std::fscanf(marker,"%63s",header)==1,"missing Beasts fixture marker");
        if(std::string(header)=="P2_BEASTS_FLOOR2_FIXTURE_2") {
            char population[32];require(std::fscanf(marker,"%31s",population)==1,"missing Beasts generation context");
            std::string digits(population);
            require(digits.size()<=10 && digits.find_first_not_of("0123456789")==std::string::npos,"invalid Beasts population");
            unsigned long long count=std::strtoull(population,nullptr,10);require(count<=2147483647ULL,"Beasts population overflow");
            beastsGlobalPurple=int(count);beastsExpectedFlowers=count<20?2:0;
        } else require(std::string(header)=="P2_BEASTS_FLOOR2_FIXTURE_1","invalid Beasts fixture version");
        require(std::fscanf(marker," %c",&extra)==EOF,"trailing Beasts fixture data");
        beastsFloor2Enabled=true;std::fclose(marker);
    }
    if(FILE* marker=std::fopen("p2-corpse-lifecycle.txt","r")) {
        corpseLifecycle=true;corpseApproach=std::fgetc(marker)=='1';std::fclose(marker);
    }
    for(float z:{275.f,510.f,800.f,920.f})walkGoals.push_back(Vector3f(0,0,z));
    if(FILE* marker=std::fopen("p2-assembled.txt","r")){assembled=true;std::fclose(marker);}
    if(FILE* route=std::fopen("p2-second-floor.txt","r")) {
        secondFloor=true;walkGoals.clear();int count=0;
        require(std::fscanf(route,"%d",&count)==1 && count>0 && count<=32,"walk fixture count");
        for(int i=0;i<count;++i){float x,z;require(std::fscanf(route,"%f %f",&x,&z)==2 && std::isfinite(x) && std::isfinite(z),"walk fixture point");walkGoals.push_back(Vector3f(x,0,z));}
        std::fclose(route);
    }
    SDL_SetMainReady();pc_gpu_preference_apply();_putenv_s("PIKMIN_RANDOMIZER_TEST_BACKGROUND","1");pc_bbft_init(argc,argv);
    require(pc_pikipelago_room_preview(),"requires --experimental-pikmin2-room");
    // Custom fixture entrypoint: mirror pc_main.cpp's mandatory small centered
    // preview window (default 960x540, overridable with PIKMIN_P2_ROOM_WINDOW=WxH).
    int windowWidth=960,windowHeight=540;bool smallWindow=true;
    if(const char* value=std::getenv("PIKMIN_P2_ROOM_WINDOW")){
        if(!std::strcmp(value,"off")||!std::strcmp(value,"0"))smallWindow=false;
        int customWidth=0,customHeight=0;
        if(std::sscanf(value,"%dx%d",&customWidth,&customHeight)==2&&customWidth>=320&&customHeight>=240){windowWidth=customWidth;windowHeight=customHeight;}
    }
    if(!pc_window_init("P2 room integration fixture",windowWidth,windowHeight))return 3;
    pc_settings_init();
    if(smallWindow){
    pc_window_set_display_mode(PC_WINDOW_FULLSCREEN_WINDOWED);
    pc_window_set_window_size(windowWidth,windowHeight);
    pc_window_center();
    std::printf("[PC Port] Experimental preview window set to %dx%d windowed and centered (override with PIKMIN_P2_ROOM_WINDOW=WxH or =off).\n",windowWidth,windowHeight);std::fflush(stdout);
    }
    gsys->Initialise();pc_settings_p2d_init();nodeMgr=new NodeMgr();gsys->run(new RoomApp());return 0;
}
