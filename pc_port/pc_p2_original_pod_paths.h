#pragma once
#include <string>
namespace p2originalpod {
// Match System::openFile(relative=true) followed by DVDOpen's assets prefix.
// Never hash a caller-selected physical file independently of the engine path.
inline bool engineModelPath(const char* active,const char* root,const std::string& model,std::string& out){
 if(!active||!root||model.empty()||model.size()>=256||model.find('?')!=std::string::npos)return false;
 std::string next=std::string(active)+root+model;
 if(next.size()>=256)return false; // Native System::openFile PATH_MAX is 256.
 if(next.front()=='/'||next.front()=='\\')next.erase(0,1);
 out="assets/"+next;return true;
}
}
