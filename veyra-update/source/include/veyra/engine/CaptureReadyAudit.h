#pragma once
#include <cstdint>

namespace veyra::engine {
// CPU-clock intervals only. The graph can submit several command lists before
// returning, so graph-return-to-ready is not GPU execution or GPU queue delay.
struct CaptureReadyAudit {
    static constexpr int64_t elapsed(int64_t begin,int64_t end){
        return begin>0&&end>=begin?end-begin:-1;
    }
    int64_t callbackToGraph=-1,graphCall=-1,graphReturnToWatch=-1;
    int64_t graphReturnToFirstPoll=-1,graphReturnToReady=-1;
    static constexpr CaptureReadyAudit measure(int64_t callback,int64_t graphBegin,
        int64_t graphEnd,int64_t watchRegistered,int64_t firstPoll,int64_t ready){
        return {elapsed(callback,graphBegin),elapsed(graphBegin,graphEnd),
            elapsed(graphEnd,watchRegistered),elapsed(graphEnd,firstPoll),elapsed(graphEnd,ready)};
    }
};
}
