#pragma once
#include "pc_p2_original_white_bank.h"
#include <cstdint>
#include <map>
namespace p2original {
class WhiteSelectedKit {
public:
 const WhiteBank& bank()const noexcept{return bank_;}
 const std::map<std::string,std::string>& files()const noexcept{return files_;}
 const std::string& campaign()const noexcept{return campaign_;}
 const std::string& session()const noexcept{return session_;}
 std::uint64_t selection()const noexcept{return selection_;}
private:
 friend bool readWhiteSelectedKit(WhiteSelectedKit&,std::string&);
 friend bool whiteSelectedKitCurrent(const WhiteSelectedKit&) noexcept;
 WhiteBank bank_;
 std::string campaign_, session_;
 std::uint64_t selection_=0;
 // Exact authenticated bytes consumed by preflight. Keys are selected roles.
 // Keeping these bytes does not install a Shape or authorize an actor birth.
 std::map<std::string,std::string> files_;
};
bool readWhiteSelectedKit(WhiteSelectedKit&,std::string& error);
bool whiteSelectedKitCurrent(const WhiteSelectedKit&) noexcept;
}
