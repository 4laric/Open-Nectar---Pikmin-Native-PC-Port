#include "pc_p2_original_number_lod.h"
#include <cmath>
#include <cstdio>
#include <limits>
#if defined(PIKI_NUMBER_NATIVE_CAMERA_COMPILE_CONTROL)
#include "Camera.h"
// Compilation control against the actual native API, without constructing
// a fake live Camera or linking/running native gameplay.
template bool p2originalnumber::lod::snapshotNativeCamera<Camera>(const Camera*,p2originalnumber::lod::CameraSnapshot&,std::string&);
#endif
using namespace p2originalnumber::lod;
namespace {
bool near(float a,float b,float tolerance=.000001f){return std::fabs(a-b)<=tolerance;}
CameraSnapshot snapshot(){CameraSnapshot c;c.sample={{},{0,0,1},1,-1};c.planeCount=1;c.planes[0]={{0,0,1},-10000};return c;}
// API-shape unit fixture only; actual Camera compilation is tested separately.
struct NativePlane { struct { Vec3 mNormal;float mOffset; }mPlane; };
struct CameraFields {
 Vec3 mPosition{1,2,3},mViewZAxis{0,0,-1};
 float mFov=60,mNear=1,mFar=1000;
 int mActivePlaneCount=1;
 NativePlane plane{{{0,0,1},-10000}};
 NativePlane* mPlanePointers[6]{&plane};
};
}
int main(){
 unsigned checks=0,failures=0;std::string error;
 auto check=[&](bool ok,const char* name){++checks;if(!ok){++failures;std::printf("FAIL %s\n",name);}};
 CameraSample projection;
 check(projectionSample({1,2,3},{0,0,1},60,1,1000,projection,error),"source projection constants from actual fields");
 const unsigned index=unsigned(int((((60.f*.5f)/180.f)*3.1415927f)*325.9493f))&0x7ffu;
 const double tableAngle=double(index)*double(6.2831855f)/2048.;
 check(near(projection.fieldOfViewTangent,float(std::cos(tableAngle))/float(std::sin(tableAngle)))&&near(projection.cameraSizeModifier,-999.f/2000.f),"source float LUT index and projection modifier");
 CameraSample beforeProjection=projection;
 check(!projectionSample({}, {},60,1,1000,projection,error)&&projection.cameraSizeModifier==beforeProjection.cameraSizeModifier,"missing view direction refuses atomically");
 for(float fov:{0.f,180.f,std::numeric_limits<float>::quiet_NaN()})check(!projectionSample({}, {0,0,1},fov,1,1000,projection,error),"invalid actual FOV refused");
 check(!projectionSample({}, {0,0,1},60,0,1000,projection,error)&&!projectionSample({}, {0,0,1},60,10,1,projection,error),"invalid actual projection range refused");
 float size=99;CameraSnapshot camera=snapshot();
 check(calcScreenSize(camera.sample,{{0,0,10},1},size,error)&&size==.1f,"source projected-depth size");
 check(calcScreenSize(camera.sample,{{100,200,10},1},size,error)&&size==.1f,"off-axis distance does not replace projected depth");
 check(calcScreenSize(camera.sample,{{0,0,-10},1},size,error)&&size==.1f,"source absolute size behind camera");
 check(calcScreenSize(camera.sample,{{1,1,0},1},size,error)&&std::isinf(size)&&size>0,"source nonzero sphere on eye plane produces positive infinity");
 check(calcScreenSize(camera.sample,{{1,1,0},0},size,error)&&std::isnan(size),"source zero-over-zero comparison semantics retained");
 size=99;check(!calcScreenSize(camera.sample,{{},-1},size,error)&&size==99,"invalid source sphere output unchanged");
 bool visible=false;camera.planes[0]={{0,0,1},0};
 check(sphereVisible(camera,{{0,0,-1},1},visible,error)&&visible,"sphere tangent to cull plane visible");
 check(sphereVisible(camera,{{0,0,-1.001f},1},visible,error)&&!visible,"sphere outside plane culled");
 camera=snapshot();camera.planeCount=2;camera.planes[1]={{1,0,0},0};
 check(sphereVisible(camera,{{-2,0,10},1},visible,error)&&!visible,"all actual source planes used");
 visible=true;camera.planes[1].normal={2,0,0};check(!sphereVisible(camera,{{},1},visible,error)&&visible,"invalid plane refusal preserves result");
 camera=snapshot();Viewport view{true,&camera};Result result;
 check(evaluate({{0,0,1},.071f},&view,1,false,result,error)&&result.flags==(Visible|VisibleVP0)&&usesRigidFive(result),"visible Near enables rigid Five");
 check(evaluate({{0,0,1},.07f},&view,1,false,result,error)&&result.flags==(Visible|VisibleVP0|Mid)&&usesRigidFive(result),"exact .07 boundary is Mid and remains rigid");
 check(evaluate({{0,0,1},.020001f},&view,1,false,result,error)&&result.flags==(Visible|VisibleVP0|Mid),"above .02 is Mid");
 check(evaluate({{0,0,1},.02f},&view,1,false,result,error)&&result.flags==(Visible|VisibleVP0|Far)&&!usesRigidFive(result),"exact .02 boundary is Far and simple Five");
 check(evaluate({{0,0,1},.001f},&view,1,true,result,error)&&result.flags==(Visible|VisibleVP0|Far|PikiInCell),"Piki cell flag preserves Far tier");
 camera.planes[0].offset=1000;
 check(evaluate({{0,0,1},1},&view,1,false,result,error)&&result.flags==Far&&!usesRigidFive(result),"fully invisible viewport forces Far despite large screen size");
 movieActorOverride(result);check(result.flags==(Far|Visible|VisibleVP0|VisibleVP1)&&!usesRigidFive(result),"source movie actor adds visibility but preserves Far dynamics");
 CameraSnapshot nearCamera=snapshot(),farCamera=snapshot();farCamera.sample.position.z=-99;
 Viewport two[2]{{true,&farCamera},{true,&nearCamera}};
 check(evaluate({{0,0,1},.1f},two,2,false,result,error)&&result.flags==(Visible|VisibleVP0|VisibleVP1)&&result.sampled[0]&&result.sampled[1],"two actual viewports select closest projected tier");
 nearCamera.planes[0].offset=1000;
 check(evaluate({{0,0,1},.1f},two,2,false,result,error)&&result.flags==(Visible|VisibleVP0)&&usesRigidFive(result),"invisible Near camera still contributes tier when another viewport sees sphere");
 two[1].viewable=false;two[1].camera=nullptr;
 check(evaluate({{0,0,1},.1f},two,2,false,result,error)&&result.flags==(Visible|VisibleVP0|Far)&&!result.sampled[1],"unviewable viewport not sampled and cannot promote tier");
 Viewport hidden{false,nullptr};check(evaluate({{0,0,1},1},&hidden,1,true,result,error)&&result.flags==(Far|PikiInCell),"actual unviewable roster supports no camera");
 check(evaluate({{0,0,1},1},nullptr,0,false,result,error)&&result.flags==Far,"empty actual roster follows source Far");
 Result before=result;Viewport missing{true,nullptr};
 check(!evaluate({{0,0,1},1},&missing,1,false,result,error)&&result.flags==before.flags,"viewable missing camera refuses atomically");
 check(!evaluate({{0,0,1},1},two,3,false,result,error)&&result.flags==before.flags,"source two-viewport bound enforced");
 nearCamera=snapshot();view.camera=&nearCamera;
 check(evaluate({{1,1,0},1},&view,1,false,result,error)&&usesRigidFive(result),"eye-plane source infinite size compares Near");
 check(evaluate({{1,1,0},0},&view,1,false,result,error)&&!usesRigidFive(result)&&(result.flags&Far),"source NaN comparisons classify Far");
 Sphere number;check(numberSphere(1,{1,2,3},number,error)&&number.radius==20.60190773010254f&&number.position.y==2,"verified One direct model LOD radius and actual root center");
 check(numberSphere(5,{4,5,6},number,error)&&number.radius==41.03627395629883f&&number.position.y==5,"verified Five direct model LOD radius differs from pick/collision radii");
 Sphere beforeNumber=number;check(!numberSphere(2,{},number,error)&&number.radius==beforeNumber.radius,"unsupported number size refused atomically");
 CameraFields actualFields;CameraSnapshot extracted;
 check(snapshotNativeCamera(&actualFields,extracted,error)&&extracted.sample.position.x==1&&extracted.sample.viewVector.z==1&&extracted.planeCount==1,"thin Camera field reader captures actual direction/projection/planes");
 check(snapshotNativeCamera<CameraFields>(nullptr,extracted,error)==false,"actual camera pointer required");
 CameraSnapshot beforeExtracted=extracted;actualFields.mPlanePointers[0]=nullptr;
 check(!snapshotNativeCamera(&actualFields,extracted,error)&&extracted.sample.position.x==beforeExtracted.sample.position.x,"missing actual plane refuses atomically");
 actualFields.mPlanePointers[0]=&actualFields.plane;actualFields.mActivePlaneCount=0;
 check(!snapshotNativeCamera(&actualFields,extracted,error),"unprepared native camera frustum refused");
 actualFields.mActivePlaneCount=7;check(!snapshotNativeCamera(&actualFields,extracted,error),"native active plane bound checked before indexing");
 std::printf("original_number_lod checks=%u failures=%u native=0 viewport_admission=0 gameplay=0 save=0\n",checks,failures);
 return failures?1:0;
}
