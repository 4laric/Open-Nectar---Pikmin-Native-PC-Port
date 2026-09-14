// Opt-in lane-23 real-engine Candypop binding (#448, family #171).
//
// See pc_p2_candypop.h. The module owns only the source identity/policy glue
// and the own-colour-aware conversion; the actor, swallow, close, discharge and
// sprout creation are the genuine engine `PomAi` behaviour. Strict sidecar
// file `p2-pom-engine.txt` (cwd, `P2_POM_1` rows, reusing the lane-23 Candypop
// parser), fail-closed:
//
//   P2_POM_ENGINE_1 <count>
//   <generator-u32> <BluePom|RedPom|YellowPom|BlackPom|WhitePom|RandPom|Pom> <x> <y> <z>
//
// BlackPom/WhitePom are reported unsupported here (the violet/ivory providers
// own them); the base Pom is rejected and never bound. Without the file the
// module is inert.
#include "pc_p2_candypop.h"
#include "pc_p2_pom_policy.h"
#include "pc_bbft.h"
#include "Boss.h"
#include "Generator.h"
#include "ItemMgr.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "ObjType.h"
#include "Piki.h"
#include "PikiAI.h"
#include "PikiHeadItem.h"
#include "PikiState.h"
#include "Pom.h"
#include "Stickers.h"
#include "Vector.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>

namespace {

struct EngineSpec {
	std::uint32_t generator = 0;
	p2pom::Species species   = p2pom::Species::RedPom;
	int colour               = 1;
	float x                  = 0.0f;
	float y                  = 0.0f;
	float z                  = 0.0f;
};

std::vector<EngineSpec> specs;
bool specsLoaded = false;
std::vector<std::uint32_t> applied;

[[noreturn]] void fail()
{
	std::fputs("P2_CANDYPOP invalid sidecar\n", stderr);
	std::abort();
}

bool rgbSpecies(p2pom::Species species)
{
	return species == p2pom::Species::BluePom || species == p2pom::Species::RedPom
	    || species == p2pom::Species::YellowPom;
}

void loadSpecs()
{
	if (specsLoaded) {
		return;
	}
	specsLoaded = true;
	if (!pc_pikipelago_room_preview()) {
		return;
	}
	std::ifstream in("p2-pom-engine.txt");
	if (!in) {
		return; // inert without the sidecar
	}
	std::vector<p2pom::PomSpec> rows;
	try {
		rows = p2pom::readPoms(in);
	} catch (const std::exception&) {
		fail();
	}
	for (const p2pom::PomSpec& row : rows) {
		if (p2pom::isBase(row.species)) {
			std::printf("P2_POM_BASE_REJECTED generator=%u species=Pom source_id=82 reason=nonspawnable_base\n",
			            row.generator);
			continue;
		}
		if (!rgbSpecies(row.species)) {
			std::printf("P2_CANDYPOP_UNSUPPORTED generator=%u species=%s reason=not_p1_colour\n", row.generator,
			            p2pom::speciesName(row.species));
			continue;
		}
		EngineSpec spec;
		spec.generator = row.generator;
		spec.species   = row.species;
		spec.colour    = p2pom::speciesColour(row.species);
		spec.x         = row.x;
		spec.y         = row.y;
		spec.z         = row.z;
		specs.push_back(spec);
	}
}

const EngineSpec* match(const Pom* pom)
{
	loadSpecs();
	if (specs.empty() || !pom) {
		return nullptr;
	}
	if (pom->mGenerator) {
		const unsigned generator = static_cast<unsigned>(pom->mGenerator->_70);
		for (const EngineSpec& spec : specs) {
			if (spec.generator == generator) {
				return &spec;
			}
		}
	}
	// At Pom init the generator link is assigned only after birth, so fall back
	// to the birth position the sidecar declared.
	for (const EngineSpec& spec : specs) {
		const float dx = pom->mSRT.t.x - spec.x;
		const float dz = pom->mSRT.t.z - spec.z;
		if (dx * dx + dz * dz <= 1.0f) {
			return &spec;
		}
	}
	return nullptr;
}

bool isApplied(std::uint32_t generator)
{
	for (std::uint32_t value : applied) {
		if (value == generator) {
			return true;
		}
	}
	return false;
}

const char* colourName(int colour)
{
	switch (colour) {
	case 0:
		return "blue";
	case 1:
		return "red";
	case 2:
		return "yellow";
	default:
		return "unknown";
	}
}

} // namespace

