#pragma once
namespace p2retailcargo {
enum class BorrowRetirement {Detached,Recycled,ViewInUse,LiveProfile};
// Call only while the actual native pool itself remains alive, after kill has
// returned. A recycled slot belongs to its replacement, never the old record.
template<class Actor,class View,class Profile>
BorrowRetirement retireBorrow(Actor* actor,View* view,Profile* profile){
    if(!actor)return BorrowRetirement::Detached;
    if(actor->mPelletView==view)return BorrowRetirement::ViewInUse;
    if(actor->mConfig!=profile)return BorrowRetirement::Recycled;
    if(actor->isAlive())return BorrowRetirement::LiveProfile;
    actor->mConfig=nullptr;return BorrowRetirement::Detached;
}
}
