#include "pc_p2_original_captain_motion.h"
#include "pc_randomizer.h"
#include "pc_p2_pose_bank.h"
#include "pc_p2_pose_loader.h"
#include "pc_p2_original_pelplant_geometry.h"
#include "netplay/pc_netplay_sha256.h"
#include "Navi.h"
#include "NaviMgr.h"
#include "Graphics.h"
#include "Camera.h"
#include "Stream.h"
#include "sysNew.h"
#include "Texture.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <regex>
#include <sstream>
#include <locale>
namespace p2original { namespace captain {
namespace {
bool fail(std::string& e,const char* why){e=why;return false;}
std::string hash(const std::string& b){unsigned char h[32];pc_netplay_sha::sha256(b.data(),b.size(),h);std::string s;const char* x="0123456789abcdef";for(auto v:h){s+=x[v>>4];s+=x[v&15];}return s;}
bool get(const std::string& role,std::string& b,std::string& e){return pc_randomizer_original_input("p2-original/captains/"+role,b,e)&&b.size()<=1024*1024;}
unsigned be16(const std::string& b,std::size_t at){return (unsigned char)b[at]*256+(unsigned char)b[at+1];}
struct Heap {int previous;Heap():previous(gsys->setHeap(SYSHEAP_App)){}~Heap(){gsys->setHeap(previous);}};
struct Key {int frame,type;bool operator==(const Key& b)const{return frame==b.frame&&type==b.type;}};
struct Clip {int duration=0;std::vector<Key> keys;std::vector<int> frames;std::vector<p2pose::Pose> poses;};
const unsigned ids[]={1,4,10,14,22,23,28,29,30,31,33,34,1000};
const char* names[]={"asibumi","damage","fue","getup","jhit","jkoke","nigeru","run2","walk","wait","throw","trwwait","down"};
const char* models[]={"orima1","orima3","syatyou"};
bool locomotion(Motion m){return m==Motion::Walk||m==Motion::Run2||m==Motion::Nigeru;}
struct Model {Shape* shape=nullptr;std::map<unsigned,Clip> clips;std::string first;std::vector<unsigned char> topology;};
struct Actor {Model* model=nullptr;std::unique_ptr<pelplant::Geometry> geometry;MotionState state;std::size_t next=0;bool down=false,advancing=false;float downFrame=0;};
std::vector<std::vector<std::string>> registry(const std::string& b){std::string clean;bool comment=false;for(char c:b){if(c=='#')comment=true;if(c=='\n')comment=false;if(!comment)clean+=c;}std::istringstream in(clean);in.imbue(std::locale::classic());int count;std::string word;std::vector<std::vector<std::string>> out;if(!(in>>count)||count!=67)return out;for(int i=0;i<count;++i){if(!(in>>word)||word!="{")return {};std::vector<std::string> row;while(in>>word&&word!="}")row.push_back(word);if(word!="}"||row.size()<3||row.back()!="-1")return {};out.push_back(std::move(row));}if(in>>word)return {};return out;}
bool number(const std::string& b,const char* key,float& out){std::regex pattern(std::string("\\{")+key+"\\}\\s+4\\s+([-+0-9.eE]+)");auto begin=std::sregex_iterator(b.begin(),b.end(),pattern);if(begin==std::sregex_iterator())return false;auto match=*begin;if(++begin!=std::sregex_iterator())return false;std::istringstream in(match[1].str());in.imbue(std::locale::classic());std::string extra;return bool(in>>out)&&!(in>>extra)&&std::isfinite(out);}
}
struct SourceBank::Impl {
 bool prepared=false,bound=false;std::uint64_t generation=0;std::string fingerprint;std::array<std::string,5> sources;SourceParameters parameters;
 std::array<Model,3> model;std::map<const Navi*,Actor> actors;
 bool sample(Actor& a,std::string& e){unsigned id=a.down?1000:unsigned(a.state.motion);auto c=a.model->clips.find(id);if(c==a.model->clips.end()||!a.geometry)return fail(e,"source captain clip/geometry absent");const auto& clip=c->second;float frame=a.down?a.downFrame:a.state.frame;frame=std::min(frame,float(clip.duration-1));auto upper=std::upper_bound(clip.frames.begin(),clip.frames.end(),frame);std::size_t r=upper==clip.frames.end()?clip.frames.size()-1:std::size_t(upper-clip.frames.begin()),l=r?r-1:0;float w=l==r?0:(frame-clip.frames[l])/float(clip.frames[r]-clip.frames[l]);p2pose::Pose pose;pose.positions.resize(clip.poses[l].positions.size());pose.normals.resize(clip.poses[l].normals.size());for(std::size_t i=0;i<pose.positions.size();++i)pose.positions[i]=p2pose::mix(clip.poses[l].positions[i],clip.poses[r].positions[i],w);for(std::size_t i=0;i<pose.normals.size();++i){auto n=p2pose::mix(clip.poses[l].normals[i],clip.poses[r].normals[i],w);if(!p2pose::unit(n,pose.normals[i]))pose.normals[i]={0,0,0};}if(!p2pose::write(a.geometry->shape,pose))return fail(e,"source captain pose write failed");e.clear();return true;}
};
SourceBank::SourceBank():m(new Impl){}SourceBank::~SourceBank()=default;
bool SourceBank::prepare(std::string& e){
 if(m->prepared)return true;if(!pc_randomizer_original_session()||!gsys)return fail(e,"source captain bank requires actual selected session/system");
 auto next=std::make_unique<Impl>();const char* sourceRoles[]={"naviParms.txt","animmgr.txt","navicoll.txt","down/demo.stb","down/s03_dead1.bck"};for(unsigned i=0;i<5;++i)if(!get(sourceRoles[i],next->sources[i],e))return false;
 if(!number(next->sources[0],"p050",next->parameters.maximumHealth)||!number(next->sources[0],"p004",next->parameters.moveSpeed)||!number(next->sources[0],"p043",next->parameters.neutralStick)||!number(next->sources[0],"p044",next->parameters.cursorStick)||next->parameters.maximumHealth<=0||next->parameters.moveSpeed<=0||next->parameters.neutralStick<0||next->parameters.cursorStick<=next->parameters.neutralStick||next->parameters.cursorStick>1)return fail(e,"source captain parameters invalid");next->parameters.rawSourceSha=hash(next->sources[0]);
 auto rows=registry(next->sources[1]);if(rows.size()!=67)return fail(e,"source captain registry malformed");
 std::string bytes;if(!get("bank.txt",bytes,e))return false;std::string closure=bytes;for(const auto& b:next->sources)closure+=hash(b);std::istringstream in(bytes);in.imbue(std::locale::classic());std::string word,registrySha;int nmodel,nclip;if(!(in>>word>>registrySha>>nmodel>>nclip)||word!="P2_SOURCE_CAPTAIN_BANK_1"||registrySha!=hash(next->sources[1])||nmodel!=3||nclip!=13)return fail(e,"source captain bank header invalid");
 std::size_t total=0;
 for(unsigned j=0;j<3;++j){auto& model=next->model[j];std::string name,expected,raw;if(!(in>>word>>name>>expected)||word!="model"||name!=models[j]||!get("models/"+name+".bmd",raw,e)||hash(raw)!=expected)return fail(e,"source captain model source mismatch");closure+=expected;
  for(unsigned k=0;k<13;++k){unsigned id;int duration,count;std::string clipName,sourceSha;if(!(in>>word>>id>>clipName>>duration>>sourceSha>>count)||word!="clip"||id!=ids[k]||clipName!=names[k]||duration<1||duration>10000||count<0||count>4096)return fail(e,"source captain clip index invalid");Clip clip;clip.duration=duration;for(int v=0;v<count;++v){Key key;if(!(in>>key.frame>>key.type)||key.frame<0||key.frame>=duration||key.type<0||key.type>=1000||(!clip.keys.empty()&&key.frame<clip.keys.back().frame))return fail(e,"source captain event invalid");clip.keys.push_back(key);}
   if(id==1000){raw=next->sources[4];if(!clip.keys.empty()||raw.size()<72||raw.compare(0,8,"J3D1bck1")||raw.compare(32,4,"ANK1"))return fail(e,"source captain Down BCK invalid");}
   else {if(!get("motion/"+clipName+".bca",raw,e)||raw.size()<72||raw.compare(0,8,"J3D1bca1")||raw.compare(32,4,"ANF1"))return fail(e,"source captain BCA invalid");const auto& row=rows[id];if(row[1]!=clipName+".bca"||(row.size()-3)%2)return fail(e,"source captain source registry identity mismatch");std::vector<Key> keys;try{for(std::size_t at=2;at+1<row.size();at+=2){std::size_t x,y;int f=std::stoi(row[at],&x),t=std::stoi(row[at+1],&y);if(x!=row[at].size()||y!=row[at+1].size())return fail(e,"source captain source key malformed");keys.push_back({f,t});}}catch(...){return fail(e,"source captain source key malformed");}std::stable_sort(keys.begin(),keys.end(),[](Key a,Key b){return a.frame<b.frame;});if(keys!=clip.keys)return fail(e,"source captain source keys mismatch");}
   if(hash(raw)!=sourceSha||be16(raw,42)!=unsigned(duration)||be16(raw,44)!=11)return fail(e,"source captain animation source mismatch");closure+=sourceSha;
   int poses;if(!(in>>poses)||poses<1||poses>64)return fail(e,"source captain pose count invalid");for(int v=0;v<poses;++v){int frame;if(!(in>>frame)||frame<0||frame>=duration||(!clip.frames.empty()&&frame<=clip.frames.back()))return fail(e,"source captain pose frame invalid");clip.frames.push_back(frame);}if(clip.frames.front()!=0||clip.frames.back()!=duration-1)return fail(e,"source captain pose endpoints invalid");
   for(int v=0;v<poses;++v){char suffix[16];std::snprintf(suffix,sizeof(suffix),"_%02d.mod",v);std::string poseRole="poses/"+name+"_"+clipName+suffix;if(!(in>>expected)||!get(poseRole,raw,e)||hash(raw)!=expected)return fail(e,"source captain exact pose bytes mismatch");total+=raw.size();if(total>64*1024*1024)return fail(e,"source captain bank byte bound exceeded");closure+=expected;std::vector<unsigned char> b(raw.begin(),raw.end());p2pose::Baked baked;if(!p2pose::decodeBaked(b,baked))return fail(e,"source captain flattened pose malformed");if(model.first.empty()){model.first=raw;model.topology=baked.topology;}else if(model.topology!=baked.topology)return fail(e,"source captain pose topology/resources mismatch");clip.poses.push_back(std::move(baked.pose));}
   model.clips.emplace(id,std::move(clip));
  }
 }
 if(in>>word)return fail(e,"source captain bank trailing data");
 // Native geometry installation is deliberately deferred until the actual
 // independent self/bound clocks and root4 joint composition are implemented.
 // Retained verified source buffers do not grant runtime readiness.
 next->fingerprint=hash(closure);next->prepared=true;m=std::move(next);e.clear();return true;
}
bool SourceBank::parameters(SourceParameters& out,std::string& e)const{if(!m->prepared)return fail(e,"source captain parameters not prepared");out=m->parameters;e.clear();return true;}
bool SourceBank::sourceBytes(SourceResource r,std::string& out,std::string& e)const{unsigned i=unsigned(r);if(!m->prepared||i>=m->sources.size())return fail(e,"source captain retained source unavailable");out=m->sources[i];e.clear();return true;}
bool SourceBank::bindRoster(Navi*,Navi*,std::string& e){return fail(e,"source captain dual-clock joint geometry not installed");}
bool SourceBank::start(Navi* n,Motion motion,std::string& e){auto it=m->actors.find(n);if(!m->bound||it==m->actors.end()||!it->second.model->clips.count(unsigned(motion))||unsigned(motion)==1000||m->generation==UINT64_MAX)return fail(e,"source captain motion/roster invalid");auto& a=it->second;a.state={motion,0,++m->generation,false,false};a.next=0;a.down=false;return m->sample(a,e);}
bool SourceBank::startPreservingFrame(Navi* n,Motion motion,std::string& e){auto it=m->actors.find(n);if(it==m->actors.end()||!locomotion(it->second.state.motion)||!locomotion(motion)||it->second.down||it->second.state.complete)return fail(e,"source captain frame preservation requires live locomotion");float frame=it->second.state.frame;if(!start(n,motion,e))return false;auto& a=it->second;a.state.frame=frame;const auto& keys=a.model->clips.at(unsigned(motion)).keys;while(a.next<keys.size()&&keys[a.next].frame<frame)++a.next;return m->sample(a,e);}
bool SourceBank::advance(Navi* n,float amount,const std::function<bool(int)>& emit,std::string& e){auto it=m->actors.find(n);if(it==m->actors.end()||!emit||!std::isfinite(amount)||amount<0)return fail(e,"source captain advance invalid");auto& a=it->second;if(a.advancing)return fail(e,"source captain recursive clock advance refused");a.advancing=true;struct Advancing {Impl& impl;Navi* actor;~Advancing(){auto it=impl.actors.find(actor);if(it!=impl.actors.end())it->second.advancing=false;}} guard{*m,n};if(a.down)return fail(e,"source captain Down clock belongs to Studio");if(a.state.complete){e.clear();return true;}const auto generation=a.state.generation;auto frame=a.state.frame+amount;auto next=a.next;const auto& clip=a.model->clips.at(unsigned(a.state.motion));if(!std::isfinite(frame)||frame>1000000)return fail(e,"source captain clock outside bound");while(next<clip.keys.size()&&clip.keys[next].frame<int(frame)){const auto key=clip.keys[next++];a.state.frame=frame;a.next=next;bool keep=emit(key.type);auto live=m->actors.find(n);if(live==m->actors.end()||live->second.state.generation!=generation){e.clear();return true;}if(!keep){a.state.frame=frame;a.next=next;return m->sample(a,e);}if(key.type==1&&!a.state.finishing){auto loop=std::find_if(clip.keys.rbegin(),clip.keys.rend(),[&](Key k){return k.type==0&&k.frame<key.frame;});if(loop==clip.keys.rend())return fail(e,"source captain loop start absent");frame=float(loop->frame);next=0;while(next<clip.keys.size()&&clip.keys[next].frame<frame)++next;a.state.frame=frame;a.next=next;return m->sample(a,e);}}
 a.state.frame=frame;a.next=next;if(frame>=clip.duration){a.state.frame=float(clip.duration-1);a.state.complete=true;emit(1000);auto live=m->actors.find(n);if(live==m->actors.end()||live->second.state.generation!=generation){e.clear();return true;}}return m->sample(a,e);}
bool SourceBank::supports(Navi* n,Motion motion,std::string& e)const{auto it=m->actors.find(n);if(!m->bound||it==m->actors.end()||!it->second.model->clips.count(unsigned(motion))||unsigned(motion)==1000)return fail(e,"source captain selected clip/roster unavailable");e.clear();return true;}
bool SourceBank::finish(Navi* n,std::string& e){auto it=m->actors.find(n);if(it==m->actors.end())return fail(e,"source captain finish roster absent");it->second.state.finishing=true;e.clear();return true;}
bool SourceBank::state(const Navi* n,MotionState& out,std::string& e)const{auto it=m->actors.find(n);if(it==m->actors.end())return fail(e,"source captain state roster absent");out=it->second.state;e.clear();return true;}
bool SourceBank::startDownMovie(Navi* n,std::string& e){auto it=m->actors.find(n);if(it==m->actors.end())return fail(e,"source captain Down roster absent");if(m->generation==UINT64_MAX)return fail(e,"source captain generation exhausted");it->second.state.generation=++m->generation;it->second.down=true;it->second.downFrame=0;return m->sample(it->second,e);}
bool SourceBank::updateDownMovie(Navi* n,float frame,std::string& e){auto it=m->actors.find(n);if(it==m->actors.end()||!it->second.down||!std::isfinite(frame)||frame<0||frame>207||frame<it->second.downFrame)return fail(e,"source captain Studio frame invalid");it->second.downFrame=frame;return m->sample(it->second,e);}
bool SourceBank::refresh(Navi* n,Graphics& gfx,std::string& e){auto it=m->actors.find(n);if(it==m->actors.end()||!gfx.mCamera)return fail(e,"source captain draw context absent");auto& a=it->second;if(!m->sample(a,e))return false;Matrix4f world,view;world.makeSRT(Vector3f(1,1,1),n->mSRT.r,n->mSRT.t);gfx.mCamera->mLookAtMtx.multiplyTo(world,view);a.geometry->shape.updateAnim(gfx,view,nullptr,n);a.geometry->shape.drawshape(gfx,*gfx.mCamera,nullptr);return true;}
void SourceBank::forget(Navi* n){m->actors.erase(n);m->bound=m->actors.size()==2;}
bool SourceBank::ready()const{return m->prepared&&m->bound&&m->actors.size()==2;}
const std::string& SourceBank::fingerprint()const{return m->fingerprint;}
} }
