#include "pc_p2_original_number_rigid.h"
#include <cmath>

// Primary dependencies (native/pikmin2-research):
// gameDynamics.cpp:53,103,299,793; dynCreature.cpp:143,596,667;
// pelletMgr.cpp:1297,1778; sysGCU/matMath.cpp:500; sysCommonU/sysMath.cpp:520,549;
// JSystem/JMath.h:219,270; trig.h:24. No native physics proxy or LOD guessing.
namespace p2originalnumber { namespace rigid {
namespace {
Vec3 add(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 sub(Vec3 a,Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 scale(Vec3 a,float b){return {a.x*b,a.y*b,a.z*b};}
float dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
Vec3 cross(Vec3 a,Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
bool finite(float a){return std::isfinite(a);}
bool finite(Vec3 a){return finite(a.x)&&finite(a.y)&&finite(a.z);}
bool finite(Quaternion a){return finite(a.w)&&finite(a.x)&&finite(a.y)&&finite(a.z);}
float norm(Quaternion q){return q.w*q.w+dot({q.x,q.y,q.z},{q.x,q.y,q.z});}
bool finite(const Matrix3& m){for(float f:m)if(!finite(f))return false;return true;}
bool finite(const Config& c){return finite(c.position)&&finite(c.velocity)&&finite(c.force)&&finite(c.rotatedMomentum)&&finite(c.momentum)&&finite(c.torque)&&finite(c.rotation)&&finite(norm(c.rotation))&&norm(c.rotation)>0&&finite(c.rotatedTransform);}
bool valid(const State& s){
 if(!s.initialized||!finite(s.current)||!finite(s.previous)||!finite(s.transformation)||!finite(s.timeStep)||s.timeStep<=0||!finite(s.baseRotation)||!finite(norm(s.baseRotation))||!(norm(s.baseRotation)>0)||!finite(s.basePosition)||!finite(s.particleOrigin)||!finite(s.transformedPosition))return false;
 for(const auto& p:s.particles)if(!finite(p.local)||!finite(p.position)||!finite(p.collisionNormal)||!finite(p.radius)||p.radius<0||(p.touching&&std::fabs(dot(p.collisionNormal,p.collisionNormal)-1.f)>.001f))return false;
 return true;
}
bool fail(std::string& e,const char* message){e=message;return false;}
bool valid(const Parameters& p){return finite(p.staticParameter)&&p.staticParameter>=0&&p.staticParameter<=5000&&finite(p.staticThreshold)&&p.staticThreshold>=0&&p.staticThreshold<=5000&&finite(p.microCollision)&&p.microCollision>=0&&p.microCollision<=10&&finite(p.elasticity)&&p.elasticity>=0&&p.elasticity<=1&&finite(p.fixedFrictionValue)&&p.fixedFrictionValue>=0&&p.fixedFrictionValue<=10000&&finite(p.rotatingMomentDamp)&&p.rotatingMomentDamp>=0&&p.rotatingMomentDamp<=1;}
float normalize(Vec3& v){float n=std::sqrt(dot(v,v));if(n>0)v=scale(v,1.f/n);return n;}
Quaternion multiply(Quaternion a,Quaternion b){Vec3 av{a.x,a.y,a.z},bv{b.x,b.y,b.z};Vec3 v=add(add(cross(av,bv),scale(bv,a.w)),scale(av,b.w));return {a.w*b.w-dot(av,bv),v.x,v.y,v.z};}
Quaternion add(Quaternion a,Quaternion b){return {a.w+b.w,a.x+b.x,a.y+b.y,a.z+b.z};}
Quaternion scale(Quaternion a,float s){return {s*a.w,s*a.x,s*a.y,s*a.z};}
Quaternion inverse(Quaternion q){float n=norm(q);return {q.w/n,-q.x/n,-q.y/n,-q.z/n};}
Quaternion normalize(Quaternion q){float inv=1.f/std::sqrt(norm(q));return {inv*q.w,inv*q.x,inv*q.y,inv*q.z};}
Matrix3 matrix(Quaternion q){
 float yy=2.f*q.y*q.y,zz=2.f*q.z*q.z,xx=2.f*q.x*q.x;
 float xy=2.f*q.x*q.y,xz=2.f*q.x*q.z,yz=2.f*q.y*q.z;
 float sz=2.f*q.w*q.z,sx=2.f*q.w*q.x,sy=2.f*q.w*q.y;
 return {{1.f-yy-zz,xy-sz,xz+sy,xy+sz,1.f-xx-zz,yz-sx,xz-sy,yz+sx,1.f-xx-yy}};
}
Vec3 transform(const Matrix3& m,Vec3 v){return {m[0]*v.x+m[1]*v.y+m[2]*v.z,m[3]*v.x+m[4]*v.y+m[5]*v.z,m[6]*v.x+m[7]*v.y+m[8]*v.z};}
Matrix3 transpose(const Matrix3& m){return {{m[0],m[3],m[6],m[1],m[4],m[7],m[2],m[5],m[8]}};}
Matrix3 multiply(const Matrix3& a,const Matrix3& b){Matrix3 out{};for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)out[r*3+c]=a[r*3]*b[c]+a[r*3+1]*b[3+c]+a[r*3+2]*b[6+c];return out;}
// Reproduce table initialization and integer lookup; host transcendental
// implementation and sqrt remain portable rather than PPC bit assertions.
float tableSin(unsigned index){return float(std::sin((double(index)*double(6.2831855f))/2048.));}
float tableCos(unsigned index){return float(std::cos((double(index)*double(6.2831855f))/2048.));}
Vec3 world(const State& s,Vec3 local){return add(transform(matrix(s.baseRotation),local),s.basePosition);}
Vec3 yVector(Quaternion q){Quaternion out=multiply(multiply(q,{0,0,1,0}),inverse(q));return {out.x,out.y,out.z};}
bool validTrace(const Trace& t){return finite(t.position)&&finite(t.velocity)&&finite(t.radius)&&t.radius>=0&&finite(t.restitution);}
}
bool initializeFive(State& out,Vec3 position,std::string& e){
 if(!finite(position))return fail(e,"Five initial position is nonfinite");
 State s;s.current.position=position;s.previous.position=position;s.basePosition=position;
 const float heightScaling=14.f/200.f,radSquared=(20.f/200.f)*(20.f/200.f);
 const float horizontal=radSquared/4.f+(heightScaling*heightScaling)/12.f,vertical=radSquared/2.f;
 s.transformation={{horizontal,0,0,0,vertical,0,0,0,horizontal}};
 for(unsigned i=0;i<4;++i){float theta=(6.2831855f/4.f)*float(i);unsigned idx=unsigned(int(theta*325.9493f))&0x7ffu;s.particles[i].local={13.f*tableSin(idx),0,13.f*tableCos(idx)};s.particleOrigin=add(s.particleOrigin,s.particles[i].local);s.particles[i].position=add(position,s.particles[i].local);}
 s.particleOrigin=scale(s.particleOrigin,1.f/4.f);s.transformedPosition=world(s,s.particleOrigin);s.initialized=true;
 if(!valid(s))return fail(e,"Five initialization produced invalid state");
 out=s;e.clear();return true;
}
bool setBaseTransform(State& out,Quaternion rotation,Vec3 position,std::string& e){
 if(!valid(out)||!finite(rotation)||!finite(norm(rotation))||!(norm(rotation)>0)||!finite(position))return fail(e,"Five base transform is invalid");
 State s=out;s.baseRotation=rotation;s.basePosition=position;s.transformedPosition=world(s,s.particleOrigin);
 for(auto& p:s.particles)p.position=world(s,p.local);
 if(!valid(s))return fail(e,"Five base transform produced invalid state");
 out=s;e.clear();return true;
}
bool computeForces(State& out,const Parameters& p,bool applyFriction,std::string& e){
 if(!valid(out)||!valid(p))return fail(e,"Five force input is invalid");
 State s=out;auto& c=s.current;c.force={};c.torque={};
 if(p.rotatingMomentDamp>0)c.momentum=sub(c.momentum,scale(c.momentum,p.rotatingMomentDamp));
 if(applyFriction){
  if(p.newFriction){
   for(const auto& particle:s.particles)if(particle.touching){
    Vec3 sep=sub(particle.position,s.transformedPosition);
    Vec3 vel=add(cross(c.rotatedMomentum,sep),c.velocity);
    float normalVelocity=dot(vel,particle.collisionNormal),normalForce=dot(c.force,particle.collisionNormal);
    Vec3 tangent=sub(vel,scale(particle.collisionNormal,normalVelocity));normalize(tangent);
    c.force=add(c.force,scale(particle.collisionNormal,normalForce));
    if(std::fabs(dot(tangent,vel))<p.staticThreshold){normalize(tangent);c.force=sub(c.force,scale(tangent,p.staticParameter));}
    else{Vec3 dir=sub(vel,scale(particle.collisionNormal,dot(vel,particle.collisionNormal)));normalize(dir);c.force=add(c.force,scale(dir,-p.fixedFrictionValue));}
   }
  }else{
   unsigned count=0;for(const auto& particle:s.particles)if(particle.touching)++count;
   if(count&&p.friction){float coefficient=-(p.fixedFriction?p.fixedFrictionValue:.9f)*(float(count)/4.f);
    for(const auto& particle:s.particles)if(particle.touching){Vec3 sep=sub(particle.position,s.transformedPosition),vel=add(cross(c.rotatedMomentum,sep),c.velocity),tangent=sub(vel,scale(particle.collisionNormal,dot(vel,particle.collisionNormal)));if(p.frictionTangentVelocity)normalize(tangent);Vec3 force=scale(tangent,coefficient);c.force=add(c.force,force);if(!p.noRotationEffect)c.torque=add(c.torque,cross(sep,force));}
   }
  }
 }
 if(!valid(s))return fail(e,"Five forces produced invalid state");
 out=s;e.clear();return true;
}
bool integrate(State& out,float dt,std::string& e){
 if(!valid(out)||!finite(dt)||dt<0)return fail(e,"Five integration input is invalid");
 State s=out;auto& c=s.current;s.previous.position=c.position;s.previous.rotation=c.rotation;
 Matrix3 rotation=matrix(c.rotation);c.rotatedTransform=multiply(multiply(rotation,s.transformation),transpose(rotation));
 c.position=add(c.position,scale(c.velocity,dt));c.momentum=add(c.momentum,scale(c.torque,dt));c.velocity=add(c.velocity,scale(c.force,dt*s.timeStep));
 c.rotatedMomentum=transform(c.rotatedTransform,c.momentum);
 Quaternion derivative=multiply({0,c.rotatedMomentum.x,c.rotatedMomentum.y,c.rotatedMomentum.z},c.rotation),halfTime=scale(derivative,.5f*dt),candidate=add(c.rotation,halfTime);
 if(s.limitTilt){Vec3 before=yVector(c.rotation),after=yVector(candidate);float low=tableSin(8192u>>5),high=tableSin(10912u>>5);
  if(before.y<high){if(after.y<before.y){c.momentum=add(c.momentum,scale(cross(before,{0,1,0}),1000.f));c.rotatedMomentum=transform(c.rotatedTransform,c.momentum);if(!(after.y<low))c.rotation=candidate;}else c.rotation=candidate;}
  else c.rotation=add(c.rotation,halfTime);
 }else c.rotation=add(c.rotation,halfTime);
 c.rotation=normalize(c.rotation);
 if(!valid(s))return fail(e,"Five integration produced invalid state");
 out=s;e.clear();return true;
}
CollisionResult resolveCollision(State& out,Vec3 point,Vec3 normal,float restitution,std::string& e){
 if(!valid(out)||!finite(point)||!finite(normal)||!finite(restitution)||restitution<0||restitution>1||std::fabs(dot(normal,normal)-1.f)>.001f){fail(e,"Five collision input is invalid");return CollisionResult::Invalid;}
 State s=out;auto& c=s.current;Vec3 delta=sub(point,c.position),contactVelocity=scale(add(c.velocity,cross(c.rotatedMomentum,delta)),-1.f);
 float magnitude=dot(contactVelocity,normal);
 if(magnitude<0){e.clear();return CollisionResult::Separating;}
 if(std::fabs(magnitude)<=0){restitution=1;magnitude=0;}
 float denominator=s.timeStep;magnitude=-(1.f+restitution)*magnitude;
 Vec3 scratch=cross(delta,normal);scratch=transform(c.rotatedTransform,scratch);scratch=cross(scratch,delta);denominator+=dot(normal,scratch);
 if(!finite(denominator)||denominator<=0){fail(e,"Five collision denominator is invalid");return CollisionResult::Invalid;}
 Vec3 impulse=scale(normal,-(magnitude/denominator));c.velocity=add(c.velocity,scale(impulse,s.timeStep));c.momentum=add(c.momentum,cross(delta,impulse));c.rotatedMomentum=transform(c.rotatedTransform,c.momentum);
 if(!valid(s)){fail(e,"Five collision produced invalid state");return CollisionResult::Invalid;}out=s;e.clear();return CollisionResult::Accepted;
}
bool simulateHalfStep(State& out,float dt,const Parameters& params,TraceProvider* trace,StepReport& report,std::string& e){
 if(!trace||!valid(out)||!valid(params)||!finite(dt)||dt<0)return fail(e,"Five halfstep input/trace is unavailable");
 State s=out;StepReport r;s.canBounce=s.hasCollided;s.hasCollided=false;s.transformedPosition=world(s,s.particleOrigin);
 if(!integrate(s,dt,e))return false;
 class Receiver final:public ContactReceiver {
  State& state;Particle& particle;StepReport& report;float restitution;
 public:bool faulted=false;
  Receiver(State& s,Particle& p,StepReport& r,float rest):state(s),particle(p),report(r),restitution(rest){}
  bool contact(Vec3 point,Vec3 normal,std::string& error)override{CollisionResult result=resolveCollision(state,point,normal,restitution,error);if(result==CollisionResult::Invalid){faulted=true;return false;}if(result==CollisionResult::Accepted){if(!state.canBounce)++report.bounceCallbacks;state.hasCollided=true;particle.touching=true;particle.collisionNormal=normal;++report.acceptedContacts;}return true;}
 };
 for(auto& particle:s.particles){
  particle.position=world(s,particle.local);Vec3 velocity=add(cross(s.current.rotatedMomentum,sub(particle.position,s.transformedPosition)),s.current.velocity);
  float extra=dt*std::sqrt(dot(velocity,velocity));if(extra>50)extra=50;
  particle.touching=false;Trace request{particle.position,velocity,particle.radius+extra,1,true};Receiver receiver(s,particle,r,params.elasticity);
  if(!validTrace(request))return fail(e,"Five particle trace is invalid");
  const float sourceRadius=request.radius;
  if(!trace->traceMap(request,dt,receiver,e))return false;
  if(receiver.faulted)return fail(e,"Five map trace ignored invalid contact");
  if(!validTrace(request)||request.radius!=sourceRadius||request.restitution!=1||!request.hardIntersect)return fail(e,"Five map trace returned invalid state/policy");
  request.hardIntersect=false;
  if(!trace->tracePlatforms(request,dt,receiver,e))return false;
  if(receiver.faulted)return fail(e,"Five platform trace ignored invalid contact");
  if(!validTrace(request)||request.radius!=sourceRadius||request.restitution!=1||request.hardIntersect)return fail(e,"Five platform trace returned invalid state/policy");
  // Retail intentionally does not copy traced sphere position/velocity back
  // to particle or rigid state; only callback impulses affect the rigid body.
 }
 if(!valid(s))return fail(e,"Five halfstep produced invalid state");
 out=s;report=r;e.clear();return true;
}
}}
