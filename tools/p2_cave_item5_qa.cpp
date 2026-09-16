// Lane 51 (#489, cave wave #468): standalone item-5 QA gate.
//
// Engine-free and dependency-free on purpose: this tool never includes a game
// header and never touches production. It reads one captured cave run log and
// checks the same item-5 matrix the Python checker
// (experimental/pikmin2_cave_item5_qa.py) scores:
//
//   natural_acquisition : P2_CAVE_BUD_ACTOR spawned=1 + ACCEPT + SPROUT
//                         natural=1 + GRANT natural_acquire=1 staged=0
//   real_geometry       : GEOMETRY_NODE class=real proxy=0 on the required
//                         choke/leaf ids + READY geometry=real + DRAW real
//   carry_block         : CARRY_PLAN blocking>=2 + a carrying=1 BLOCKED line,
//                         and no elec receipt before the first CARRY_OPEN
//   open_credit         : CARRY_OPEN/GEOMETRY_GATE_OPEN + exactly one new=1
//                         elec receipt, only after a natural grant
//   water_gating        : a carrying=1 BLOCKED hazard=water line
//   timeline            : fresh/yellow/full ROOMS_TIMELINE tags with the hole
//                         closed until the final tag
//
// Usage: p2_cave_item5_qa <run.log>
// Exit 0 only when every row passes; otherwise prints the failing rows and
// exits 1. Malformed marker lines are ignored, never fatal.
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool has(const std::string& line, const std::string& token)
{
    return line.find(token) != std::string::npos;
}

