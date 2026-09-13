#include "pc_p2_bombsarai_arena.h"

#include "Camera.h"
#include "Graphics.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kCoordinateLimit = 100000.0f;
constexpr float kParameterLimit = 10000.0f;
constexpr int kMaxReceivers = 8;

struct ArenaState {
    P2BombSaraiHover hover;
    P2BombSaraiHoverParms hoverParms;
    P2BombSaraiBombConfig bombConfig;
    P2BombSaraiBombPool pool{ 2 }; // mChildNum preallocation (enemyInfo.cpp:46)
    P2BombSaraiBomb* held = nullptr;
    P2BombSaraiVec3 carrier;
    P2BombSaraiVec3 joint; // pinned capture joint (kamu_jnt1 stand-in, #128 pending)
    float carrierYaw = 0.0f;
    float carrierY = 0.0f; // current hover height, integrated by the harness
    std::uint64_t carrierToken = 0;
    P2BombSaraiReceiver receivers[kMaxReceivers];
    int receiverCount = 0;
    P2BombSaraiRoutedHit hits[kMaxReceivers];
    int hitCount = 0;
    P2BombSaraiVec3 lastBlastCenter;
    bool blastMarker = false;
    bool blastFired = false;
    bool ready = false;
    bool drew = false;
};

ArenaState sArena;

bool finite(float value) { return std::isfinite(value); }
bool finite(P2BombSaraiVec3 value)
{
    return finite(value.x) && finite(value.y) && finite(value.z);
}
bool bounded(P2BombSaraiVec3 value)
{
    return finite(value) && std::fabs(value.x) <= kCoordinateLimit
        && std::fabs(value.y) <= kCoordinateLimit && std::fabs(value.z) <= kCoordinateLimit;
}
bool paramBound(float value, float minimum, float maximum)
{
    return finite(value) && value >= minimum && value <= maximum;
}
bool exhausted(std::istringstream& line)
{
    std::string extra;
    return !(line >> extra);
}
bool nextLine(std::ifstream& input, std::string& line)
{
    if (!std::getline(input, line)) return false;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return true;
}

