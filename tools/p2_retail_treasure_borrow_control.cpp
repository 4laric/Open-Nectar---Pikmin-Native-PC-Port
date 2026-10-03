#include "pc_p2_retail_treasure_borrow.h"
#include <cassert>
#include <iostream>
// Pool-borrow policy control only. No native actor, source or receipt authority.
struct Profile{};struct View{};
struct Slot{Profile* mConfig=nullptr;View* mPelletView=nullptr;bool live=false;bool isAlive(){return live;}};
int main(){
    using p2retailcargo::retireBorrow;using R=p2retailcargo::BorrowRetirement;
    Profile original,replacement;View view,other;Slot slot;
    slot.mConfig=&original;slot.mPelletView=&view;slot.live=true;
    assert(retireBorrow(&slot,&view,&original)==R::ViewInUse&&slot.mConfig==&original);
    slot.mPelletView=nullptr;
    assert(retireBorrow(&slot,&view,&original)==R::LiveProfile&&slot.mConfig==&original);
    slot.live=false;assert(retireBorrow(&slot,&view,&original)==R::Detached&&slot.mConfig==nullptr);
    slot.mConfig=&replacement;slot.mPelletView=&other;slot.live=true;
    assert(retireBorrow(&slot,&view,&original)==R::Recycled);
    assert(slot.mConfig==&replacement&&slot.mPelletView==&other&&slot.live);
    assert((retireBorrow<Slot,View,Profile>(nullptr,&view,&original)==R::Detached));
    std::cout<<"PASS active-view/live-profile refusal, killed-borrow detach and recycled-slot preservation; no gameplay authority\n";
}
