#pragma once
// #1281 source6 leaf -> #148 typed producer. Dependencies: published core
// fce554d27 + f95492a3 and BudOrigin f40b27e8 (with Graph SDK167cc).
#include "pc_p2_original_blackpom_native.h"
#include "pc_p2_cave_bud_actor.h"
#include "pc_p2_bud_conversion_producer.h"
#include "Pom.h"

namespace p2original { namespace blackpom {
// Pass this actual consumer to the floor's bindBlackPom helper. The owner must
// first bind the producer Registry to its actual Authority/selected card proof,
// and keep producer, Authority and proof reader alive until all roots release.
// This installs callbacks only; it grants no floor/card/donor authority itself.
inline std::function<bool(Pom*,const InstanceIdentity&,unsigned,std::string&)>
producerConsumer(p2budorigin::Producer& producer) {
 return [&producer](Pom* body,const InstanceIdentity& expected,unsigned token,std::string& error){
  unsigned source=0,actualToken=0;InstanceIdentity actual;
  if(!Native::owner(body)||!pc_p2_original_pom_managed(body)
     ||pc_p2_original_pom_ready(body)
     ||!originalActors().query(body,source,actualToken,&actual)
     ||source!=6||!token||token!=actualToken||!(actual==expected)){
   error="BlackPom typed consumer requires exact bound unstarted source6";return false;
  }
  if(!pc_p2_original_pom_set_head_callback(body,
       &p2budorigin::Producer::snapshotCallback,
       &p2budorigin::Producer::headCallback,&producer)){
   error="BlackPom actual typed producer registration refused";return false;
  }
  error.clear();return true;
 };
}
} }