bool startsWith(const std::string& line, const char* prefix)
{
    const std::string pre(prefix);
    return line.compare(0, pre.size(), pre) == 0;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "usage: p2_cave_item5_qa <run.log>\n";
        return 2;
    }
    std::ifstream in(argv[1]);
    if (!in) {
        std::cerr << "cannot open " << argv[1] << "\n";
        return 2;
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) lines.push_back(line);

    bool budActor = false, accept = false, sproutNatural = false;
    bool naturalGrant = false, stagedGrant = false;
    std::string naturalSpecies;
    bool nodeChokeWater = false, nodeLeafWater = false, nodeLeafElec = false, nodeGate = false;
    bool readyReal = false, drawReal = false;
    bool planBlocking = false, carryingBlocked = false, waterCarryingBlocked = false;
    bool opened = false, creditAfterOpen = false, creditBeforeOpen = false;
    bool freshHoleClosed = false, yellowElec = false, fullHole = false;
    bool freshSeen = false, yellowSeen = false, fullSeen = false;
    int elecNew = 0;
    int openIndex = -1, firstElecNewIndex = -1;

    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& ln = lines[i];
        if (startsWith(ln, "P2_CAVE_BUD_ACTOR") && has(ln, "spawned=1")) budActor = true;
        if (startsWith(ln, "P2_CAVE_BUD_ACCEPT")) accept = true;
        if ((startsWith(ln, "P2_CAVE_BUD_SPROUT") || startsWith(ln, "P2_POM_SPROUT")) &&
            has(ln, "natural=1"))
            sproutNatural = true;
        if (startsWith(ln, "P2_CAVE_ROOMS_GRANT")) {
            if (has(ln, "natural_acquire=1") && has(ln, "staged=0")) {
                naturalGrant = true;
                const size_t pos = ln.find("species=");
                if (pos != std::string::npos) {
                    const size_t end = ln.find(' ', pos);
                    naturalSpecies = ln.substr(pos + 8, end == std::string::npos ? end : end - pos - 8);
                }
            }
        }
        if (startsWith(ln, "P2_CAVE_GEOMETRY_NODE")) {
            if (has(ln, "id=choke_water_0") && has(ln, "class=real") && has(ln, "proxy=0"))
                nodeChokeWater = true;
            if (has(ln, "id=leaf_water_0") && has(ln, "class=real") && has(ln, "proxy=0"))
                nodeLeafWater = true;
            if (has(ln, "id=leaf_elec_0") && has(ln, "class=real")) nodeLeafElec = true;
            if (has(ln, "kind=gate") && has(ln, "class=real")) nodeGate = true;
        }
        if (startsWith(ln, "P2_CAVE_GEOMETRY_READY") && has(ln, "geometry=real")) readyReal = true;
        if (startsWith(ln, "P2_CAVE_GEOMETRY_DRAW") && has(ln, "geometry=real")) drawReal = true;
        if (startsWith(ln, "P2_CAVE_CARRY_PLAN") && has(ln, "blocking=")) {
            const size_t pos = ln.find("blocking=");
            if (pos != std::string::npos && pos + 9 < ln.size() && ln[pos + 9] >= '2')
                planBlocking = true;
        }
        if (startsWith(ln, "P2_CAVE_CARRY_BLOCKED") && has(ln, "carrying=1")) {
            carryingBlocked = true;
            if (has(ln, "hazard=water")) waterCarryingBlocked = true;
        }
        if (startsWith(ln, "P2_CAVE_CARRY_OPEN") || startsWith(ln, "P2_CAVE_GEOMETRY_GATE_OPEN")) {
            if (openIndex < 0) openIndex = (int)i;
            opened = true;
        }
        if (startsWith(ln, "P2_CAVE_ITEM_RECEIPT") && ln.find("treasure_elec") != std::string::npos &&
            has(ln, "new=1")) {
            ++elecNew;
            if (firstElecNewIndex < 0) firstElecNewIndex = (int)i;
        }
        if (startsWith(ln, "P2_CAVE_ROOMS_TIMELINE")) {
            if (has(ln, "tag=fresh_floor_no_abilities")) {
                freshSeen = true;
                if (has(ln, "hole=0")) freshHoleClosed = true;
            }
            if (has(ln, "tag=come_back_with_yellow")) {
                yellowSeen = true;
                if (has(ln, "treasure_elec=1")) yellowElec = true;
            }
            if (has(ln, "tag=return_with_yellow_and_blue")) {
                fullSeen = true;
                if (has(ln, "hole=1")) fullHole = true;
            }
        }
    }
    if (openIndex >= 0 && firstElecNewIndex >= 0) {
        if (firstElecNewIndex > openIndex) creditAfterOpen = true;
        else creditBeforeOpen = true;
    }

    // A staged grant for a *different* species (e.g. lane 48's still-staged
    // blue grant alongside the natural yellow one) must not poison the
    // natural verdict: only a staged grant for the naturally acquired
    // species downgrades acquisition.
    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& ln = lines[i];
        if (startsWith(ln, "P2_CAVE_ROOMS_GRANT") && has(ln, "staged=1") && !naturalSpecies.empty() &&
            has(ln, "species=" + naturalSpecies))
            stagedGrant = true;
    }

    const bool acquisition = budActor && accept && sproutNatural && naturalGrant && !stagedGrant;
    const bool geometry = nodeChokeWater && nodeLeafWater && nodeLeafElec && nodeGate && readyReal && drawReal;
    const bool blockRow = planBlocking && carryingBlocked && !creditBeforeOpen;
    const bool openRow = opened && creditAfterOpen && elecNew == 1 && naturalGrant && !stagedGrant;
    const bool waterRow = waterCarryingBlocked;
    const bool timelineRow = freshSeen && freshHoleClosed && yellowSeen && yellowElec && fullSeen && fullHole;

    std::cout << "acquisition=" << (acquisition ? "PASS" : "FAIL") << " geometry=" << (geometry ? "PASS" : "FAIL")
              << " block=" << (blockRow ? "PASS" : "FAIL") << " open_credit=" << (openRow ? "PASS" : "FAIL")
              << " water=" << (waterRow ? "PASS" : "FAIL") << " timeline=" << (timelineRow ? "PASS" : "FAIL") << "\n";
    const bool pass = acquisition && geometry && blockRow && openRow && waterRow && timelineRow;
    std::cout << (pass ? "PASS p2 cave item5 qa" : "FAIL p2 cave item5 qa") << "\n";
    return pass ? 0 : 1;
}