bool parseProfileFull(const char* profilePath, ArenaState& parsed)
{
    if (!profilePath || !*profilePath) return false;
    std::ifstream input(profilePath);
    if (!input) return false;
    input.seekg(0, std::ios::end);
    if (input.tellg() < 0 || input.tellg() > 4096) return false;
    input.seekg(0);

    std::string line;
    if (!nextLine(input, line) || line != "P2_BOMBSARAI_ARENA_1") return false;

    {
        if (!nextLine(input, line)) return false;
        std::istringstream values(line);
        std::string key;
        if (!(values >> key) || key != "carrier") return false;
        if (!(values >> parsed.carrier.x >> parsed.carrier.y >> parsed.carrier.z
              >> parsed.carrierYaw >> parsed.carrierToken) || !exhausted(values)) return false;
    }
    {
        if (!nextLine(input, line)) return false;
        std::istringstream values(line);
        std::string key;
        if (!(values >> key) || key != "joint") return false;
        if (!(values >> parsed.joint.x >> parsed.joint.y >> parsed.joint.z) || !exhausted(values)) {
            return false;
        }
    }
    {
        // hover <flightHeight> <pitchRate> <pitchAmp> <freeRise> <ladenRise>
        if (!nextLine(input, line)) return false;
        std::istringstream values(line);
        std::string key;
        if (!(values >> key) || key != "hover") return false;
        if (!(values >> parsed.hoverParms.flightHeight >> parsed.hoverParms.pitchRate
              >> parsed.hoverParms.pitchAmp >> parsed.hoverParms.freeRiseFactor
              >> parsed.hoverParms.ladenRiseFactor) || !exhausted(values)) return false;
    }
    {
        // bomb <gravityPerTick> <fuseHealth> <armLoopTicks> <bombRadius>
        //      <blastRadius> <blastHalfHeight> <tekiDamage> <naviPikiDamage>
        if (!nextLine(input, line)) return false;
        std::istringstream values(line);
        std::string key;
        if (!(values >> key) || key != "bomb") return false;
        if (!(values >> parsed.bombConfig.gravityPerTick >> parsed.bombConfig.fuseHealth
              >> parsed.bombConfig.armLoopTicks >> parsed.bombConfig.bombRadius
              >> parsed.bombConfig.blastRadius >> parsed.bombConfig.blastHalfHeight
              >> parsed.bombConfig.tekiDamage >> parsed.bombConfig.naviPikiDamage)
            || !exhausted(values)) return false;
    }
    {
        // receivers <count>, then count lines: receiver <id> <kind> <x> <y> <z> [airborneimmune]
        if (!nextLine(input, line)) return false;
        std::istringstream values(line);
        std::string key;
        if (!(values >> key >> parsed.receiverCount) || key != "receivers" || !exhausted(values)
            || parsed.receiverCount < 0 || parsed.receiverCount > kMaxReceivers) return false;
        for (int i = 0; i < parsed.receiverCount; ++i) {
            if (!nextLine(input, line)) return false;
            std::istringstream entry(line);
            std::string entryKey, kind;
            P2BombSaraiReceiver receiver;
            int immune = 0, grounded = 1;
            if (!(entry >> entryKey) || entryKey != "receiver") return false;
            if (!(entry >> receiver.id >> kind >> receiver.position.x >> receiver.position.y
                  >> receiver.position.z >> grounded >> immune) || !exhausted(entry)) return false;
            if (kind == "teki") receiver.kind = P2BombSaraiReceiverKind::Teki;
            else if (kind == "navi") receiver.kind = P2BombSaraiReceiverKind::Navi;
            else if (kind == "piki") receiver.kind = P2BombSaraiReceiverKind::Piki;
            else return false;
            receiver.grounded = grounded != 0;
            receiver.airborneBombImmune = immune != 0;
            receiver.alive = true;
            if (!bounded(receiver.position)) return false;
            parsed.receivers[i] = receiver;
        }
    }
    if (nextLine(input, line)) return false; // trailing content rejected

    return bounded(parsed.carrier) && bounded(parsed.joint) && finite(parsed.carrierYaw)
        && std::fabs(parsed.carrierYaw) <= 2.0f * kPi
        && paramBound(parsed.hoverParms.flightHeight, 0.0f, kParameterLimit)
        && paramBound(parsed.hoverParms.pitchRate, 0.0f, kParameterLimit)
        && paramBound(parsed.hoverParms.pitchAmp, 0.0f, kParameterLimit)
        && paramBound(parsed.hoverParms.freeRiseFactor, 0.0f, kParameterLimit)
        && paramBound(parsed.hoverParms.ladenRiseFactor, 0.0f, kParameterLimit)
        && paramBound(parsed.bombConfig.gravityPerTick, 0.0f, kParameterLimit)
        && paramBound(parsed.bombConfig.fuseHealth, 0.0001f, kParameterLimit)
        && parsed.bombConfig.armLoopTicks > 0 && parsed.bombConfig.armLoopTicks <= 10000
        && paramBound(parsed.bombConfig.bombRadius, 0.0001f, kParameterLimit)
        && paramBound(parsed.bombConfig.blastRadius, 0.0001f, kParameterLimit)
        && paramBound(parsed.bombConfig.blastHalfHeight, 0.0f, kParameterLimit)
        && paramBound(parsed.bombConfig.tekiDamage, 0.0f, kParameterLimit)
        && paramBound(parsed.bombConfig.naviPikiDamage, 0.0f, kParameterLimit);
}

struct TraceBridge {
    P2BombSaraiTraceFn trace = nullptr;
    void* context = nullptr;
    bool failed = false;
};

bool requiredTrace(void* context, const P2BombSaraiVec3& position, const P2BombSaraiVec3& velocity,
                   float delta, float radius, P2BombSaraiTraceResult& result)
{
    TraceBridge& bridge = *static_cast<TraceBridge*>(context);
    if (!bridge.trace(bridge.context, position, velocity, delta, radius, result)) {
        bridge.failed = true;
        return false;
    }
    return true;
}
}

void pc_p2_bombsarai_arena_reset()
{
    const P2BombSaraiBombConfig config = sArena.bombConfig;
    const P2BombSaraiHoverParms hover = sArena.hoverParms;
    sArena = ArenaState{};
    sArena.bombConfig = config;
    sArena.hoverParms = hover;
}

bool pc_p2_bombsarai_arena_setup(const char* profilePath)
{
    pc_p2_bombsarai_arena_reset();
    ArenaState parsed;
    if (!parseProfileFull(profilePath, parsed)) {
        std::fputs("P2_BOMBSARAI_ARENA invalid profile\n", stderr);
        return false;
    }
    parsed.carrierY = parsed.carrier.y;
    parsed.hover.reset(parsed.hoverParms);
    parsed.ready = true;
    sArena = parsed;
    std::printf("P2_BOMBSARAI_ARENA_READY pinned=1 no_ai=1 no_fsm=1 no_visual_assets=1 "
                "receivers=%d pool=%d\n", sArena.receiverCount, sArena.pool.capacity());
    return true;
}

