#pragma once
#include "pc_p2_attachments.h"
#include <map>
namespace p2original { namespace catfish {
inline std::shared_ptr<const p2attach::Bank> readAttachments(std::istream& input,std::string& error){
 auto bank=p2attach::read(input);
 auto fail=[&](const char* why)->std::shared_ptr<const p2attach::Bank>{error=why;return {};};
 if(!bank)return fail("invalid original Catfish attachment bank");
 const char* names[]={"kosi","ago","kamu1","kamu2","body","head","regL_momo","regL_sune","regR_momo","regR_sune"};
 const int parents[]={-1,0,1,1,0,0,0,6,0,8};
 if(bank->joints.size()!=10)return fail("original Catfish literal joint inventory mismatch");
 for(unsigned i=0;i<10;++i)if(bank->joints[i].name!=names[i]||bank->joints[i].parent!=parents[i])return fail("original Catfish joint hierarchy mismatch");
 const std::map<std::string,int> expected={{"attack",85},{"dead",95},{"flick",70},{"move1",25},{"type5",40},{"wait1",30},{"waitact2",16}};
 if(bank->clips.size()!=expected.size())return fail("original Catfish attachment clip inventory mismatch");
 for(const auto& clip:bank->clips){auto found=expected.find(clip.name);
  if(found==expected.end()||clip.duration!=found->second||clip.frames.size()!=size_t(clip.duration))return fail("original Catfish attachment clock mismatch");
  for(int f=0;f<clip.duration;++f)if(clip.frames[f]!=f)return fail("original Catfish attachment must contain every authored frame");
 }
 error.clear();return bank;
}
} }
