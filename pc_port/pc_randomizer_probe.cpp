#include "pc_randomizer.h"
#include <chrono>
#include <cstdio>
#include <thread>
#include <cstring>
#undef NDEBUG
#include <cassert>
int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    if (!pc_randomizer_init(argc, argv)) {
        if (pc_randomizer_enabled() || pc_randomizer_goal() || pc_randomizer_next_day(29) != 30) return 4;
        std::puts("standalone adapter inert"); return 0;
    }
    for (int i = 0; i < 100; ++i) {
        pc_randomizer_update();
        if (pc_randomizer_ready()) {
            if (pc_randomizer_expanded()) {
                assert(pc_randomizer_field_capacity() == 20);
                pc_randomizer_observe_population(19, true);
                pc_randomizer_observe_population(20, false);
                assert(!pc_randomizer_checked("Population: 20 Pikmin in the field"));
                pc_randomizer_observe_population(20, true);
                pc_randomizer_observe_population(30, true);
                assert(!pc_randomizer_checked("Population: 30 Pikmin in the field"));
                pc_randomizer_enemy_defeated(3, 1, false, true);
                pc_randomizer_enemy_defeated(3, 1, true, false);
                assert(!pc_randomizer_checked("Bestiary: Dwarf Bulborb"));
                pc_randomizer_enemy_defeated(3, 1, true, true);
                pc_randomizer_enemy_defeated(3, 1, true, true);
                pc_randomizer_enemy_defeated(6, 1, true, true); // Honeywisp is not in this catalog.
                pc_randomizer_observe_exploration(4, 800, 0, true, true); // locked area
                assert(!pc_randomizer_checked("Explore: The Final Trial - Land"));
                pc_randomizer_observe_exploration(1, 800, 0, false, true);
                pc_randomizer_observe_exploration(1, 800, 0, true, false);
                assert(!pc_randomizer_checked("Explore: The Forest of Hope - Land"));
                pc_randomizer_observe_exploration(1, 599, 0, true, true);
                assert(!pc_randomizer_checked("Explore: The Forest of Hope - Scout"));
                pc_randomizer_observe_exploration(1, 600, 0, true, true);
                std::puts("EXPANDED_INITIAL");
                for (int step = 0; step < 100; ++step) {
                    pc_randomizer_update();
                    if (pc_randomizer_field_capacity() == 100) {
                        assert(!pc_randomizer_checked("Population: 100 Pikmin in the field"));
                        pc_randomizer_observe_population(99, true);
                        assert(!pc_randomizer_checked("Population: 100 Pikmin in the field"));
                        pc_randomizer_observe_population(100, true);
                        pc_randomizer_observe_exploration(4, 600, 0, true, true);
                        assert(pc_randomizer_checked("Explore: The Final Trial - Scout"));
                        std::puts("EXPANDED_COMPLETE"); return 0;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
                return 6;
            }
            pc_randomizer_check("Pikmin: Eternal Fuel Dynamo");
            pc_randomizer_check("Pikmin: Eternal Fuel Dynamo");
            if (pc_randomizer_next_day(29) != 29 || pc_randomizer_has("Zora Tunic")) return 5;
            std::puts("standalone ready; duplicate delivery tested"); return 0;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::fprintf(stderr, "standalone handshake timed out\n"); return 3;
}
