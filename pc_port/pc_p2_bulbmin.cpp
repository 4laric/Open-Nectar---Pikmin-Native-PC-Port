#include "pc_p2_bulbmin.h"
#include "pc_p2_species.h"
#include "pc_p2_preview.h"
#include "pc_bbft.h"
#include "Piki.h"
#include "PikiMgr.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "MapMgr.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

namespace {
constexpr float kPi = 3.14159265358979323846f;

P2BulbminBridge bridge;
bool active = false;
std::unordered_map<Piki*, std::uint32_t> idOfPiki;
std::unordered_map<std::uint32_t, Piki*> pikiOfId;
std::uint32_t nextId = 1;

std::uint32_t idFor(Piki* piki) {
    auto found = idOfPiki.find(piki);
    if (found != idOfPiki.end()) return found->second;
    const std::uint32_t id = nextId++;
    idOfPiki[piki] = id;
    pikiOfId[id] = piki;
    return id;
}
} // namespace

bool pc_p2_bulbmin_active() { return active; }

void pc_p2_bulbmin_reset() {
    bridge.reset();
    idOfPiki.clear();
    pikiOfId.clear();
    nextId = 1;
    active = false;
}

void pc_p2_bulbmin_setup() {
    pc_p2_bulbmin_reset();
    if (!pc_pikipelago_room_preview()) return;
    std::string body;
    if (const char* env = std::getenv("PIKMIN_P2_BULBMIN")) {
        if (env[0] != '\0') {
            body = env;
            // A bare positive epoch is accepted as shorthand.
            if (body.rfind("P2_BULBMIN_1", 0) != 0)
                body = "P2_BULBMIN_1 " + body + " "
                     + std::to_string(P2BULBMIN_MAX_DEPENDENTS);
        }
    }
    if (body.empty()) {
        std::ifstream file("p2-bulbmin.txt");
        if (!file.is_open()) return; // no config: inert no-op
        std::ostringstream stream;
        stream << file.rdbuf();
        if (file.bad()) {
            std::fprintf(stderr, "Cannot read P2 Bulbmin config\n");
            std::abort();
        }
        body = stream.str();
    }
    std::istringstream in(body);
    P2BulbminConfig config;
    if (!p2_bulbmin_read(in, config) || !bridge.setup(config, nullptr)) {
        std::fprintf(stderr, "Invalid P2 Bulbmin config\n");
        std::abort();
    }
    active = true;
    std::printf("P2_BULBMIN_READY mother_epoch=%llu dependents=%d behavior=policy_ledger live_spawn=unregistered\n",
                static_cast<unsigned long long>(config.motherEpoch), config.maxDependents);
    std::fflush(stdout);
}

bool pc_p2_bulbmin_birth(Piki* bulbmin) {
    if (!active || !bulbmin) return false;
    const P2BulbminCommand command = bridge.birth(idFor(bulbmin));
    if (!command.accepted) return false;
    if (!pc_p2_is_bulbmin(bulbmin)) pc_p2_make_bulbmin(bulbmin);
    return true;
}

Piki* pc_p2_bulbmin_birth_dependent(Creature* leader, const Vector3f& motherPos,
                                    float faceDir, int index) {
    if (!active || !pikiMgr || !naviMgr || !leader
        || index < 0 || index >= P2BULBMIN_MAX_DEPENDENTS)
        return nullptr;
    Creature* born = pikiMgr->birth();
    if (!born || !born->isPiki()) return nullptr;
    Piki* bulbmin = static_cast<Piki*>(born);
    bulbmin->init(naviMgr->getNavi());
    pc_p2_make_bulbmin(bulbmin);
    bulbmin->mLeaderCreature = leader;
    const float angle = faceDir + kPi;
    const float modifier = 2.5f * index + 17.5f;
    Vector3f position(motherPos.x + modifier * std::sin(angle), motherPos.y,
                      motherPos.z + modifier * std::cos(angle));
    if (mapMgr) position.y = mapMgr->getMinY(position.x, position.z, true);
    bulbmin->inputPosition(position);
    if (!pc_p2_bulbmin_birth(bulbmin)) {
        bulbmin->setEraseKill();
        bulbmin->kill(false);
        return nullptr;
    }
    return bulbmin;
}

bool pc_p2_bulbmin_whistle(Piki* bulbmin) {
    if (!active || !bulbmin) return false;
    const auto found = idOfPiki.find(bulbmin);
    if (found == idOfPiki.end()) return false;
    const P2BulbminCommand command = bridge.whistle(found->second, P2CaptainInvalid);
    if (!command.accepted) return false;
    bulbmin->mLeaderCreature = nullptr;
    if (naviMgr) {
        if (Navi* navi = naviMgr->getNavi()) bulbmin->mNavi = navi;
    }
    return true;
}

void pc_p2_bulbmin_forget(Piki* piki) {
    if (!active || !piki) return;
    const auto found = idOfPiki.find(piki);
    if (found == idOfPiki.end()) return;
    bridge.forget(found->second);
    pikiOfId.erase(found->second);
    idOfPiki.erase(found);
}

std::vector<std::uint32_t> pc_p2_bulbmin_transition(P2BulbminCaveTransition move) {
    const P2BulbminTransitionOut result = bridge.transition(move);
    for (const std::uint32_t id : result.removed) {
        auto found = pikiOfId.find(id);
        if (found != pikiOfId.end()) {
            idOfPiki.erase(found->second);
            pikiOfId.erase(found);
        }
    }
    return result.removed;
}

void pc_p2_bulbmin_bind_captain_table(P2CaptainOwnershipTable* ownership) {
    bridge.bindCaptains(ownership);
}
