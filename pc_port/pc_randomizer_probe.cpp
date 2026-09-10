#include "pc_randomizer.h"
#include <chrono>
#include <cstdio>
#include <thread>
int main(int argc, char** argv) {
    if (!pc_randomizer_init(argc, argv)) {
        if (pc_randomizer_enabled() || pc_randomizer_goal() || pc_randomizer_next_day(29) != 30) return 4;
        std::puts("standalone adapter inert"); return 0;
    }
    for (int i = 0; i < 100; ++i) {
        pc_randomizer_update();
        if (pc_randomizer_ready()) {
            pc_randomizer_check("Pikmin: Eternal Fuel Dynamo");
            pc_randomizer_check("Pikmin: Eternal Fuel Dynamo");
            if (pc_randomizer_next_day(29) != 29 || pc_randomizer_has("Zora Tunic")) return 5;
            std::puts("standalone ready; duplicate delivery tested"); return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::fprintf(stderr, "standalone handshake timed out\n"); return 3;
}
