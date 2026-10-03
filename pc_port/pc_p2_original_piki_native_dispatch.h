#pragma once
#include <string>
class Piki;
class Graphics;
class CollEvent;
namespace p2original {namespace piki {
struct Handle;
// Only Unowned authorizes the ordinary P1 branch. Partial source allocations
// and owned Loading/Inactive actors are absorbed without physical/model writes.
enum class Dispatch {Unowned,Handled,Refused};
Dispatch dispatchUpdate(Piki*,float delta,std::string&);
Dispatch dispatchAnimation(Piki*,float delta,std::string&);
Dispatch dispatchAI(Piki*,float delta,std::string&);
Dispatch dispatchVelocity(Piki*,float delta,std::string&);
Dispatch dispatchMove(Piki*,float delta,bool gravity,std::string&);
Dispatch dispatchPostUpdate(Piki*,std::string&);
Dispatch dispatchDraw(Piki*,Graphics&,std::string&);
Dispatch dispatchBounce(Piki*,std::string&);
Dispatch dispatchCollision(Piki*,const CollEvent&,std::string&);
// Required singular factory/startup fatal sink. A refused void engine callback
// stops before any ordinary fallthrough or subsequent native mutation.
[[noreturn]] void nativeDispatchFailure(Piki*,const std::string&);
// Required genuine source cell/tree/contact producer, not P1 postUpdate or an
// empty successful census. It owns typed contact lifetimes and dispatches the
// actual source receiver/filter sequence. Missing authority must refuse.
bool nativeDispatchContacts(Handle,std::string&);
} }
