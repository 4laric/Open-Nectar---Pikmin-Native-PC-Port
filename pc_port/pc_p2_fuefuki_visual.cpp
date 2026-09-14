#include "pc_p2_fuefuki_visual.h"

#include "Camera.h"
#include "Graphics.h"
#include "Matrix4f.h"
#include "Shape.h"
#include "Texture.h"
#include "gameflow.h"
#include "sysNew.h"
#include "system.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

namespace {
constexpr int kMaxClips = 16;
constexpr int kMaxPoses = 24;
constexpr std::streamoff kVisualProfileBytes = 16384;

struct ClipBank {
    std::string name;
    int duration = 0;
    int frames[kMaxPoses] = {};
    Shape* poses[kMaxPoses] = {};
    int poseCount = 0;
};

struct VisualState {
    bool ready = false;
    ClipBank clips[kMaxClips];
    int clipCount = 0;
    int active = -1;
    int pose = 0;
    bool drew = false;
};

VisualState sVisual;

bool visualExhausted(std::istringstream& line)
{
    std::string extra;
    return !(line >> extra);
}

bool validClipName(const std::string& name)
{
    return !name.empty() && name.size() <= 32
        && name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_") == std::string::npos;
}

void attachTextures(Shape* shape)
{
    if (!shape) {
        return;
    }
    for (int i = 0; i < shape->mTexAttrCount; ++i) {
        if (shape->mTexAttrList[i].mTexture) {
            shape->mTexAttrList[i].mTexture->attach();
        }
    }
}

Shape* loadPose(const char* filename)
{
    char path[192];
    std::snprintf(path, sizeof(path), "courses/pikmin2room/%s", filename);
    // Force the App heap (Groink/BigTreasure pattern) so a reset movie heap
    // cannot fail to fit the converted mods.
    const int previousHeap = gsys->setHeap(SYSHEAP_App);
    Shape* shape = gameflow.loadShape(path, true);
    gsys->setHeap(previousHeap);
    return shape;
}
} // namespace

bool pc_p2_fuefuki_visual_setup(const char* profilePath)
{
    pc_p2_fuefuki_visual_reset();
    if (!profilePath || !*profilePath) {
        return false;
    }
    std::ifstream input(profilePath);
    if (!input) {
        return false;
    }
    input.seekg(0, std::ios::end);
    if (input.tellg() < 0 || input.tellg() > kVisualProfileBytes) {
        return false;
    }
    input.seekg(0);

    std::string line;
    if (!std::getline(input, line) || line != "P2_FUEFUKI_VISUAL_1") {
        return false;
    }
    struct PendingClip {
        std::string name;
        int poses, duration;
        int frames[kMaxPoses];
    };
    PendingClip pending[kMaxClips];
    int pendingCount = 0;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        std::istringstream values(line);
        std::string key;
        values >> key;
        if (key != "clip" || pendingCount >= kMaxClips) {
            return false;
        }
        PendingClip& clip = pending[pendingCount];
        if (!(values >> clip.name >> clip.poses >> clip.duration) || !validClipName(clip.name)
            || clip.poses < 1 || clip.poses > kMaxPoses || clip.duration < 1
            || clip.duration > 10000) {
            return false;
        }
        int previous = -1;
        for (int i = 0; i < clip.poses; ++i) {
            if (!(values >> clip.frames[i]) || clip.frames[i] <= previous || clip.frames[i] < 0
                || clip.frames[i] >= clip.duration) {
                return false;
            }
            previous = clip.frames[i];
        }
        if (!visualExhausted(values)) {
            return false;
        }
        ++pendingCount;
    }
    if (pendingCount == 0) {
        return false;
    }
    for (int c = 0; c < pendingCount; ++c) {
        const PendingClip& item = pending[c];
        ClipBank& clip = sVisual.clips[sVisual.clipCount];
        clip.name = item.name;
        clip.duration = item.duration;
        for (int i = 0; i < item.poses; ++i) {
            char filename[96];
            std::snprintf(filename, sizeof(filename), "fuefuki_Fuefuki_%s_%02d.mod",
                          item.name.c_str(), i);
            clip.poses[i] = loadPose(filename);
            if (!clip.poses[i]) {
                pc_p2_fuefuki_visual_reset();
                return false;
            }
            clip.frames[i] = item.frames[i];
        }
        clip.poseCount = item.poses;
        attachTextures(clip.poses[0]);
        ++sVisual.clipCount;
    }
    sVisual.ready = true;
    std::printf("P2_FUEFUKI_VISUAL_READY clips=%d\n", sVisual.clipCount);
    return true;
}

void pc_p2_fuefuki_visual_reset()
{
    sVisual = VisualState();
}

bool pc_p2_fuefuki_visual_ready()
{
    return sVisual.ready;
}

int pc_p2_fuefuki_visual_clip_count()
{
    return sVisual.clipCount;
}

bool pc_p2_fuefuki_visual_clip(const char* name)
{
    if (!sVisual.ready || !name) {
        return false;
    }
    for (int i = 0; i < sVisual.clipCount; ++i) {
        if (sVisual.clips[i].name == name) {
            sVisual.active = i;
            sVisual.pose = 0;
            return true;
        }
    }
    return false;
}

const char* pc_p2_fuefuki_visual_active_clip()
{
    return sVisual.active >= 0 ? sVisual.clips[sVisual.active].name.c_str() : nullptr;
}

int pc_p2_fuefuki_visual_update(float sourceFrames)
{
    if (!sVisual.ready || sVisual.active < 0 || !std::isfinite(sourceFrames) || sourceFrames < 0.0f) {
        return -1;
    }
    const ClipBank& clip = sVisual.clips[sVisual.active];
    sVisual.pose += static_cast<int>(sourceFrames);
    if (clip.duration > 0) {
        sVisual.pose %= clip.duration;
    }
    return pc_p2_fuefuki_visual_pose_index();
}

int pc_p2_fuefuki_visual_pose_index()
{
    if (sVisual.active < 0) {
        return -1;
    }
    const ClipBank& clip = sVisual.clips[sVisual.active];
    int index = 0;
    for (int i = 0; i < clip.poseCount; ++i) {
        if (clip.frames[i] <= sVisual.pose) {
            index = i;
        }
    }
    return index;
}

bool pc_p2_fuefuki_visual_drew()
{
    return sVisual.drew;
}

void pc_p2_fuefuki_visual_draw(Graphics& gfx, const Matrix4f& ownerWorld)
{
    if (!sVisual.ready || sVisual.active < 0 || !gfx.mCamera) {
        return;
    }
    gfx.setPerspective(gfx.mCamera->mPerspectiveMatrix.mMtx, gfx.mCamera->mFov,
                       gfx.mCamera->mAspectRatio, gfx.mCamera->mNear, gfx.mCamera->mFar, 1.0f);
    const int pose = pc_p2_fuefuki_visual_pose_index();
    Matrix4f view;
    gfx.mCamera->mLookAtMtx.multiplyTo(ownerWorld, view);
    Shape* shape = sVisual.clips[sVisual.active].poses[pose];
    shape->updateAnim(gfx, view, nullptr, nullptr);
    shape->drawshape(gfx, *gfx.mCamera, nullptr);
    if (!sVisual.drew) {
        std::printf("P2_FUEFUKI_VISUAL_DRAW clip=%s pose=%d\n",
                    pc_p2_fuefuki_visual_active_clip(), pose);
        sVisual.drew = true;
    }
}
