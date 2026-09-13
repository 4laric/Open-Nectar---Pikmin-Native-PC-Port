// Reuse only private fixture initialization/capture helpers, not its app.
#define main groink_unused_fixture_main
#include "p2_groink_target_runtime.cpp"
#undef main
#include "Texture.h"
#include "sysNew.h"

class DemonVisualApp final : public PlugPikiApp {
    Shape* models[2]{};
    Vector3f mouths[2][2];
    int frames=0, ticks=0;
    bool ready=false;
public:
    int idle() override {
        int result=PlugPikiApp::idle(); require(++frames<1800,"Demon timeout");
        if(gameflow.mMoviePlayer&&gameflow.mMoviePlayer->mIsActive) {
            gameflow.mMoviePlayer->requestSkip(); return result;
        }
        if(!pc_p2_preview_ready()||!naviMgr||!naviMgr->getNavi()) return result;
        if(!ready) {
            std::ifstream in("demon-mouths.txt");
            for(auto& pose:mouths) for(auto& p:pose) {
                in>>p.x>>p.y>>p.z;
                require(bool(in)&&std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z),"mouth input");
            }
            const int heap=gsys->setHeap(SYSHEAP_App);
            models[0]=gameflow.loadShape("courses/pikmin2room/demon0.mod",true);
            models[1]=gameflow.loadShape("courses/pikmin2room/demon17.mod",true);
            for(auto model:models) {
                require(model!=nullptr,"Demon model");
                for(int i=0;i<model->mTexAttrCount;++i)
                    if(model->mTexAttrList[i].mTexture) model->mTexAttrList[i].mTexture->attach();
            }
            gsys->setHeap(heap); ready=true;
        }
        naviMgr->getNavi()->resetPosition(Vector3f(0,0,100));
        ++ticks; return result;
    }
    void draw(Graphics& gfx) override {
        PlugPikiApp::draw(gfx); if(!ready) return;
        const int pose=ticks<60?0:1;
        gfx.setPerspective(gfx.mCamera->mFov,gfx.mCamera->mAspectRatio,gfx.mCamera->mNear,gfx.mCamera->mFar,1);
        gfx.useMaterial(nullptr); gfx.setDepth(true);
        Matrix4f world,view;
        world.makeSRT(Vector3f(1,1,1),Vector3f(0,0,0),Vector3f(0,30,0));
        gfx.mCamera->mLookAtMtx.multiplyTo(world,view);
        models[pose]->updateAnim(gfx,view,nullptr,nullptr);
        models[pose]->drawshape(gfx,*gfx.mCamera,nullptr);
        gfx.useMaterial(nullptr);
        for(int slot=0;slot<2;++slot) {
            const auto p=mouths[pose][slot];
            gfx.setColour(slot?Colour(0,80,255,255):Colour(255,0,0,255),true);
            gfx.drawSphere(Vector3f(p.x,p.y+30,p.z),3,gfx.mCamera->mLookAtMtx);
        }
        if(ticks==30||ticks==90) {
            capture(pose?"demon17.ppm":"demon0.ppm");
            std::printf("DEMON_VISUAL_CAPTURE frame=%d markers=2 no_attachment=1\n",pose?17:0);
        }
        if(ticks>=90) { std::puts("PASS DEMON_VISUAL"); std::fflush(stdout); std::_Exit(0); }
    }
};
int main(int argc,char** argv) {
    SDL_setenv("SDL_AUDIODRIVER","dummy",1); SDL_SetMainReady(); pc_gpu_preference_apply();
    _putenv_s("PIKMIN_RANDOMIZER_TEST_BACKGROUND","1"); pc_bbft_init(argc,argv);
    require(pc_pikipelago_room_preview(),"room flag");
    require(pc_window_init("Demon mouth visual fixture",960,720),"window init");
    pc_settings_init(); gsys->Initialise(); pc_settings_p2d_init(); nodeMgr=new NodeMgr();
    gsys->run(new DemonVisualApp()); return 0;
}
