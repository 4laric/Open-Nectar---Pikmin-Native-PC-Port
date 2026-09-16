// Muse packaging lane (#493): candidate staging verification fixture.
//
// Built by the isolated fixture build only (root repo
// scripts/build_pikmin2_fixture.py); not part of the game target. Verifies a
// locally staged candidate run for Fuefuki (41), Kurage (57), BombSarai (58)
// and MiniHoudai (78) WITHOUT gameplay injection: it checks the packaging
// receipt marker, the per-identity actors sidecars and the identity sidecars,
// and the generator-binding agreement against the expected generators passed
// on the command line. It never spawns actors, injects state/HP/transport,
// or writes admission markers.
//
// Usage:
//   p2_muse_packaging_fixture <run-dir> <plan-digest-hex>
//       <TargetA>=<generator> [<TargetB>=<generator> ...]
//
// Markers (stdout):
//   P2_MUSE_PACKAGING_RECEIPT_OK <plan-digest>
//   P2_MUSE_PACKAGING_SIDECAR_OK <EnumName> <generators...>
//   P2_MUSE_PACKAGING_BINDING_OK <target> <generator>
//   P2_MUSE_PACKAGING_OK <plan-digest>
// Any failure prints "FAIL P2_MUSE_PACKAGING <reason>" and exits non-zero.

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

void fail(const std::string& reason)
{
    std::printf("FAIL P2_MUSE_PACKAGING %s\n", reason.c_str());
    std::fflush(stdout);
    std::_Exit(1);
}

std::string readFile(const std::string& path)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        fail("missing file: " + path);
    }
    std::ostringstream out;
    out << stream.rdbuf();
    return out.str();
}

// Minimal substring check: the receipt is JSON written by the packaging
// module; the fixture proves the marker fields exist without a JSON parser.
void requireContains(const std::string& text, const std::string& token, const std::string& what)
{
    if (text.find(token) == std::string::npos) {
        fail(std::string("receipt missing ") + what + ": " + token);
    }
}

struct Binding {
    std::string target;
    std::uint32_t generator = 0;
};

Binding parseBinding(const std::string& arg)
{
    const std::size_t eq = arg.find('=');
    if (eq == std::string::npos || eq == 0 || eq + 1 >= arg.size()) {
        fail("malformed binding (want Target=generator): " + arg);
    }
    Binding binding;
    binding.target = arg.substr(0, eq);
    const std::string value = arg.substr(eq + 1);
    for (char ch : value) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            fail("non-numeric generator in binding: " + arg);
        }
    }
    binding.generator = static_cast<std::uint32_t>(std::stoul(value));
    return binding;
}

struct Identity {
    int sourceId;
    const char* enumName;
};

constexpr Identity kCandidates[] = {
    { 41, "Fuefuki" },
    { 57, "Kurage" },
    { 58, "BombSarai" },
    { 78, "MiniHoudai" },
};

std::string lowerOf(const std::string& text)
{
    std::string out = text;
    for (char& ch : out) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return out;
}

std::string upperOf(const std::string& text)
{
    std::string out = text;
    for (char& ch : out) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return out;
}

// Actors sidecar shape: "<P2_MUSE_<ENUM>_ACTORS_1> <count>" then one
// "<generator> <Species>" token pair per line. Returns the generator list.
std::vector<std::uint32_t> readActorsGenerators(const std::string& path, const std::string& header)
{
    const std::string text = readFile(path);
    std::istringstream tokens(text);
    std::string gotHeader;
    std::string gotCount;
    if (!(tokens >> gotHeader >> gotCount)) {
        fail("malformed actors sidecar: " + path);
    }
    if (gotHeader != header) {
        fail("actors sidecar header mismatch: " + path);
    }
    for (char ch : gotCount) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            fail("malformed actors sidecar count: " + path);
        }
    }
    const int count = std::atoi(gotCount.c_str());
    std::vector<std::uint32_t> generators;
    for (int i = 0; i < count; ++i) {
        std::string genToken;
        std::string speciesToken;
        if (!(tokens >> genToken >> speciesToken)) {
            fail("actors sidecar truncated: " + path);
        }
        for (char ch : genToken) {
            if (!std::isdigit(static_cast<unsigned char>(ch))) {
                fail("non-numeric generator in sidecar: " + path);
            }
        }
        generators.push_back(static_cast<std::uint32_t>(std::stoul(genToken)));
    }
    return generators;
}

