#include "pc_p2_chappy.h"
#include "pc_p2_chappy_policy.h"
#include "pc_p2_chappy_fsm.h"
#include "pc_p2_campaign_actor.h"
#include "pc_p2_enemy.h"
#include "pc_p2_kochappy.h"
#include "pc_p2_dwarf_orange.h"
#include "pc_p2_sheargrub.h"
#include "pc_p2_sokkuri.h"
#include "pc_p2_setup_failsafe.h"
#include "pc_randomizer.h"
#include "teki.h"
#include "Generator.h"
#include "Shape.h"
#include "Texture.h"
#include "Material.h"
#include "gameflow.h"
#include "Graphics.h"
#include "Camera.h"
#include "PaniAnimator.h"
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>

namespace {
struct ClipDef {
    int frames = 0;
    int poses = 0;
};
struct SpeciesBank {
    unsigned source = 0;
    std::string species;
    std::map<std::string, ClipDef> clips;
};
std::map<std::string, SpeciesBank> banks; // enum -> bank (from p2-chappy-bank.txt)
std::map<std::string, std::vector<Shape*>> shapes; // "Enum|clip" -> poses
std::map<PelletView*, const p2chappy::SpeciesParams*> actors; // view -> species
std::map<PelletView*, float> lastHealth;
std::map<PelletView*, std::string> lastClip;
std::map<PelletView*, unsigned> corpseGenerators; // delivered-corpse lookup (preview path)
p2chappy::Health health;
bool bankLoaded = false;
bool loggedDraw[2] = {false, false};
size_t bankBytes = 0;

bool claimedElsewhere(BTeki* actor)
{
    PelletView* view = static_cast<PelletView*>(actor);
    return pc_p2_enemy_name(view) || pc_p2_kochappy_name(view) || pc_p2_dwarf_orange_name(view)
           || pc_p2_sheargrub_name(view) || pc_p2_sokkuri_registered(actor);
}

const char* pickClip(const SpeciesBank& bank, int motion, bool corpse)
{
    // Fallback groups mirror the proxy/batch2 draw order (dead, attack,
    // flick, move, wait) spelled with the family banks' actual stems.
    static const char* const dead[] = {"dead", "dead1", "pdead1"};
    static const char* const attack[] = {"attack", "attack1", "attack2"};
    static const char* const flick[] = {"flick"};
    static const char* const move[] = {"move1", "move", "move2", "run1", "walk"};
    static const char* const wait[] = {"wait1", "wait", "wait2", "waitact1", "waitact2", "sleep"};
    static const char* const press[] = {"press"};
    static const char* const carry[] = {"carry"};
    const char* const* first = wait;
    if (corpse || motion == TekiMotion::Dead) {
        first = dead;
    } else if (motion == TekiMotion::Attack) {
        first = attack;
    } else if (motion == TekiMotion::Flick) {
        first = flick;
    } else if (motion == TekiMotion::Move1 || motion == TekiMotion::Move2) {
        first = move;
    } else if (motion == TekiMotion::Type1) {
        first = press;
    } else if (motion == TekiMotion::Type5) {
        first = carry;
    }
    const char* const* groups[2] = {first, wait};
    for (int g = 0; g < 2; ++g) {
        for (const char* const* name = groups[g]; *name; ++name) {
            if (bank.clips.count(*name)) {
                return bank.clips.find(*name)->first.c_str();
            }
        }
        if (corpse) break;
    }
    return bank.clips.empty() ? nullptr : bank.clips.begin()->first.c_str();
}

// Bind with an explicit seed source (generated-placement path).
bool bindActorAs(BTeki* actor, unsigned generator, unsigned sourceId)
{
    if (!actor || !generator) return false;
    const p2chappy::SpeciesParams* spec = p2chappy::speciesForSource(sourceId);
    if (!spec) return false;
    PelletView* view = static_cast<PelletView*>(actor);
    if (actors.count(view)) return true;
    if (actor->mTekiType != spec->host) return false;
    if (claimedElsewhere(actor)) return false;
    if (!health.bind(view, spec->health)) return false;
    actors[view] = spec;
    actor->mHealth = spec->health;
    lastHealth[view] = spec->health;
    pc_randomizer_p2_bind_source(view, sourceId, generator);
    std::printf("P2_CHAPPY_DELIVERY_BIND generator=%u source_id=%u key=chappy|%s\n", generator,
                sourceId, spec->enumName);
    const auto& pos = actor->getPosition();
    std::printf("P2_ENEMY_READY species=%s source_id=%u native_family=Chappy generator=%u "
                "x=%.7f y=%.7f z=%.7f health=%.1f max_health=%.1f behavior=P1 host=%d "
                "source_FSM=implemented\n",
                spec->enumName, sourceId, generator, pos.x, pos.y, pos.z, actor->mHealth,
                spec->health, spec->host);
    std::printf("P2_CHAPPY_BIND generator=%u source_id=%u species=%s visual_only=0\n", generator,
                sourceId, spec->enumName);
    std::fflush(stdout);
    return true;
}
} // namespace

