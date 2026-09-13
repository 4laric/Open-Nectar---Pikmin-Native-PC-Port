#include "pc_p2_demon_pose_bank.h"
#include <cassert>
#include <fstream>
int main(int argc, char** argv) {
    assert(argc == 3);
    P2DemonPoseBank bank;
    assert(bank.load(argv[1]));
    const auto* zero = bank.exact(0);
    const auto* hit = bank.exact(17);
    assert(zero && hit && !bank.exact(18));
    const auto saved = hit->values;
    assert(zero->values != saved);
    { std::ofstream bad(argv[2]); bad << "P2_DEMON_MOUTHS_1 " << std::string(64, 'a') << " 129"; }
    assert(!bank.load(argv[2]));
    assert(bank.exact(17)->values == saved);
    { std::ofstream bad(argv[2]); bad << "P2_DEMON_MOUTHS_1 " << std::string(64, 'a') << " 1 0 nan"; }
    assert(!bank.load(argv[2]));
    assert(bank.exact(17)->values == saved);
}
