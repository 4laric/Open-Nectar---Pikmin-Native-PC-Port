#include "pc_p2_kurage_visual.h"
#include "Graphics.h"
#include "Shape.h"
#include "Texture.h"
#include "gameflow.h"
#include "sysNew.h"
#include "teki.h"
#include <cstdio>
#include <filesystem>
#include <map>
#include <string>
namespace {
Shape* sWait = nullptr;
Shape* sAttack = nullptr;
bool sReady = false;
// Converted per-motion poses.  Each is one static source pose; the host selects
// by the current FSM state so the drawn pose follows the source motion rather
// than only wait/attack.
std::map<std::string, Shape*> sShapes;
// Greater Spotted Jellyfloat (OniKurage, 72) poses, same names.
std::map<std::string, Shape*> sShapesGreater;
bool sReadyGreater = false;
Shape* load(const char* path)
{
    if (!std::filesystem::exists(std::filesystem::path("assets/dataDir") / path)) return nullptr;
    const int heap = gsys->setHeap(SYSHEAP_App);
    Shape* shape = gameflow.loadShape(path, true);
    if (shape)
        for (int i = 0; i < shape->mTexAttrCount; ++i)
            if (shape->mTexAttrList[i].mTexture) shape->mTexAttrList[i].mTexture->attach();
    gsys->setHeap(heap);
    return shape;
}
}
bool pc_p2_kurage_visual_setup()
{
    if (sReady) return true;
    Shape* wait = load("courses/pikmin2room/kurage_wait.mod");
    Shape* attack = load("courses/pikmin2room/kurage_attack.mod");
    if (!wait || !attack) return false;
    sWait = wait;
    sAttack = attack;
    sShapes["wait"] = wait;
    sShapes["attack"] = attack;
    // Optional source poses; a missing file simply keeps the wait/attack pair.
    static const char* const optional[] = { "move1", "move2", "type1", "type2",
        "flick1", "flick2", "dead1", "dead2" };
    int loaded = 0;
    for (const char* name : optional) {
        std::string path = std::string("courses/pikmin2room/kurage_") + name + ".mod";
        if (Shape* shape = load(path.c_str())) { sShapes[name] = shape; ++loaded; }
    }
    std::printf("P2_KURAGE_VISUAL_POSES optional_loaded=%d/%d\n", loaded, 8);
    std::fflush(stdout);
    sReady = true;
    return true;
}
void pc_p2_kurage_visual_reset()
{
    sShapesGreater.clear();
    sReadyGreater = false;
    sWait = nullptr;
    sAttack = nullptr;
    sReady = false;
    sShapes.clear();
}
Shape* pc_p2_kurage_visual_wait_shape() { return sWait; }
Shape* pc_p2_kurage_visual_attack_shape() { return sAttack; }
Shape* pc_p2_kurage_visual_shape(const char* motionBase)
{
    if (!motionBase || !*motionBase) return nullptr;
    auto it = sShapes.find(motionBase);
    return it == sShapes.end() ? nullptr : it->second;
}
// p2kurage::State: Dead=0, Wait=1, Move=2, Chase=3, Attack=4, Fall=5, Land=6,
// Ground=7, TakeOff=8, FlyFlick=9, GroundFlick=10, Drop=11.
const char* pc_p2_kurage_visual_motion_for_state(int state)
{
    switch (state) {
    case 0: return "dead1";
    case 1: return "wait";
    case 2: return "move1";
    case 3: return "move1";
    case 4: return "attack";
    // Kurage.h AnimID: Land = move2, TakeOff = type1, Fall = type2 (wave 3
    // flyers corrected the Fall/Land/TakeOff pose mapping, #960).
    case 5: return "type2";
    case 6: return "move2";
    case 7: return "wait";
    case 8: return "type1";
    case 9: return "flick1";
    case 10: return "flick2";
    case 11: return "type2";
    default: return nullptr;
    }
}
bool pc_p2_kurage_visual_draw(BTeki* actor, Graphics& gfx, const Matrix4f& matrix, bool corpse)
{
    if (!sReady || !actor) return false;
    Shape* shape = corpse || actor->mTekiAnimator->getCurrentMotionIndex() != TekiMotion::Attack ? sWait : sAttack;
    if (!shape) return false;
    shape->updateAnim(gfx, matrix, nullptr, actor);
    shape->drawshape(gfx, *gfx.mCamera, nullptr);
    return true;
}

bool pc_p2_kurage_visual_setup_greater()
{
    if (sReadyGreater) return true;
    Shape* wait = load("courses/pikmin2room/onikurage_wait.mod");
    Shape* attack = load("courses/pikmin2room/onikurage_attack.mod");
    if (!wait || !attack) return false;
    sShapesGreater["wait"] = wait;
    sShapesGreater["attack"] = attack;
    static const char* const optional[] = { "move1", "move2", "type1", "type2",
        "flick1", "flick2", "dead1", "dead2" };
    int loaded = 0;
    for (const char* name : optional) {
        std::string path = std::string("courses/pikmin2room/onikurage_") + name + ".mod";
        if (Shape* shape = load(path.c_str())) { sShapesGreater[name] = shape; ++loaded; }
    }
    std::printf("P2_ONIKURAGE_VISUAL_POSES optional_loaded=%d/%d\n", loaded, 8);
    std::fflush(stdout);
    sReadyGreater = true;
    return true;
}
Shape* pc_p2_kurage_visual_shape_greater(const char* motionBase)
{
    if (!motionBase || !*motionBase) return nullptr;
    auto it = sShapesGreater.find(motionBase);
    return it == sShapesGreater.end() ? nullptr : it->second;
}