void pc_p2_chappy_reset()
{
    banks.clear();
    shapes.clear();
    actors.clear();
    lastHealth.clear();
    lastClip.clear();
    corpseGenerators.clear();
    health.reset();
    bankLoaded = false;
    loggedDraw[0] = loggedDraw[1] = false;
    bankBytes = 0;
}

void pc_p2_chappy_forget(BTeki* actor)
{
    PelletView* view = static_cast<PelletView*>(actor);
    if (actors.erase(view)) {
        std::printf("P2_CHAPPY_FORGET registered=1\n");
        std::fflush(stdout);
    }
    lastHealth.erase(view);
    lastClip.erase(view);
    corpseGenerators.erase(view);
    health.forget(view);
}

float pc_p2_chappy_max_health(const BTeki* actor, float fallback)
{
    return health.life(actor, fallback);
}

const char* pc_p2_chappy_name(PelletView* actor)
{
    auto it = actors.find(actor);
    return it == actors.end() ? nullptr : it->second->english;
}

bool pc_p2_chappy_registered(const BTeki* actor)
{
    return actors.count(const_cast<BTeki*>(actor)) != 0;
}

unsigned long pc_p2_chappy_count()
{
    return (unsigned long)actors.size();
}

bool pc_p2_chappy_receipt(PelletView* view, unsigned& generator)
{
    auto it = corpseGenerators.find(view);
    if (it == corpseGenerators.end()) return false;
    generator = it->second;
    return true;
}

