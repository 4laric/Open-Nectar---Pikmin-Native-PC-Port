#pragma once
#include "pc_p2_proxy_table.h"

// Engine-linked proxy table loader (#871). Reads p2-proxy-campaign.txt from
// the current directory once per process.
const p2proxy::Table& pc_p2_proxy_table();
int pc_p2_proxy_host(unsigned source);
