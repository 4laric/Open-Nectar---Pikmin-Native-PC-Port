#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
namespace p2original { namespace pikiJPA {
inline constexpr const char* sourceArchive="user/Ebisawa/effect/game.jpc";
inline constexpr const char* sourceArchiveSHA="ebf889b4e5df0391662bd531e220b93d148ab56fdec96362386b9cd1e734302c";
struct ResourceRole {const char* role;unsigned id;std::uint32_t offset,bytes;const char* sha256;};
const ResourceRole* resourceRoles(std::size_t& count);
// session carries the actual full session stamp representation supplied by the
// selected owner. Validation against current authority belongs to Scene.
struct SelectedIdentity {std::string campaignSHA,packetSHA,session;};
// Startup supplies already-selected raw bytes. Consumer never reopens a path.
// Offsets are relative to the authenticated game.jpc member, not the ISO.
struct SelectedBytes {
 std::string role,archiveMember,archiveSHA;
 std::uint32_t memberOffset=0,memberBytes=0;
 std::vector<unsigned char> bytes;
};
class Bank {
public:
 // Atomic: refused inputs preserve the preceding bank and identity.
 bool load(const SelectedIdentity&,const std::vector<SelectedBytes>&,std::string&);
 const std::vector<unsigned char>* bytes(const std::string& role)const;
 const SelectedIdentity& selected()const{return mSelected;}
private:
 SelectedIdentity mSelected;
 std::map<std::string,std::vector<unsigned char>> mBytes;
};
// Actual Piki color enum: Blue0,Red1,Yellow2,Purple3,White4,Bulbmin5.
unsigned haloId(unsigned species);
unsigned blurId(unsigned species);
} }