void pc_p2_chappy_setup()
{
    pc_p2_chappy_reset();
    std::ifstream bank("p2-chappy-bank.txt"), bindings("p2-chappy-actors.txt");
    if (!bank && !bindings) return;
    if (!bank || !bindings || !tekiMgr) std::abort();
    // Bank: P2_CHAPPY_BANK_1, species rows, clip rows.
    std::string token;
    if (!(bank >> token) || token != "P2_CHAPPY_BANK_1") std::abort();
    std::string word;
    while (bank >> word) {
        if (word == "species") {
            std::string species;
            unsigned long long id = 0;
            if (!(bank >> species >> id)) std::abort();
            const p2chappy::SpeciesParams* spec = p2chappy::speciesForEnum(species);
            if (!spec || spec->source != (unsigned)id) std::abort();
            SpeciesBank& entry = banks[species];
            entry.source = (unsigned)id;
            entry.species = species;
        } else if (word == "clip") {
            std::string species, name, events, posesWord, status;
            long long frames = 0;
            int poses = 0;
            if (!(bank >> species >> name >> frames >> events >> posesWord >> poses >> status)) std::abort();
            if (posesWord != "poses" || status != "converted") std::abort();
            if (!banks.count(species)) std::abort();
            if (frames < 1 || frames > 10000 || poses < 1 || poses > 64) std::abort();
            if (!banks[species].clips.emplace(name, ClipDef{(int)frames, poses}).second) std::abort();
        } else {
            std::abort();
        }
    }
    if (banks.empty()) std::abort();
    // Actors: P2_CHAPPY_ACTORS_1 <count> + <generator> <Species> rows.
    if (!(bindings >> word)) std::abort();
    int count = 0;
    if (word != "P2_CHAPPY_ACTORS_1" || !(bindings >> count) || count < 1 || count > 100) std::abort();
    std::map<unsigned, std::string> wanted;
    for (int i = 0; i < count; ++i) {
        unsigned long long generator = 0;
        std::string species;
        if (!(bindings >> generator >> species)) std::abort();
        if (!generator || generator > 0xffffffffULL) std::abort();
        if (!p2chappy::speciesForEnum(species) || !wanted.emplace((unsigned)generator, species).second) std::abort();
    }
    if (bindings >> word) std::abort();
    if (pc_randomizer_p2_bridge()) {
        // Campaign identity comes from the seed, not the filed placeholders.
        wanted.clear();
        for (const auto& entry : banks) {
            for (unsigned id : pc_p2_campaign_ids(entry.second.source)) wanted[id] = entry.second.species;
        }
    }
    if (wanted.empty()) return;
    // Load the staged pose bank before touching actors (fail-closed).
    for (const auto& entry : banks) {
        for (const auto& clip : entry.second.clips) {
            for (int i = 0; i < clip.second.poses; ++i) {
                char rel[192];
                std::snprintf(rel, sizeof(rel),
                              "assets/dataDir/courses/pikmin2room/ch_%s_%s_%02d.mod",
                              entry.second.species.c_str(), clip.first.c_str(), i);
                std::ifstream probe(rel, std::ios::binary | std::ios::ate);
                if (!probe) std::abort();
                const auto size = probe.tellg();
                if (size <= 0 || size_t(size) > 512 * 1024 || bankBytes + size_t(size) > 48 * 1024 * 1024) std::abort();
                bankBytes += size_t(size);
            }
        }
    }
    for (const auto& entry : banks) {
        for (const auto& clip : entry.second.clips) {
            std::vector<Shape*>& out = shapes[entry.second.species + "|" + clip.first];
            for (int i = 0; i < clip.second.poses; ++i) {
                char load[192];
                std::snprintf(load, sizeof(load), "courses/pikmin2room/ch_%s_%s_%02d.mod",
                              entry.second.species.c_str(), clip.first.c_str(), i);
                Shape* shape = gameflow.loadShape(load, true);
                if (!shape) std::abort();
                out.push_back(shape);
            }
        }
    }
    bankLoaded = true;
    // Bind the actors.
    std::set<unsigned> found;
    Iterator it(tekiMgr);
    CI_LOOP(it)
    {
        Teki* actor = static_cast<Teki*>(*it);
        if (!actor || !actor->mGenerator) continue;
        const unsigned token = pc_randomizer_p2_bridge() ? pc_p2_campaign_token(actor)
                                                         : (unsigned)actor->mGenerator->_70;
        auto match = wanted.find(token);
        if (match == wanted.end()) continue;
        const p2chappy::SpeciesParams* spec = p2chappy::speciesForEnum(match->second);
        if (!spec) std::abort();
        if (actor->mTekiType != spec->host || claimedElsewhere(actor)) {
            if (pc_p2_setup_skip(pc_randomizer_p2_bridge(), spec->enumName, "actor_identity_mismatch")) return;
        }
        // Pack generators share one campaign token across members: bind
        // every member, count the token once (proxy Finding 5 pattern).
        found.insert(token);
        PelletView* view = static_cast<PelletView*>(actor);
        if (!health.bind(view, spec->health)) std::abort();
        actors[view] = spec;
        actor->mHealth = spec->health;
        lastHealth[view] = spec->health;
        if (pc_randomizer_p2_bridge()) {
            pc_randomizer_p2_bind_source(view, spec->source, token);
            std::printf("P2_CHAPPY_DELIVERY_BIND generator=%u source_id=%u key=chappy|%s\n", token,
                        spec->source, spec->enumName);
        }
        const auto& pos = actor->getPosition();
        std::printf("P2_ENEMY_READY species=%s source_id=%u native_family=Chappy generator=%u "
                    "x=%.7f y=%.7f z=%.7f health=%.1f max_health=%.1f behavior=P1 host=%d "
                    "source_FSM=implemented\n",
                    spec->enumName, spec->source, token, pos.x, pos.y, pos.z, actor->mHealth,
                    spec->health, spec->host);
        std::printf("P2_CHAPPY_BIND generator=%u source_id=%u species=%s visual_only=0\n", token,
                    spec->source, spec->enumName);
    }
    if (found.size() != wanted.size()) {
        std::printf("P2_CHAPPY_MISSING found=%zu wanted=%zu\n", found.size(), wanted.size());
        if (pc_p2_setup_skip(true, "Chappy", "actor_roster_incomplete")) return;
    }
    std::printf("P2_CHAPPY_BANK species=%zu mod_bytes=%zu\n", banks.size(), bankBytes);
    std::fflush(stdout);
}

bool pc_p2_chappy_bind_dynamic(BTeki* actor, unsigned generatorId, unsigned sourceId)
{
    const p2chappy::SpeciesParams* spec = p2chappy::speciesForSource(sourceId);
    if (!spec || !actor || !generatorId) return false;
    PelletView* view = static_cast<PelletView*>(actor);
    if (actors.count(view)) return true;
    // Only staged species bind: the pose bank arrives with the stage setup,
    // so an unstaged family member refuses here and keeps its P1/proxy path.
    if (!bankLoaded || !banks.count(spec->enumName)) return false;
    if (actor->mTekiType != spec->host) return false;
    if (claimedElsewhere(actor)) return false;
    // The pose bank arrives with the stage setup; the identity binds now so
    // damage/death/delivery track from spawn even before the first draw.
    if (!health.bind(view, spec->health)) return false;
    actors[view] = spec;
    actor->mHealth = spec->health;
    lastHealth[view] = spec->health;
    pc_randomizer_p2_bind_source(view, sourceId, generatorId);
    std::printf("P2_CHAPPY_DELIVERY_BIND generator=%u source_id=%u key=chappy|%s\n", generatorId,
                sourceId, spec->enumName);
    const auto& pos = actor->getPosition();
    std::printf("P2_ENEMY_READY species=%s source_id=%u native_family=Chappy generator=%u "
                "x=%.7f y=%.7f z=%.7f health=%.1f max_health=%.1f behavior=P1 host=%d "
                "source_FSM=implemented\n",
                spec->enumName, sourceId, generatorId, pos.x, pos.y, pos.z, actor->mHealth,
                spec->health, spec->host);
    std::printf("P2_CHAPPY_BIND generator=%u source_id=%u species=%s visual_only=0\n", generatorId,
                sourceId, spec->enumName);
    std::fflush(stdout);
    return true;
}

