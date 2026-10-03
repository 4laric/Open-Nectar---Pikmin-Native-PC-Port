#include "pc_p2_original_captain_rig.h"
#include "pc_p2_original_captain_render_policy.h"
#include <cassert>
#include <limits>
#include <iostream>
#include <type_traits>
// Engineering scope-lifetime control with the same friend boundary name;
// this stand-in is not an actual native section or a gameplay draw.
struct Graphics {};
struct GameCoreSection {
 static void scopeControl(){
  using p2original::captain::NativeViewScope;Graphics first,second;unsigned slot=77;
  assert(!NativeViewScope::current(first,slot)&&slot==77);
  {NativeViewScope outer(first,0);assert(NativeViewScope::current(first,slot)&&slot==0);
   slot=77;assert(!NativeViewScope::current(second,slot)&&slot==77);
   {NativeViewScope inner(first,1);assert(NativeViewScope::current(first,slot)&&slot==1);}
   assert(NativeViewScope::current(first,slot)&&slot==0);
   {NativeViewScope invalid(first,2);slot=77;assert(!NativeViewScope::current(first,slot)&&slot==77);}
   assert(NativeViewScope::current(first,slot)&&slot==0);
  }
  slot=77;assert(!NativeViewScope::current(first,slot)&&slot==77);
 }
};
static_assert(!std::is_constructible<p2original::captain::NativeViewScope,const Graphics&,unsigned>::value,"only native draw boundary constructs view scope");
namespace rig=p2original::captain::rig;
int main(){
 GameCoreSection::scopeControl();
 const rig::Matrix identity={1,0,0,0,0,1,0,0,0,0,1,0};
 const rig::Matrix turn={0,-1,0,0,1,0,0,0,0,0,1,0};
 std::array<rig::Matrix,11> draw;draw.fill(identity);
 rig::NormalBuffers history,otherView;p2pose::Vec out{};
 // Each viewCalc swaps destinations. A singular frame retains that buffer's
 // last value, two renders ago, regardless of elapsed animation frame count.
 assert(rig::renderNormals(draw,history));const auto first=history;
 draw[8]=turn;assert(rig::renderNormals(draw,history));
 draw[8].fill(0);assert(rig::renderNormals(draw,history));
 assert(rig::retainedNormal(history.buffer[history.active],8,{1,0,0},out)&&out.x==1&&out.y==0);
 assert(rig::renderNormals(draw,history));
 assert(rig::retainedNormal(history.buffer[history.active],8,{1,0,0},out)&&out.x==0&&out.y==1);
 // A different actual view cannot borrow the first view's prior destination.
 assert(rig::renderNormals(draw,otherView));
 assert(!rig::retainedNormal(otherView.buffer[otherView.active],8,{1,0,0},out));
 // Real view number, never a camera pointer, selects history. Two viewports
 // may share a camera; replacing one camera must preserve its view's buffers.
 std::array<rig::NormalBuffers,2> views;draw.fill(identity);
 assert(rig::renderNormals(draw,views[0]));draw[8]=turn;
 assert(rig::renderNormals(draw,views[1]));
 draw.fill(identity);assert(rig::renderNormals(draw,views[0]));draw[8]=turn;assert(rig::renderNormals(draw,views[1]));
 draw[8].fill(0);assert(rig::renderNormals(draw,views[0]));assert(rig::renderNormals(draw,views[1]));
 assert(rig::retainedNormal(views[0].buffer[views[0].active],8,{1,0,0},out)&&out.x==1&&out.y==0);
 assert(rig::retainedNormal(views[1].buffer[views[1].active],8,{1,0,0},out)&&out.x==0&&out.y==1);
 // Camera replacement only changes the current native compensation, not slot.
 const auto beforeReplacement=views[0];
 rig::Matrix replacedBasis=turn;std::array<float,9> replacedInverse;assert(rig::inverseLinear(replacedBasis,replacedInverse));
 p2pose::Vec retained{};assert(rig::retainedNormal(views[0].buffer[views[0].active],8,{1,0,0},retained));
 p2pose::Vec compensated={replacedInverse[0]*retained.x+replacedInverse[1]*retained.y,replacedInverse[3]*retained.x+replacedInverse[4]*retained.y,0};
 auto replacedLit=rig::point(replacedBasis,compensated);assert(p2pose::unit(replacedLit,out)&&out.x==1&&out.y==0);
 assert(views[0].active==beforeReplacement.active&&views[0].buffer[0].matrix==beforeReplacement.buffer[0].matrix&&views[0].buffer[1].matrix==beforeReplacement.buffer[1].matrix);
 // Failed finite validation does not swap or partially overwrite a buffer.
 const auto saved=history;draw[10][0]=std::numeric_limits<float>::infinity();
 assert(!rig::renderNormals(draw,history)&&history.active==saved.active);
 for(unsigned i=0;i<2;++i)assert(history.buffer[i].matrix==saved.buffer[i].matrix&&history.buffer[i].initialized==saved.buffer[i].initialized);
 // Retention is in VIEW space. Compensating the current native normal matrix
 // must reproduce the retained direction even when the camera/actor turns.
 rig::Model model;model.positions.push_back({8,{1,0,0}});model.normals.push_back({8,{1,0,0}});
 draw.fill(identity);draw[8].fill(0);rig::Matrix native={-2,0,0,0,0,-2,0,0,0,0,2,0};
 std::array<float,9> inverse;assert(rig::inverseLinear(native,inverse));p2pose::Pose pose;
 assert(rig::pose(model,draw,pose,&first.buffer[first.active],&inverse));
 auto object=pose.normals[0];assert(object.x==-1&&object.y==0);
 auto lit=rig::point(native,object);assert(p2pose::unit(lit,out)&&out.x==1&&out.y==0);
 rig::NormalBuffers empty;assert(!rig::pose(model,draw,pose,&empty.buffer[0],&inverse));
 native.fill(0);assert(!rig::inverseLinear(native,inverse));
 // Exact zero only: a finite small determinant still updates its destination.
 draw.fill(identity);draw[8][0]=1e-13f;assert(rig::renderNormals(draw,history));
 assert(history.buffer[history.active].matrix[8][0]>1e12f);
 // Engineering draw-policy control: mimic both DGX cached/new material
 // selection with dirty incoming scale, then apply its actual direct upload.
 struct Material {struct {bool mUseScale=false;} mTextureInfo;};
 Material materials[2];struct Shape {int mMaterialCount=2;Material* mMaterialList;};Shape shape{2,materials};
 struct Graphics {p2pose::Vec* mCustomScale=nullptr;};p2pose::Vec dirty={1,7,13};Graphics gfx{&dirty};
 assert(p2original::captain::unscaledMaterials(shape));
 p2original::captain::selectUnscaledMaterials(gfx);assert(!gfx.mCustomScale);
 // A selected unscaled material uploads the current view directly; no dirty
 // custom-scale compensation is included in the object normal direction.
 native={-2,0,0,0,0,-2,0,0,0,0,2,0};assert(rig::inverseLinear(native,inverse));model.normals[0].local={1,1,0};
 draw.fill(identity);draw[8].fill(0);assert(rig::pose(model,draw,pose,&first.buffer[first.active],&inverse));lit=rig::point(native,pose.normals[0]);assert(p2pose::unit(lit,out)&&std::fabs(out.x-out.y)<1e-6f&&out.x>0);
 materials[1].mTextureInfo.mUseScale=true;assert(!p2original::captain::unscaledMaterials(shape));
 std::cout<<"PASS captain_double_normal_buffers_and_view_compensation\n";
}