void pc_p2_candypop_reset()
{
	specsLoaded = false;
	specs.clear();
	applied.clear();
}

void pc_p2_candypop_setup()
{
	loadSpecs();
	if (!specs.empty()) {
		std::printf("P2_CANDYPOP_SIDECAR engine_buds=%u\n", unsigned(specs.size()));
	}
	std::fflush(stdout);
}

void pc_p2_candypop_tick()
{
	loadSpecs();
	if (specs.empty() || !bossMgr) {
		return;
	}
	Iterator it(bossMgr);
	CI_LOOP(it)
	{
		Boss* boss = static_cast<Boss*>(*it);
		if (!boss || !boss->isAlive() || boss->mObjType != OBJTYPE_Pom) {
			continue;
		}
		const EngineSpec* spec = match(static_cast<Pom*>(boss));
		if (!spec || isApplied(spec->generator)) {
			continue;
		}
		applied.push_back(spec->generator);
		// The staged boss generator carries a valid P1 container colour so the
		// engine birth gate passes; stamp the source identity here.
		static_cast<Pom*>(boss)->setColor(spec->colour);
		std::printf(
		    "P2_POM_READY generator=%u species=%s source_id=%d colour=%d budget=%d queen=%d engine=1 x=%.2f y=%.2f z=%.2f\n",
		    spec->generator, p2pom::speciesName(spec->species), p2pom::speciesId(spec->species), spec->colour,
		    p2pom::budget(spec->species), int(p2pom::queen(spec->species)), spec->x, spec->y, spec->z);
		std::printf("P2_POM_INVULNERABLE generator=%u invulnerable_after_landing=1\n", spec->generator);
	}
	std::fflush(stdout);
}

int pc_p2_candypop_budget(const Pom* pom)
{
	const EngineSpec* spec = match(pom);
	return spec ? p2pom::budget(spec->species) : 0;
}

int pc_p2_convert_candypop(Pom* pom, int remaining)
{
	const EngineSpec* spec = match(pom);
	if (!spec) {
		return -1;
	}
	if (remaining < 0) {
		remaining = 0;
	}
	Stickers stickers(pom);
	Iterator it(&stickers);
	int converted = 0, used = 0, refunds = 0;
	CI_LOOP(it)
	{
		Creature* creature = *it;
		if (!creature || !creature->isAlive() || !creature->isPiki()) {
			continue;
		}
		Piki* piki      = static_cast<Piki*>(creature);
		const int input = int(piki->mColor);
		const bool sameColour = input == spec->colour;
		if (!sameColour && used >= remaining) {
			// Budget exhausted: release the input rather than consuming it.
			piki->endStickObject();
			piki->mFSM->transit(piki, PIKISTATE_Normal);
			piki->changeMode(PikiMode::FreeMode, naviMgr->getNavi());
			it.dec();
			continue;
		}
		PikiHeadItem* sprout = static_cast<PikiHeadItem*>(itemMgr->birth(OBJTYPE_Pikihead));
		if (!sprout) {
			// Capacity failure must never eat an input.
			piki->endStickObject();
			piki->mFSM->transit(piki, PIKISTATE_Normal);
			piki->changeMode(PikiMode::FreeMode, naviMgr->getNavi());
			it.dec();
			continue;
		}
		Vector3f position = pom->mSRT.t;
		position.y += 50.0f;
		sprout->init(position);
		sprout->setColor(spec->colour);
		const float angle = float(converted) * 1.256637f;
		sprout->mVelocity.set(p2pom::LaunchHoriz * std::sin(angle), p2pom::LaunchVert, p2pom::LaunchHoriz * std::cos(angle));
		sprout->startAI(0);
		C_SAI(sprout)->start(sprout, PikiHeadAI::PIKIHEAD_Flying);
		piki->setEraseKill();
		piki->kill(false);
		it.dec();
		++converted;
		if (sameColour) {
			++refunds;
		} else {
			++used;
		}
		std::printf("P2_CANDYPOP_WITNESS generator=%u species=%s input=%s refund=%d\n", spec->generator,
		            p2pom::speciesName(spec->species), colourName(input), int(sameColour));
	}
	std::printf("P2_CANDYPOP_CONVERT generator=%u species=%s converted=%d used=%d refunds=%d\n", spec->generator,
	            p2pom::speciesName(spec->species), converted, used, refunds);
	std::fflush(stdout);
	return used;
}
