#pragma once
#include <string>
namespace p2original {namespace captain {namespace nativereader {
// Actual stage composition calls after Body constructs its sole Plate around
// the initialized Reader's PlateSource. Borrows that exact stable storage.
bool bindNativePlateDriver(std::string&);
}}}
