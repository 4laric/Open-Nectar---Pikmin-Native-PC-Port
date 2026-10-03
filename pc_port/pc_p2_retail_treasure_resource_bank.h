#pragma once
#include <cstddef>
#include <map>
#include <string>
namespace p2retailcargo {
// Storage policy only. The caller must authenticate the bank and every selected
// model buffer before acquire; this never issues scene or gameplay authority.
template<class Owner>
class RetainedResourceBank {
public:
    bool pin(const void* system,const std::string& bank,std::size_t capacity,std::string& error){
        if(!system||bank.empty()||!capacity||capacity>201){error="cargo retained bank bounds";return false;}
        if(mSystem){
            if(mSystem!=system||mBank!=bank||mCapacity!=capacity){error="cargo retained bank lifetime differs";return false;}
        }else{mSystem=system;mBank=bank;mCapacity=capacity;}
        return true;
    }
    Owner* acquire(const std::string& id,const std::string& hash,bool& fresh,std::string& error){
        fresh=false;if(!mSystem||id.empty()||hash.empty()){error="cargo retained model identity";return nullptr;}
        auto found=mOwners.find(id);
        if(found!=mOwners.end()){
            if(found->second.hash!=hash){error="cargo retained model hash differs";return nullptr;}
            return &found->second; // Even a partial owner is never abandoned/reallocated.
        }
        if(mOwners.size()>=mCapacity){error="cargo retained model capacity";return nullptr;}
        auto inserted=mOwners.try_emplace(id);inserted.first->second.hash=hash;fresh=true;
        return &inserted.first->second; // Adopt before the parser can allocate nested objects.
    }
    const void* system()const{return mSystem;}
    const std::map<std::string,Owner>& entries()const{return mOwners;}
private:
    const void* mSystem=nullptr;std::string mBank;std::size_t mCapacity=0;
    std::map<std::string,Owner> mOwners;
};
}