bool pc_p2_bombsarai_arena_update(float sourceDelta, bool supplyEvent,
                                  P2BombSaraiThrowKind throwKind, bool throwEvent,
                                  P2BombSaraiTraceFn trace, void* traceContext,
                                  P2BombSaraiCarrierFn carrier, void* carrierContext)
{
    if (!sArena.ready || !trace || !finite(sourceDelta)
        || std::fabs(sourceDelta - P2BombSaraiBomb::kSourceDelta) > 0.000001f) return false;

    TraceBridge bridge{ trace, traceContext };
    // Hover vertical control samples terrain through the same adapter the
    // trace callback wraps; the harness owns horizontal stillness.
    P2BombSaraiTerrainAdapter* adapter = static_cast<P2BombSaraiTerrainAdapter*>(traceContext);
    float velocityY = 0.0f, heightAboveGround = 0.0f;
    if (!sArena.hover.update(false, 0, { sArena.carrier.x, sArena.carrierY, sArena.carrier.z },
                             sourceDelta, adapter->mGetMinY, adapter->mGetMinYContext,
                             velocityY, heightAboveGround)) {
        return false;
    }
    sArena.carrierY += velocityY * sourceDelta;
    if (!finite(sArena.carrierY)) return false;

    // Explicit harness events stand in for the source FSM's Supply/Release.
    if (supplyEvent && !sArena.held) {
        sArena.held = sArena.pool.supply(sArena.carrierToken, sArena.joint, sArena.bombConfig);
        // Exhaustion is tolerated silently (source behavior); the arena
        // simply has no payload.
    }
    if (throwEvent && sArena.held) {
        sArena.held->throwBomb(throwKind, sArena.carrierYaw);
    }
    if (sArena.held) {
        P2BombSaraiBomb* held = sArena.held;
        held->update(sourceDelta, requiredTrace, &bridge, carrier, carrierContext);
        if (bridge.failed) return false;
        if (held->hasBlast()) {
            sArena.lastBlastCenter = held->lastBlast().center;
            sArena.hitCount = p2_bombsarai_route_blast(held->lastBlast(), sArena.receivers,
                                                       sArena.receiverCount, sArena.hits,
                                                       kMaxReceivers);
            if (sArena.hitCount < 0) return false;
            sArena.blastMarker = true;
            sArena.blastFired = true;
            held->clearBlast();
        }
        if (!sArena.held) { // unreachable, guards pool pointer reuse
            return false;
        }
        if (sArena.held->phase() == P2BombSaraiBombPhase::Despawned) {
            sArena.held = nullptr;
        }
    }
    return true;
}

bool pc_p2_bombsarai_arena_blast_fired()
{
    return sArena.blastFired;
}

int pc_p2_bombsarai_arena_blast_count()
{
    return sArena.hitCount;
}

const P2BombSaraiRoutedHit* pc_p2_bombsarai_arena_blast_hits()
{
    return sArena.hits;
}

void pc_p2_bombsarai_arena_draw(Graphics& gfx)
{
    if (!sArena.ready || !gfx.mCamera) return;
    gfx.setPerspective(gfx.mCamera->mPerspectiveMatrix.mMtx, gfx.mCamera->mFov,
                       gfx.mCamera->mAspectRatio, gfx.mCamera->mNear, gfx.mCamera->mFar, 1.0f);
    gfx.useMaterial(nullptr);
    gfx.setDepth(true);
    const Vector3f carrierPos(sArena.carrier.x, sArena.carrierY, sArena.carrier.z);
    gfx.drawSphere(carrierPos, 10.0f, gfx.mCamera->mLookAtMtx);
    if (sArena.held) {
        const P2BombSaraiVec3& p = sArena.held->position();
        const Vector3f bombPos(p.x, p.y, p.z);
        gfx.drawSphere(bombPos, sArena.bombConfig.bombRadius, gfx.mCamera->mLookAtMtx);
    }
    if (sArena.blastMarker) {
        const Vector3f blastPos(sArena.lastBlastCenter.x, sArena.lastBlastCenter.y,
                                sArena.lastBlastCenter.z);
        gfx.drawSphere(blastPos, sArena.bombConfig.blastRadius, gfx.mCamera->mLookAtMtx);
    }
    if (!sArena.drew) {
        std::puts("P2_BOMBSARAI_ARENA_DRAW debug_markers_only no_visual_assets=1");
        sArena.drew = true;
    }
}
