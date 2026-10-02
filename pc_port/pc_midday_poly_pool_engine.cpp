#include "pc_midday_poly_pool.h"
#include "ObjectMgr.h"
struct PcMiddayPolyAccess {
 static bool view(const PolyObjectMgr&m,pc_midday::PolyPoolView&out,std::string&e){
  using namespace pc_midday;
  // Constructor initializes mObjectPool to null before geometry exists. Never
  // inspect the uninitialized capacity/stride fields until endRegister ran.
  if(!m.mObjectPool){e="poly native backing allocation unavailable";return false;}
  if(m.mPoolCapacity<0||m.mPoolCapacity>int(MaxActors)||m.mActiveObjects<0||m.mActiveObjects>m.mPoolCapacity||m.mMaxSize<=0||m.mMaxSize>16*1024*1024||m.mMaxClassLength<=0||m.mMaxClassLength>4096||!m.mEntryCount||m.mEntryCount>uint32_t(m.mMaxClassLength)||!m.mEntries||(m.mPoolCapacity&&!m.mObjectIndices)){e="invalid native poly geometry";return false;}
  PolyPoolView v;v.capacity=uint32_t(m.mPoolCapacity);v.count=uint32_t(m.mActiveObjects);v.stride=uint32_t(m.mMaxSize);
  for(uint32_t i=0;i<m.mEntryCount;++i){const auto&t=m.mEntries[i];if(t.mClassSize<=0){e="poly template size invalid";return false;}v.templates.push_back({t.mClassId,uint32_t(t.mClassSize),t.mClassObj});}
  for(int i=0;i<m.mPoolCapacity;++i){v.statuses.push_back(m.mObjectIndices[i]);v.objects.push_back(m.mObjectPool+size_t(m.mMaxSize)*size_t(i));}
  out=std::move(v);return true;
 }
};
namespace pc_midday {
bool readPolyPoolView(const PolyObjectMgr&m,PolyPoolView&out,std::string&e){
 PolyPoolView a,b;if(!PcMiddayPolyAccess::view(m,a,e)||!PcMiddayPolyAccess::view(m,b,e))return false;
 if(a.capacity!=b.capacity||a.count!=b.count||a.stride!=b.stride||a.statuses!=b.statuses||a.objects!=b.objects||a.templates.size()!=b.templates.size()){e="poly pool changed during stopped census";return false;}
 for(size_t i=0;i<a.templates.size();++i){const auto&x=a.templates[i];const auto&y=b.templates[i];if(x.classId!=y.classId||x.bytes!=y.bytes||x.prototype!=y.prototype){e="poly template changed during stopped census";return false;}}
 out=std::move(a);e.clear();return true;
}
}
