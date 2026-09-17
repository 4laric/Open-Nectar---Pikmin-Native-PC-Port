// Standalone probe for the squad-spawn gateDiag throttle contract (#745).

// Mirrors the fixture throttle decision with injected observed counts; proves
// first/periodic/cap behavior with no engine headers and no runtime.

#include <cstdio>

static int lastDiagObserved=-1,stuckPrints=0;
static bool wantDiag(int observed){
 if(observed!=lastDiagObserved){lastDiagObserved=observed;stuckPrints=0;}
 ++stuckPrints;
 if((stuckPrints>1&&(stuckPrints-1)%120!=0)||stuckPrints>600)return false;
 return true;
}

int main(){
 int fails=0;
 auto check=[&](bool cond,const char* name){if(!cond){std::printf("PROBE_FAIL %s\n",name);++fails;}};
 lastDiagObserved=-1;stuckPrints=0;
 check(wantDiag(1),"first");
 check(!wantDiag(1),"second-suppressed");
 for(int i=0;i<118;++i)wantDiag(1);
 check(wantDiag(1),"periodic-121");
 check(wantDiag(2),"reset-first");
 lastDiagObserved=2;stuckPrints=600;
 check(!wantDiag(2),"cap");
 if(fails){std::printf("PROBE_FAIL count=%d\n",fails);return 1;}
 std::printf("PROBE_PASS checks=5\n");return 0;
}
