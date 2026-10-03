#pragma once
#include <array>
#include <string>
namespace p2original {
// Source provenance and shape bytes are authenticated by OriginalSession.
// This parser validates the sampled-animation contract; it grants no birth.
struct WhiteClip {
 std::string name, sourceSha;
 unsigned duration=0;
 std::array<unsigned,12> frames{};
 std::array<std::array<float,12>,12> happa{};
};
struct WhiteBank {
 std::string modelSha, parameterSha;
 std::array<WhiteClip,3> clips;
};
bool parseWhiteBank(const std::string& bytes, WhiteBank& out, std::string& error);
}
