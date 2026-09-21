#include "pc_p2_proxy.h"
#include "pc_randomizer_p2_roster.h"
#include "teki.h"
#include <cstdio>
#include <fstream>

const p2proxy::Table& pc_p2_proxy_table() {
    static p2proxy::Table table;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        std::ifstream in("p2-proxy-campaign.txt");
        if (!in) {
            table.valid = false;
            table.error.clear();
            table.rows.clear();
        } else {
            table = p2proxy::parse(in, randomizerP2IsBindable, TEKI_TypeCount);
            if (table.valid) {
                std::printf("P2_PROXY_TABLE rows=%u\n", (unsigned)table.rows.size());
            } else {
                std::printf("P2_SETUP_SKIP proxy table_invalid %s\n", table.error.c_str());
            }
        }
    }
    return table;
}

int pc_p2_proxy_host(unsigned source) {
    const p2proxy::Table& table = pc_p2_proxy_table();
    if (!table.valid) return -1;
    const p2proxy::Row* row = p2proxy::bySource(table, source);
    return row ? row->host : -1;
}
