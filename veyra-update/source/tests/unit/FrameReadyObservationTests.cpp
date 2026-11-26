#include "veyra/engine/FrameReadyObservation.h"
#include "veyra/engine/LivePairLatency.h"
#include <cassert>
#include <iostream>
using namespace veyra::engine;
int main(){
    FrameReadyObservation fence;
    assert(fence.ready100ns()==0&&fence.bracket100ns()==-1);
    assert(!fence.observe(0,true)&&!fence.observe(10000,false));
    assert(!fence.observe(20000,false));
    assert(fence.observe(22000,true));
    assert(fence.ready100ns()==22000&&fence.bracket100ns()==2000);
    assert(!fence.observe(90000,true)&&!fence.observe(100000,false));
    assert(fence.ready100ns()==22000&&fence.bracket100ns()==2000);
    FrameReadyObservation unknown;
    assert(unknown.observe(50000,true)&&unknown.bracket100ns()==-1);
    FrameReadyObservation backwards;
    backwards.observe(30000,false);
    assert(!backwards.observe(29000,true)&&backwards.ready100ns()==0);
    assert(backwards.observe(31000,true));

    // The same completed fence is observed before or after 4 ms of metadata
    // work. The early observation remains conservative, but avoids teaching
    // that CPU work to the phase learner. No prediction marks a fence ready.
    constexpr int64_t actual=30000,earlyPoll=32000,latePoll=72000;
    FrameReadyObservation early,late;
    early.observe(25000,false);late.observe(25000,false);
    assert(!early.ready100ns()&&!late.ready100ns());
    early.observe(earlyPoll,true);late.observe(latePoll,true);
    assert(early.ready100ns()>=actual&&late.ready100ns()>=actual);
    LivePairLatency earlyPhase,latePhase;
    int64_t a=0,b=0;
    for(int i=0;i<120;++i){
        const int64_t now=10000000+int64_t(i)*333333;
        earlyPhase.observe(now,early.ready100ns());
        latePhase.observe(now,late.ready100ns());
        a=earlyPhase.select(now,333333,333333,2);
        b=latePhase.select(now,333333,333333,2);
    }
    assert(a==earlyPoll+LivePairLatency::kMargin100ns);
    assert(b-a==latePoll-earlyPoll);
    // A new watch does not retain the preceding frame's readiness.
    early={};assert(!early.ready100ns()&&early.bracket100ns()==-1);
    std::cout<<"PASS fence observations: first completion, pending brackets, unknown bounds, delayed polling, phase learning and reset\n";
}
