// Bounded engineering controls; no canonical Stage, actual camera or GL grant.
#include "pc_p2_source_camera_math.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace p2original::cameraMath;
namespace {
unsigned checks=0;
void require(bool b,const char* name){++checks;if(!b){std::fprintf(stderr,"FAIL: %s\n",name);std::exit(1);}}
bool near(float a,float b){return std::fabs(a-b)<0.00002f;}
}
int main(){
 require(!processDisableCulling(),"source process-global zero initialized false");
 Matrix identity={{{1,0,0,0},{0,1,0,0},{0,0,1,0}}};
 Projection projection={60,2,1,100};Frustum f;
 require(derive(identity,projection,nullptr,f),"derive identity");
 const float half=3.14159265358979323846f/6;
 const float horizontal=std::atan(2*std::tan(half));
 require(near(f.plane[0].normal.y,-std::cos(half))&&near(f.plane[0].normal.z,-std::sin(half)),"top plane order/sign");
 require(near(f.plane[1].normal.y,std::cos(half))&&near(f.plane[1].normal.z,-std::sin(half)),"bottom plane order/sign");
 require(near(f.plane[2].normal.x,-std::cos(horizontal))&&near(f.plane[2].normal.z,-std::sin(horizontal)),"right aspect plane");
 require(near(f.plane[3].normal.x,std::cos(horizontal))&&near(f.plane[3].normal.z,-std::sin(horizontal)),"left aspect plane");
 require(f.plane[4].normal.z==1&&f.plane[4].offset==-100,"far plane");
 require(f.plane[5].normal.z==-1&&f.plane[5].offset==1,"near plane");
 require(sphereVisible(f,{0,0,-50},0),"interior");
 require(!sphereVisible(f,{0,0,50},10),"behind camera");
 require(sphereVisible(f,{0,0,9},10),"exact near sphere boundary");
 require(!sphereVisible(f,{0,0,9.0001f},10),"outside near sphere boundary");
 require(sphereVisible(f,{0,0,-110},10),"exact far sphere boundary");
 require(!sphereVisible(f,{0,0,-110.0001f},10),"outside far sphere boundary");
 require(!sphereVisible(f,{80,0,-50},10)&&!sphereVisible(f,{-80,0,-50},10),"both horizontal sides");
 require(!sphereVisible(f,{0,50,-50},10)&&!sphereVisible(f,{0,-50,-50},10),"both vertical sides");
 Matrix turned={{{0,0,-1,30},{0,1,0,-20},{1,0,0,-10}}};Frustum rotated;
 require(derive(turned,projection,nullptr,rotated),"rotated and translated derive");
 require(near(rotated.plane[5].offset,-9),"inverse translation uses columns, basis uses rows");
 require(sphereVisible(rotated,{-40,20,30},0),"rotated forward point");
 require(!sphereVisible(rotated,{60,20,30},10),"rotated behind point");
 Vec jst={50,60,70};Frustum jstF;
 require(derive(identity,projection,&jst,jstF)&&sphereVisible(jstF,{50,60,20},0),"JST position override");
 Viewport v0={0,{0,0,960,540},&f},v1={0,{0,0,480,540},&rotated};
 const Viewport* views[]={&v0,&v1};bool culled=true;
 require(cull(views,2,false,{-40,20,30},10,culled)&&!culled,"second viewport grants visibility outside first");
 v1.flags=1;
 require(cull(views,2,false,{-40,20,30},10,culled)&&culled,"disabled second viewport excluded");
 Viewport v2={0,{0,0,480,540},&rotated};const Viewport* three[]={&v0,&v1,&v2};
 require(cull(three,3,false,{-40,20,30},10,culled)&&!culled,"third referenced viewport alone grants visibility");
 require(cull(views,2,true,{-40,20,30},10,culled)&&!culled,"actual disable flag math semantics");
 v1.flags=2;require(viewable(v1),"other flag bits do not hide");
 v1.bounds2.right=.5f;require(!viewable(v1),"subpixel width hides");
 v1.bounds2.right=1;v1.bounds2.bottom=1;require(viewable(v1),"exact one pixel accepted");
 require(aspect({0,0,960,540})==960.0f/540&&aspect({0,0,0,540})==1,"source viewport aspect including zero");
 culled=true;v1.camera=nullptr;
 require(!cull(views,2,false,{0,0,-50},10,culled)&&culled,"missing later camera refuses even if first visible");
 require(!cull(views,2,true,{0,0,-50},10,culled)&&culled,"missing camera refuses despite disabled flag");
 require(!cull(views,4,false,{0,0,-50},10,culled),"source exclusive count bound");
 require(cull(nullptr,0,false,{0,0,-50},10,culled)&&culled,"zero referenced views culls");
 Frustum retained=f;projection.aspect=0;
 require(!derive(identity,projection,nullptr,f)&&f.plane[4].offset==retained.plane[4].offset,"bad projection preserves prior planes");
 projection.aspect=2;identity.row[1][1]=std::numeric_limits<float>::quiet_NaN();
 require(!derive(identity,projection,nullptr,f),"nonfinite matrix refuses");
 require(!cull(nullptr,0,false,{0,0,-50},-1,culled),"negative radius refuses");
 std::printf("PASS %u source-semantic math controls; no Stage/GL admission\n",checks);
}
