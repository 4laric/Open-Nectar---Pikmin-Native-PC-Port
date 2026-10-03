#pragma once
#include <string>

// Flush the current native scene into its existing GeneratorCache bank without
// sunset, stock deposits, actor teardown or a realm switch. Call at a settled
// gameplay boundary. The cache image can be restored if card publication fails.
bool pc_p2_campaign_flush(std::string& reason);
