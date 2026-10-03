#pragma once
#include "pc_p2_authored_cave_session.h"
#include <string>
// Validates readonly launch inputs only; does not admit a live world or SAVE.
bool pc_p2_authored_cave_route_validate(const P2AuthoredCaveRoute&,
    const std::string& runRoot,std::string& error);
