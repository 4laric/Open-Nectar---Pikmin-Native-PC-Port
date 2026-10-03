#include "pc_p2_original_gate.h"
#include "netplay/pc_netplay_sha256.h"
#include <cstring>
namespace p2original {
bool gateExport(const GateRecord& r,const GateState& s,std::vector<std::uint8_t>& out,std::string& e){
 if(!gateSettled(r,s,e))return false;
 std::vector<std::uint8_t> bytes{'G','S','0','1'};
 auto digest=gateDigest(r);bytes.insert(bytes.end(),digest.begin(),digest.end());
 auto word=[&](std::uint32_t v){for(unsigned i=0;i<4;++i)bytes.push_back(std::uint8_t(v>>(8*i)));};
 word(r.uid);std::uint32_t f;std::memcpy(&f,&s.health,4);word(f);word(s.segmentsDown);
 unsigned char hash[32];pc_netplay_sha::sha256(bytes.data(),bytes.size(),hash);bytes.insert(bytes.end(),hash,hash+32);
 out.swap(bytes);e.clear();return true;
}
bool gateImport(const GateRecord& r,const std::vector<std::uint8_t>& bytes,GateState& out,std::string& e){
 if(!validateGate(r,e))return false;
 if(bytes.size()!=112||std::memcmp(bytes.data(),"GS01",4)!=0){e="invalid original gate save envelope";return false;}
 unsigned char hash[32];pc_netplay_sha::sha256(bytes.data(),80,hash);
 if(std::memcmp(hash,bytes.data()+80,32)){e="original gate save checksum mismatch";return false;}
 auto digest=gateDigest(r);
 if(std::memcmp(bytes.data()+4,digest.data(),64)){e="original gate save source mismatch";return false;}
 auto word=[&](unsigned offset){std::uint32_t v=0;for(unsigned i=0;i<4;++i)v|=std::uint32_t(bytes[offset+i])<<(8*i);return v;};
 if(word(68)!=r.uid){e="original gate save UID mismatch";return false;}
 GateState next;auto f=word(72);std::memcpy(&next.health,&f,4);next.segmentsDown=word(76);
 next.phase=next.segmentsDown==3?GatePhase::Open:GatePhase::Wait;
 if(!gateSettled(r,next,e))return false;
 out=next;e.clear();return true;
}
}
