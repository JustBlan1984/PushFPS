#pragma once
#include <cstdint>

namespace veyra::engine {
// Owner-thread observations of a fence, not GPU timestamps. The first completed
// observation is an upper bound; a preceding incomplete poll brackets it.
class FrameReadyObservation {
    int64_t ready_=0,lastPending_=0;
public:
    bool observe(int64_t now,bool complete){
        if(now<=0||ready_)return false;
        if(!complete){if(now>lastPending_)lastPending_=now;return false;}
        if(now<lastPending_)return false;
        ready_=now;return true;
    }
    int64_t ready100ns()const{return ready_;}
    // -1 means there was no incomplete observation: lateness is unknown.
    int64_t bracket100ns()const{return ready_&&lastPending_?ready_-lastPending_:-1;}
};
}
