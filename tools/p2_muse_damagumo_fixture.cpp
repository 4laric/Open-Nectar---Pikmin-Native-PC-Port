// Standalone engine-free marker-contract checker for the Damagumo56
// acceptance observer (issue #173, shard enemies-3).
//
// It implements the SAME fail-closed grammar as
// experimental/pikmin2_muse_damagumo.py so a future instrumented Damagumo
// fixture can be checked with a native tool as well as the Python reader.
// No engine headers, no game linkage, no output markers:
//
//   p2_muse_damagumo_fixture <native.log>
//
// exit 0: every contract check passed.
// exit 1: one or more checks failed (details on stdout).
// exit 2: wrong argv or unreadable log.
//
// Standalone compile (MinGW):
//   g++ -std=gnu++17 -Wall -Wextra -Werror
//     tools/p2_muse_damagumo_fixture.cpp -o p2_muse_damagumo_fixture
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

const double kSourceSpeed = 100.0;
const double kMinWalkDistance = 25.0;
const int kChildCount = 25;

bool contains(const std::string& text, const std::string& needle)
{
    return text.find(needle) != std::string::npos;
}

double trailingNumber(const std::string& line, const std::string& key)
{
    std::string::size_type at = line.find(key);
    if (at == std::string::npos) {
        return -1.0;
    }
    at += key.size();
    std::string value;
    while (at < line.size() && (line[at] == '-' || line[at] == '.' || line[at] == 'e' ||
                                line[at] == 'E' || (line[at] >= '0' && line[at] <= '9'))) {
        value.push_back(line[at]);
        ++at;
    }
    std::istringstream stream(value);
    double number = -1.0;
    stream >> number;
    return stream.fail() ? -1.0 : number;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <native.log>\n", argc > 0 ? argv[0] : "fixture");
        return 2;
    }
    std::ifstream input(argv[1]);
    if (!input.good()) {
        std::fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    const std::string text = buffer.str();

    const bool injected = contains(text, "P2_MUSE_DAMAGUMO_INJECT") ||
                          contains(text, "P2_LL_INJECT") ||
                          contains(text, "mhealth_injected=1") ||
                          contains(text, "injection=1") ||
                          contains(text, "forced_transport=1");
    const bool bind = contains(text, "P2_MUSE_DAMAGUMO_BIND generator=") &&
                      contains(text, "species=Damagumo native_fsm=implemented");
    const bool walk = contains(text, "P2_LONG_LEGS_WALK_END species=Damagumo");
    const bool walk_state = contains(text, "P2_LONG_LEGS_STATE species=Damagumo");
    const bool dead = contains(text, "P2_LONG_LEGS_DEAD species=Damagumo") &&
                      contains(text, "health=0");
    const bool child = contains(text, "P2_MUSE_DAMAGUMO_CHILD_BIRTH") &&
                        contains(text, "species=ShijimiChou") &&
                        contains(text, "count=25");
    const bool reentry = contains(text, "P2_MUSE_DAMAGUMO_REENTRY") &&
                         contains(text, "stale=0 fresh=1 rebind=1");
    const bool window = contains(text, "Experimental preview window set to 960x540 windowed and centered");
    const bool session = contains(text, "P2_MUSE_DAMAGUMO_SESSION navi=1");
    const bool completion = contains(text, "PASS P2_MUSE_DAMAGUMO");

    // Walk displacement must be real and within the source speed budget.
    bool walk_ok = false;
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.find("P2_LONG_LEGS_WALK_END species=Damagumo") == std::string::npos) {
            continue;
        }
        const double distance = trailingNumber(line, "distance=");
        const double seconds = trailingNumber(line, "seconds=");
        if (distance >= kMinWalkDistance && seconds > 0.0 &&
            distance / seconds <= kSourceSpeed * 1.25) {
            walk_ok = true;
        }
    }

    const std::map<std::string, bool> checks = {
        {"identity_spawn", bind && !injected},
        {"movement_animation", walk && walk_state && walk_ok && !injected},
        {"attacks_receivers", dead && !injected},
        {"death_corpse", dead && child},
        {"transport_reward", child},
        {"cleanup_reentry", reentry},
        {"window_960x540", window},
        {"live_session", session},
        {"not_injected", !injected},
        {"completion", completion},
    };
    int failures = 0;
    for (const auto& entry : checks) {
        std::printf("P2_MUSE_DAMAGUMO_CHECK %s ok=%d\n", entry.first.c_str(),
                    entry.second ? 1 : 0);
        if (!entry.second) {
            ++failures;
        }
    }
    if (failures == 0) {
        std::printf("P2_MUSE_DAMAGUMO_CONTRACT_OK\n");
        return 0;
    }
    std::printf("P2_MUSE_DAMAGUMO_CONTRACT_FAIL failures=%d\n", failures);
    return 1;
}