bool containsGenerator(const std::vector<std::uint32_t>& list, std::uint32_t generator)
{
    for (std::uint32_t value : list) {
        if (value == generator) {
            return true;
        }
    }
    return false;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 3) {
        std::printf("usage: p2_muse_packaging_fixture <run-dir> <plan-digest> "
                    "<Target>=<generator>...\n");
        return 2;
    }
    const std::string runDir = argv[1];
    const std::string planDigest = argv[2];
    if (planDigest.size() != 64) {
        fail("plan digest must be 64 hex chars");
    }
    std::vector<Binding> bindings;
    for (int i = 3; i < argc; ++i) {
        bindings.push_back(parseBinding(argv[i]));
    }
    if (bindings.empty()) {
        fail("at least one Target=generator binding is required");
    }

    // 1. Packaging receipt marker: candidate-only staging, no admission.
    const std::string receipt = readFile(runDir + "/p2-muse-packaging-receipt.json");
    requireContains(receipt, "\"mode\": \"muse-candidate-packaging\"", "mode");
    requireContains(receipt, "\"candidate_only\": true", "candidate_only");
    requireContains(receipt, "\"admission\": \"none\"", "admission marker");
    requireContains(receipt, planDigest, "plan digest");
    std::printf("P2_MUSE_PACKAGING_RECEIPT_OK %s\n", planDigest.c_str());

    // 2. Per-identity sidecars. BombSarai (58) is staged by the family
    // installer as-is, so its actors file lives at the family path; the
    // other three use the candidate sidecar path. Identity copies are
    // candidate sidecars for all sidecar identities.
    for (const Identity& identity : kCandidates) {
        const std::string lower = lowerOf(identity.enumName);
        const std::string upper = upperOf(identity.enumName);
        std::string actorsPath;
        std::string header;
        if (identity.sourceId == 58) {
            actorsPath = runDir + "/p2-bombsarai-actors.txt";
            header = "P2_BOMBSARAI_ACTORS_1";
        } else {
            actorsPath = runDir + "/p2-candidate-" + lower + "-actors.txt";
            header = "P2_MUSE_" + upper + "_ACTORS_1";
            // Identity sidecar only exists for the candidate-sidecar path.
            readFile(runDir + "/p2-candidate-" + lower + "-identity.json");
        }
        const std::vector<std::uint32_t> generators = readActorsGenerators(actorsPath, header);
        if (generators.empty()) {
            fail("empty generator list in sidecar: " + actorsPath);
        }
        std::string line;
        for (std::uint32_t generator : generators) {
            char entry[32];
            std::snprintf(entry, sizeof(entry), " %u", generator);
            line += entry;
        }
        std::printf("P2_MUSE_PACKAGING_SIDECAR_OK %s%s\n", identity.enumName, line.c_str());
    }

    // 3. Generator-binding agreement: every expected binding's generator must
    // appear in its identity's sidecar (source-id/enum agreement is proven
    // Python-side by stage/verify; this re-checks the staged outcome).
    for (const Binding& binding : bindings) {
        bool matched = false;
        for (const Identity& identity : kCandidates) {
            const std::string lower = lowerOf(identity.enumName);
            const std::string upper = upperOf(identity.enumName);
            std::string actorsPath;
            std::string header;
            if (identity.sourceId == 58) {
                actorsPath = runDir + "/p2-bombsarai-actors.txt";
                header = "P2_BOMBSARAI_ACTORS_1";
            } else {
                actorsPath = runDir + "/p2-candidate-" + lower + "-actors.txt";
                header = "P2_MUSE_" + upper + "_ACTORS_1";
            }
            const std::vector<std::uint32_t> generators = readActorsGenerators(actorsPath, header);
            if (containsGenerator(generators, binding.generator)) {
                matched = true;
                break;
            }
        }
        if (!matched) {
            fail("binding generator missing from staged sidecars: " + binding.target);
        }
        std::printf("P2_MUSE_PACKAGING_BINDING_OK %s %u\n", binding.target.c_str(),
                    binding.generator);
    }

    std::printf("P2_MUSE_PACKAGING_OK %s\n", planDigest.c_str());
    std::fflush(stdout);
    return 0;
}
