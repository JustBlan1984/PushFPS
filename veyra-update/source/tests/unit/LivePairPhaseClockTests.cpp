#include "veyra/engine/LivePairLatency.h"
#include "veyra/engine/PresentationScheduler.h"
#include <cassert>
#include <iostream>
using namespace veyra::engine;
static int64_t due(int64_t anchor,int64_t phase,int64_t extra,int64_t real,int64_t generated){
    PresentationScheduler timeline;
    timeline.resetPair(1,real,anchor,phase+extra);
    return timeline.deadline(generated);
}
int main(){
    constexpr int64_t callback=10000000,ready=callback+30000,B=1000000,G=B-166667;
    const auto legacy=livePairAdmissionPhase100ns(ready,callback,B,G,0);
    const auto offsetAnchor=callback+80000;
    const auto corrected=livePairAdmissionPhase100ns(ready,offsetAnchor,B,G,0);
    assert(legacy-corrected==80000);
    assert(due(offsetAnchor,corrected,0,B,G)==ready);
    assert(due(offsetAnchor,legacy,0,B,G)==ready+80000); // regression: double-counted offset

    // Positive/negative anchor offsets, 2X-8X spacing, GPU costs and margins.
    // Admission must never move a generated frame before its predicted readiness.
    unsigned cases=0;
    for(int64_t offset=-200000;offset<=300000;offset+=17000)
      for(unsigned multiplier=2;multiplier<=8;++multiplier)
        for(int64_t cost=1000;cost<=300000;cost+=7000)
          for(int64_t extra: {int64_t(0),int64_t(2500),int64_t(10000)}){
            const auto anchor=callback+offset,generated=B-333333/multiplier;
            const auto earliest=callback+cost;
            const auto phase=livePairAdmissionPhase100ns(earliest,anchor,B,generated,extra);
            assert(phase>=0);
            assert(due(anchor,phase,extra,B,generated)>=earliest);
            if(phase>0)assert(due(anchor,phase-1,extra,B,generated)<earliest);
            ++cases;
          }
    // An insufficient cap still rejects; it cannot silently extend latency.
    const auto needed=livePairAdmissionPhase100ns(callback+500000,callback,B,G,0);
    LivePairLatency capped;
    const auto limited=capped.enforceAdmissionFloor(needed,200000);
    assert(limited==200000&&due(callback,limited,0,B,G)<callback+500000);

    // Legitimate zero-delay observations must allow the old phase to decay.
    LivePairLatency phase;int64_t selected=333333;
    for(int i=0;i<120;++i){
        const int64_t now=10000000+int64_t(i)*333333;
        phase.observe(now,0);
        const auto next=phase.select(now,333333,333333,2);
        if(i>=8)assert(next>=selected-5000); // retain bounded downward slew
        selected=next;
    }
    assert(selected==LivePairLatency::kMargin100ns);
    phase.reset();
    assert(phase.select(90000000,333333,333333,2)==333333); // warmup unchanged
    std::cout<<"PASS aligned pair clocks: "<<cases<<" deadline cases, overload rejection, zero learning, slew and reset\n";
}
