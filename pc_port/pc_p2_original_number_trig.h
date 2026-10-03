#ifndef PC_P2_ORIGINAL_NUMBER_TRIG_H
#define PC_P2_ORIGINAL_NUMBER_TRIG_H
#include <array>
#include <string>
namespace p2originalnumber { namespace trig {
using Table = std::array<std::array<float,2>,2048>;
// Pure bounded original JMath initializer and lookup. No owner/live-body grant.
bool buildTable(Table& output, std::string& error);
bool lookup(float face, float& sine, float& cosine, std::string& error);
} }
#endif
