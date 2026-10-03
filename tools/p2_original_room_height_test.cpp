#include "pc_p2_original_room_height.h"
#include "pc_p2_original_number_triangle.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace H=p2originalnumber::roomHeight;
static unsigned controls=0;
static void check(bool x,const char* why){++controls;if(!x)throw std::runtime_error(why);}
static float f(std::uint32_t b){float x;std::memcpy(&x,&b,4);return x;}
static std::uint32_t bits(float x){std::uint32_t b;std::memcpy(&b,&x,4);return b;}
static bool same(H::Vec3 a,H::Vec3 b){return bits(a.x)==bits(b.x)&&bits(a.y)==bits(b.y)&&bits(a.z)==bits(b.z);}
static bool same(const H::Query&a,const H::Query&b){return same(a.position,b.position)&&same(a.normal,b.normal)&&bits(a.minY)==bits(b.minY)&&bits(a.maxY)==bits(b.maxY)&&a.triangle.original==b.triangle.original&&a.triangle.index==b.triangle.index&&a.triangle.roomIndex==b.triangle.roomIndex&&a.table==b.table&&a.getFullInfo==b.getFullInfo&&a.updateOnNewMaxY==b.updateOnNewMaxY;}
static int tokens[16];
static std::array<std::uint32_t,16> plane(float nx,float ny,float nz,float offset){std::array<std::uint32_t,16> p{};p[0]=bits(nx);p[1]=bits(ny);p[2]=bits(nz);p[3]=bits(offset);return p;}
struct Owner final:H::Owner{
 unsigned calls=0,expire=0;bool available=true,hiddenAvailable=true;H::Hidden floor;
 bool current(const std::vector<H::Room>&,std::string&e)override{++calls;if(!available||calls==expire){e="fixture owner expired";return false;}return true;}
 bool hidden(H::Hidden&out,std::string&e)override{if(!hiddenAvailable){e="fixture hidden flag unavailable";return false;}out=floor;return true;}
};
static H::UnitGrid grid(std::initializer_list<float> heights){H::UnitGrid g;g.minimum={0,0,0};g.maximum={20,10,20};g.maxX=g.maxZ=2;g.cells.resize(4);unsigned n=0;for(float y:heights){g.triangles.push_back({plane(0,1,0,y),&tokens[n]});g.cells[0].push_back(n++);}return g;}
static H::Room room(const H::UnitGrid&g){H::Room r;r.unit=&g;r.inverse={1,0,0,0,0,1,0,0,0,0,1,0};r.sphereRadius=1000;r.roomIndex=19;return r;}
static H::Query query(){H::Query q;q.position={1,99,1};return q;}
static void projection(){
 std::string e;bool accepted=false;
 // Literal goldens independently calculated with exact Fraction binary32
 // decoding and nearest-even rounding after each source instruction. No
 // expected value calls the production arithmetic or host fmaf.
 struct Golden{float nx,ny,nz,d,x,z;std::uint32_t y;};
 const Golden cases[]={{15,.75f,12.5f,1.25f,f(0xbf3d43b2),-f(0x3ec90328),0x41b7f877},{f(0x3f123456),1.25f,-.625f,7.375f,1.875f,-3.5f,0x4052c5fa},{1,3,1,17,16777216,-16777216,0x40b55555}};
 for(auto c:cases){auto p=plane(c.nx,c.ny,c.nz,c.d);H::Vec3 v{c.x,99,c.z};check(H::insideXZ(p,v,accepted,e)&&accepted,"raw projection accepted");check(bits(v.y)==c.y,"independent projection bits");check(bits(v.x)==bits(c.x)&&bits(v.z)==bits(c.z),"projection retains XZ");p[7]=bits(-1);v.y=99;check(H::insideXZ(p,v,accepted,e)&&!accepted&&bits(v.y)==c.y,"edge miss publishes projected Y");}
 for(float ny:{0.f,-0.f,-1.f}){auto p=plane(0,ny,0,37);H::Vec3 v{1,99,2},before=v;accepted=true;check(H::insideXZ(p,v,accepted,e)&&!accepted&&same(v,before),"nonpositive ny retains point");}
 for(unsigned mode=0;mode<3;++mode){auto p=plane(0,1,0,37);H::Vec3 v{2,99,2},before=v;accepted=true;if(mode==0)p[9]=0x7fc00000;if(mode==1){p[0]=0x7f7fffff;}if(mode==2){p[4]=0x7f7fffff;}check(!H::insideXZ(p,v,accepted,e)&&accepted&&same(v,before),"projection failure atomic");}
}
static void selection(){
 std::string e;auto g=grid({2,7});auto r=room(g);std::vector<H::Room> rooms{r};Owner owner;auto q=query();q.updateOnNewMaxY=false;q.table=&tokens[15];q.getFullInfo=true;
 check(H::query(rooms,owner,q,e)&&q.minY==7&&q.maxY==2&&q.triangle.original==&tokens[1],"highest selector");check(q.table==&tokens[15]&&q.getFullInfo&&q.position.y==99,"unused inputs retained");
 owner=Owner{};q=query();check(H::query(rooms,owner,q,e)&&q.triangle.original==&tokens[0]&&q.minY==7&&q.maxY==2,"lowest selector");
 g=grid({7,7});g.cells[0]={1,0,1};owner=Owner{};q=query();q.updateOnNewMaxY=false;check(H::query(rooms,owner,q,e)&&q.triangle.original==&tokens[1],"ordered first tie");check(owner.calls==8,"duplicate index traversal retained");
 g=grid({37});owner=Owner{};q=query();check(H::query(rooms,owner,q,e)&&q.minY==37&&q.maxY==37,"raw offset authoritative without vertex reconstruction");
 g.cells[0].clear();owner=Owner{};q=query();q.normal={2,3,4};check(H::query(rooms,owner,q,e)&&q.minY==0&&q.maxY==0&&same(q.normal,{2,3,4}),"fresh no hit zero retains normal");
 owner=Owner{};q=query();q.triangle={&tokens[9],42,71};q.minY=123;q.normal={2,3,4};check(H::query(rooms,owner,q,e)&&q.minY==123&&q.maxY==328000&&q.triangle.original==&tokens[9]&&same(q.normal,{2,3,4}),"no hit incoming identity and min retained");
 g=grid({-4});owner=Owner{};owner.floor={true,{&tokens[10],77,-3}};q=query();check(H::query(rooms,owner,q,e)&&q.maxY==0&&q.minY==0&&q.triangle.original==&tokens[10]&&same(q.normal,{0,1,0}),"hidden negative floor retains selected normal");
 g=grid({7});owner=Owner{};q=query();q.updateOnNewMaxY=false;q.minY=10;q.triangle={&tokens[9],42,71};q.normal={2,3,4};check(H::query(rooms,owner,q,e)&&q.maxY==7&&q.minY==10&&q.triangle.original==&tokens[9]&&same(q.normal,{2,3,4}),"highest selector respects incoming minimum");
 g=grid({-200000});owner=Owner{};q=query();q.updateOnNewMaxY=false;check(H::query(rooms,owner,q,e)&&q.maxY==0&&q.minY==0&&!q.triangle.original,"default min sentinel can leave no selected identity");
 owner=Owner{};q=query();check(H::query(rooms,owner,q,e)&&q.maxY==-200000&&q.minY==-128000&&q.triangle.original==&tokens[0],"lowest selector preserves default min sentinel");
 g=grid({2,7});owner=Owner{};float highest=-999;check(H::minY(rooms,owner,{1,-900,1},highest,e)&&highest==7,"Room minY returns highest independent of input Y");
 owner=Owner{};owner.expire=7;highest=-999;check(!H::minY(rooms,owner,{1,0,1},highest,e)&&highest==-999,"minY final owner expiry atomic");
 for(unsigned mode=0;mode<2;++mode){owner=Owner{};if(mode==0)owner.floor.enabled=true;else owner.hiddenAvailable=false;q=query();auto before=q;check(!H::query(rooms,owner,q,e)&&same(q,before),"missing hidden prerequisite refuses atomically");}
}
static void gridsAndGates(){
 std::string e;auto g=grid({11,22,33,44});for(unsigned i=0;i<4;++i)g.cells[i]={i};auto r=room(g);std::vector<H::Room> rooms{r};
 for(auto test: {std::pair<H::Vec3,float>{{-50,99,100},22},{{100,99,-50},33},{{100,99,100},44},{{-50,99,-50},11}}){Owner owner;auto q=query();q.position=test.first;check(H::query(rooms,owner,q,e)&&q.minY==test.second,"original local cells clamp and z+x*maxZ");}
 g=grid({7});for(auto&c:g.cells)c={0};r=room(g);r.sphereRadius=5;rooms={r};Owner owner;auto q=query();q.position={3,100000,4};check(H::query(rooms,owner,q,e)&&q.minY==7,"XZ gate inclusive ignores Y");owner=Owner{};q=query();q.position={std::nextafter(3.f,4.f),99,4};check(H::query(rooms,owner,q,e)&&q.minY==0,"XZ gate nextUp excludes");
 // Deliberately non-orthonormal actual inverse: local=(2x,3y,4z).
 // Literal face is nonunit; normal uses inverse transpose without normalization.
 g.triangles[0].planeBits=plane(2,3,4,60);r=room(g);r.inverse={2,0,0,0,0,3,0,0,0,0,4,0};rooms={r};owner=Owner{};q=query();check(H::query(rooms,owner,q,e)&&q.minY==float(40.f/3.f)&&same(q.normal,{4,9,16}),"paired inverse point and unnormalized transpose normal");
 // Fraction/RNE oracle for nontrivial paired lanes and nested normal FMAs.
 const std::uint32_t m[12]={0x3f123456,0x3f876543,0xbeabc123,0x40123456,0x3fa12345,0x3fc23456,0x3e654321,0xbf123456,0xbf654321,0x3e123456,0x3f876543,0x3f123456};
 for(unsigned i=0;i<12;++i){r.inverse[i]=f(m[i]);}
 g.triangles[0].planeBits=plane(f(0x3f123456),f(0x3fc23456),f(0x3e654321),48);rooms={r};owner=Owner{};q=query();q.position={f(0x3f56789a),f(0xbfa12345),f(0x3f234567)};
 const bool transformed=H::query(rooms,owner,q,e);
 if(!transformed||bits(q.minY)!=0x41f90e3b){std::cerr<<"paired height actual="<<std::hex<<bits(q.minY)<<std::dec<<" error="<<e<<'\n';}
 check(transformed&&bits(q.minY)==0x41f90e3b,"independent paired transform height bits");check(bits(q.normal.x)==0x40024888&&bits(q.normal.y)==0x403c08d1&&bits(q.normal.z)==0x3ec51591,"independent transpose normal bits");
 auto second=grid({48});second.triangles[0].original=&tokens[7];g=grid({48});r=room(g);auto r2=room(second);r2.roomIndex=27;rooms={r2,r};owner=Owner{};q=query();check(H::query(rooms,owner,q,e)&&q.triangle.original==&tokens[7]&&q.triangle.roomIndex==27,"ordered room tie retains first room");rooms={r,r2};owner=Owner{};q=query();check(H::query(rooms,owner,q,e)&&q.triangle.original==&tokens[0]&&q.triangle.roomIndex==19,"room roster order observable");
}
static void refusal(){
 std::string e;auto g=grid({2,7});auto r=room(g);std::vector<H::Room> rooms{r};
 for(unsigned n:{1u,4u,5u,7u}){Owner owner;owner.expire=n;auto q=query(),before=q;check(!H::query(rooms,owner,q,e)&&same(q,before)&&e=="fixture owner expired","owner expiry failure atomic");}
 for(unsigned mode=0;mode<6;++mode){auto bad=g;auto rr=room(bad);if(mode==0)bad.triangles[1].planeBits[0]=0x7fc00000;if(mode==1)bad.cells[0]={0,999};if(mode==2)bad.triangles[1].original=nullptr;if(mode==3)bad.maxX=0;if(mode==4)rr.sphereRadius=std::numeric_limits<float>::max();if(mode==5)rr.inverse[0]=std::numeric_limits<float>::max();Owner owner;auto q=query();if(mode==5)q.position.x=2;auto before=q;check(!H::query({rr},owner,q,e)&&same(q,before),"borrow/arithmetic failure rolls back earlier winner");}
 Owner owner;owner.available=false;auto q=query(),before=q;r.unit=nullptr;check(!H::query({r},owner,q,e)&&same(q,before)&&e=="fixture owner expired","expiry before unavailable borrow");
}
static void bounding(){
 // Research HEAD632af937 sysMath.cpp1039/1054, asm80412F74/8041303C.
 // cpp SHA36394b55708791273e38bb7e8f44d4f9eb7ac5060ebd08f286781980de400c99;
 // asm SHAe0957c1da72982c6fe961fda7821879f87a92ae020be3d7bc0b1b92d62819556.
 std::string e;auto m=room(grid({1})).inverse;H::Bounds out{{9,8,7},{6,5,4}};
 const H::Vec3 d{f(0x3fdcbb8d),f(0x40a32c5a),f(0x40434c28)};
 H::Sphere sphere{{9,8,7},6};H::Bounds b{{-d.x,-d.y,-d.z},d};
 // Independent Fraction/RNE: qdist3 squared421925f7, Vector squared421925f8.
 // Pinned hardware coefficient interval3 integer interpolation12bef57 yields
 // radius40c60198 vs40c6019a. This is emulator-table evidence, not hardware.
 check(H::boundSphere(b,sphere,e)&&bits(sphere.radius)==0x40c60198&&same(sphere.center,{0,0,0}),"qdist3 distinct FMA order golden");
 float length=0;check(p2originalnumber::triangle::sourceVectorLength(d,length)&&bits(length)==0x40c6019a&&bits(length)!=bits(sphere.radius),"VectorLength cannot substitute qdist3");
 const std::uint32_t mb[12]={0x3f123456,0x3f876543,0xbeabc123,0x40123456,0x3fa12345,0x3fc23456,0x3e654321,0xbf123456,0xbf654321,0x3e123456,0x3f876543,0x3f123456};
 for(unsigned i=0;i<12;++i){m[i]=f(mb[i]);}
 b={{-1,-2,-3},{4,5,6}};check(H::transformBounds(b,m,out,e),"eight corner transform available");
 check(same(out.minimum,{f(0xc01a8edd),f(0xc0b127d2),f(0xc0cf0a3e)})&&same(out.maximum,{f(0x412dd390),f(0x41564d5e),f(0x41086f81)}),"independent eight corner paired goldens");
 m=room(grid({1})).inverse;b={{40000,40001,40002},{50000,50001,50002}};check(H::transformBounds(b,m,out,e)&&same(out.minimum,{32768,32768,32768})&&same(out.maximum,b.maximum),"positive SHORT_FLOAT_MAX sentinel retained");
 b={{-50000,-50001,-50002},{-40000,-40001,-40002}};check(H::transformBounds(b,m,out,e)&&same(out.maximum,{-32768,-32768,-32768})&&same(out.minimum,b.minimum),"negative SHORT_FLOAT_MAX sentinel retained");
 b={{-1,-1000,0},{1,2000,0}};check(H::roomBounds(b,m,out,sphere,e)&&same(out.minimum,b.minimum)&&same(out.maximum,b.maximum),"full bounds retained separately from flat copy");check(bits(sphere.center.y)==0&&bits(sphere.radius)==0x3f7ff400,"room _190 zero Y copy before raw sphere");check(b.minimum.y==-1000&&b.maximum.y==2000,"serialized bounds input unchanged");
 b={{0,0,0},{0,0,0}};check(H::boundSphere(b,sphere,e)&&bits(sphere.radius)==0,"zero sphere source branch");
 for(unsigned mode=0;mode<3;++mode){b={{1,2,3},{4,5,6}};m=room(grid({1})).inverse;out={{9,8,7},{6,5,4}};sphere={{9,8,7},6};const auto before=out;const auto prior=sphere;if(mode==0)b.maximum.x=std::numeric_limits<float>::quiet_NaN();if(mode==1)m[0]=std::numeric_limits<float>::max();if(mode==2){b.minimum.x=b.maximum.x=std::numeric_limits<float>::max();}check(!H::roomBounds(b,m,out,sphere,e)&&same(out.minimum,before.minimum)&&same(out.maximum,before.maximum)&&same(sphere.center,prior.center)&&bits(sphere.radius)==bits(prior.radius),"bounds and sphere failure atomic");}
}
struct Provider final:H::HeightProvider{
 std::vector<H::Vec3> samples;float height=0;unsigned failAt=0;
 bool minY(H::Vec3 v,float&out,std::string&e)override{samples.push_back(v);if(samples.size()==failAt){e="fixture unavailable";return false;}out=height;return true;}
};
static void routes(){
 // Fraction/RNE oracle: repeated rounded .1 increment, separate mul/add.
 const std::uint32_t expected[10][2]={{0x3f123456,0xc0523456},{0x4021ce64,0x3fdb3160},{0x408f87d9,0x40d6b2db},{0x40ce2881,0x413b4cae},{0x41066494,0x41859ff8},{0x4125b4e7,0x41ad9998},{0x4145053b,0x41d59339},{0x4164558f,0x41fd8cdb},{0x4181d2f2,0x4212c33e},{0x41917b1c,0x4226c00f}};
 H::Vec3 a{f(0x3f123456),0,f(0xc0523456)},b{f(0x41a12345),100,f(0x423abcde)};Provider p;std::string e;bool out=false;
 check(H::linkable(p,a,b,out,e)&&out&&p.samples.size()==10,"ten samples no endpoint");for(unsigned i=0;i<10;++i){check(bits(p.samples[i].x)==expected[i][0]&&bits(p.samples[i].z)==expected[i][1],"independent route sample bits");check(bits(p.samples[i].y)==0,"route ignores endpoint Y");}
 check(!same(p.samples.back(),b),"endpoint absent");
 p=Provider{};p.height=25;out=false;check(H::linkable(p,{0,0,0},{1,900,1},out,e)&&out&&p.samples.size()==10,"strict 25 passes");p=Provider{};p.height=std::nextafter(25.f,26.f);out=true;check(H::linkable(p,{0,0,0},{1,0,1},out,e)&&!out&&p.samples.size()==1,"nextUp25 fails");
 for(unsigned n:{1u,5u,10u}){p=Provider{};p.failAt=n;out=false;check(!H::linkable(p,a,b,out,e)&&!out&&p.samples.size()==n,"unavailable preserves result");}
 p=Provider{};p.height=std::numeric_limits<float>::quiet_NaN();out=true;check(!H::linkable(p,a,b,out,e)&&out,"nonfinite provider failure atomic");p=Provider{};out=true;check(H::linkable(p,{0,26,0},{1,-900,1},out,e)&&!out,"genuine no hit zero participates in threshold");
}
int main(){try{projection();selection();gridsAndGates();refusal();bounding();routes();std::cout<<"room_height controls="<<controls<<" actual_world=0 gameplay=0 save=0\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL after "<<controls<<": "<<e.what()<<'\n';return 1;}}
