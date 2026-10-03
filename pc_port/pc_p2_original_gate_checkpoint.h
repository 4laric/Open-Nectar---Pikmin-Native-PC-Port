#pragma once
#include "pc_p2_original_gate.h"
#include "pc_p2_original_catalog.h"

namespace p2original {
// Scene ownership supplies the retained incarnation; order is the observed
// physical birth order and never substitutes for InstanceIdentity::ordinal.
struct GateCheckpointEntry {
    InstanceIdentity identity;
    unsigned order = 0;
    GateState state;
};
// Whole-graph section role original_gate, version1, payload GCP1. This codec
// does not allocate actors, activate generations or publish a registry.
bool gateCheckpointExport(const std::string& campaign,
    const std::vector<GateRecord>& authority,
    const std::vector<GateCheckpointEntry>&,
    std::vector<std::uint8_t>&, std::string&);
bool gateCheckpointImport(const std::string& campaign,
    const std::vector<GateRecord>& authority,
    const std::vector<std::uint8_t>&,
    std::vector<GateCheckpointEntry>&, std::string&);
}
