#include "pc_p2_placement_probe.h"

#include "pc_bbft.h"
#include "teki.h"
#include "MapMgr.h"
#include "MapCode.h"
#include "Route.h"
#include "Creature.h"
#include "Generator.h"

#include <cmath>
#include <cstdio>

namespace {
// Mirror the family adapters' bound: reject NaN/Inf and absurd coordinates
// before touching the map, so a malformed spawn never reads out of bounds.
bool bounded(float x, float y, float z)
{
    constexpr float kBound = 100000.0f;
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z)
        && std::fabs(x) <= kBound && std::fabs(y) <= kBound && std::fabs(z) <= kBound;
}

// Carry-route coverage radius. A spawn is "on the route graph" only when its
// nearest OPEN waypoint lies within this XZ distance. This mirrors engine
// reach conventions (aiRescue.cpp thresholds against the waypoint radius;
// navi.cpp:761 uses 200.0f as its nearest-actor bound) and admits every point
// inside a corridor segment of the P2 room graph (worst half-segment ~183
// units) while rejecting points off the graph.
constexpr float kRouteCoverageRadius = 200.0f;
}

void pc_p2_placement_probe_run()
{
    if (!pc_pikipelago_room_preview()) return; // this graph is only the P2 room
    if (!mapMgr || !routeMgr || !tekiMgr) return;

    int actors = 0;
    int evidenceSlots = 0;
    const u32 kRouteHandle = 'test'; // every gameplay caller uses the shared handle

    Iterator it(tekiMgr);
    CI_LOOP(it) {
        Teki* teki = static_cast<Teki*>(*it);
        if (!teki) continue;
        const Vector3f& pos = teki->getPosition();
        if (!bounded(pos.x, pos.y, pos.z)) continue;

        // (a) XYZ evidence: a ground triangle must exist beneath the spawn, so
        // we never admit a position the map does not actually cover.
        CollTriInfo* tri = mapMgr->getCurrTri(pos.x, pos.z, true);
        const bool hasTerrain = tri != nullptr;

        // (b) Terrain class: 'none' when no triangle, otherwise 'water' vs
        // 'ground' by map-code attribute. A missing triangle must never read
        // back as 'ground'.
        const int attribute = tri ? MapCode::getAttribute(tri) : ATTR_NULL;
        const char* terrain = hasTerrain ? (attribute == ATTR_Water ? "water" : "ground") : "none";
        const float groundY = hasTerrain ? mapMgr->getMinY(pos.x, pos.z, true) : 0.0f;
        const float depth = (hasTerrain && attribute == ATTR_Water && std::isfinite(groundY))
                                ? (pos.y - groundY) : 0.0f;

        // (c) Route evidence: the spawn must sit within the route graph's
        // coverage radius so a corpse corridor exists. A nearest-waypoint miss,
        // or a waypoint farther than the cap, means the spawn is off the carry
        // graph and the slot is not carryable.
        WayPoint* waypoint = routeMgr->findNearestWayPoint(kRouteHandle, pos, false);
        bool route = waypoint != nullptr;
        float routeDistance = -1.0f;
        if (waypoint) {
            const float dx = waypoint->mPosition.x - pos.x;
            const float dz = waypoint->mPosition.z - pos.z;
            routeDistance = std::sqrt(dx * dx + dz * dz);
            route = routeDistance <= kRouteCoverageRadius;
        }

        // The id here is the generator's 4-byte file id (_70), NOT a placement
        // catalog slot uid.
        const unsigned generator = teki->mGenerator ? teki->mGenerator->_70 : 0;
        std::printf("P2_PLACEMENT_SLOT generator=%u actor=%d xyz=%d terrain=%s route=%d route_distance=%.1f water_depth=%.2f\n",
                    generator, static_cast<int>(teki->mTekiType),
                    hasTerrain ? 1 : 0, terrain, route ? 1 : 0,
                    static_cast<double>(routeDistance), static_cast<double>(depth));
        ++actors;
        if (hasTerrain && route) ++evidenceSlots;
    }
    std::printf("P2_PLACEMENT_PROBE actors=%d evidence_slots=%d\n", actors, evidenceSlots);
    std::fflush(stdout);
}