void pc_p2_chappy_update(BTeki* actor)
{
    if (!actor) return;
    PelletView* view = static_cast<PelletView*>(actor);
    auto it = actors.find(view);
    if (it == actors.end()) return;
    const p2chappy::SpeciesParams* spec = it->second;
    const unsigned generator = actor->mGenerator ? pc_p2_campaign_token(actor) : 0;
    const float hp = actor->mHealth;
    auto last = lastHealth.find(view);
    if (last != lastHealth.end() && hp < last->second - 0.0001f && hp > 0.0f) {
        std::printf("P2_CHAPPY_DAMAGE generator=%u source_id=%u health=%.1f\n", generator,
                    spec->source, hp);
        std::fflush(stdout);
    }
    lastHealth[view] = hp;
    const bool dead = (hp <= 0.0f || actor->mDeadState != 0);
    if (dead && health.markDead(view)) {
        std::printf("P2_CHAPPY_DEAD generator=%u source_id=%u\n", generator, spec->source);
        std::printf("P2_CHAPPY_CORPSE_READY generator=%u source_id=%u\n", generator, spec->source);
        if (generator) corpseGenerators[view] = generator;
        std::fflush(stdout);
    }
}

bool pc_p2_chappy_draw(BTeki* actor, Graphics& gfx, const Matrix4f& matrix, bool corpse)
{
    if (!actor || !bankLoaded) return false;
    PelletView* view = static_cast<PelletView*>(actor);
    auto it = actors.find(view);
    if (it == actors.end()) return false;
    const p2chappy::SpeciesParams* spec = it->second;
    auto bank = banks.find(spec->enumName);
    if (bank == banks.end() || bank->second.clips.empty()) return false;
    const bool dead = corpse || actor->mHealth <= 0.0f || actor->mDeadState != 0;
    const int motion = actor->mTekiAnimator ? actor->mTekiAnimator->getCurrentMotionIndex() : 2;
    const char* clip = pickClip(bank->second, motion, dead);
    if (!clip) return false;
    auto shapesIt = shapes.find(std::string(spec->enumName) + "|" + clip);
    if (shapesIt == shapes.end() || shapesIt->second.empty()) return false;
    int frames = 1;
    float phase = 0.0f;
    if (actor->mTekiAnimator) {
        frames = actor->mTekiAnimator->getFrameCount();
        if (frames > 1) {
            phase = actor->mTekiAnimator->getCounter() / float(frames - 1);
            if (!(phase >= 0.0f && phase <= 1.0f)) phase = 0.0f;
        }
    }
    size_t index = 0;
    if (!dead && !shapesIt->second.empty()) {
        index = size_t(phase * float(shapesIt->second.size() - 1) + 0.5f);
        if (index >= shapesIt->second.size()) index = shapesIt->second.size() - 1;
    } else {
        index = shapesIt->second.size() - 1;
    }
    Shape* shape = shapesIt->second[index];
    if (!loggedDraw[corpse ? 1 : 0]) {
        const unsigned generator = actor->mGenerator ? pc_p2_campaign_token(actor) : 0;
        std::printf("P2_CHAPPY_DRAW corpse=%d species=%s generator=%u clip=%s\n", int(corpse),
                    spec->enumName, generator, clip);
        loggedDraw[corpse ? 1 : 0] = true;
        std::fflush(stdout);
    } else {
        auto known = lastClip.find(view);
        if (known == lastClip.end() || known->second != clip) {
            lastClip[view] = clip;
            const unsigned generator = actor->mGenerator ? pc_p2_campaign_token(actor) : 0;
            std::printf("P2_CHAPPY_STATE generator=%u source_id=%u clip=%s\n", generator,
                        spec->source, clip);
            std::fflush(stdout);
        }
    }
    shape->updateAnim(gfx, matrix, nullptr, actor);
    shape->drawshape(gfx, *gfx.mCamera, nullptr);
    return true;
}
