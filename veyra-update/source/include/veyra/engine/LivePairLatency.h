#pragma once
#include <algorithm>
#include <cstdint>
#include <array>

namespace veyra::engine {
// Solve the generated-frame deadline in its actual presentation clock:
// anchor + phase + extra - (B - generatedPts) >= earliestGpuReady.
// A nonnegative phase is conservative when the content anchor already supplies
// enough time. The caller retains its existing phase cap and rejection policy.
inline constexpr int64_t livePairAdmissionPhase100ns(int64_t earliestGpuReady,int64_t anchor,
                                                    int64_t realPts,int64_t generatedPts,int64_t extra) {
    return std::max<int64_t>(0,earliestGpuReady-anchor+realPts-generatedPts-extra);
}
// Observed readiness of earlier pairs bounds the phase of the next pair.
// Values are host-clock 100 ns; source PTS differences only supply spacing.
class LivePairLatency {
    struct Sample { int64_t time, delay; };
    std::array<Sample,64> samples_{};
    size_t head_=0,count_=0;
    int64_t selected_=0;
public:
    // Beyond the 95th percentile of readiness; LiveArrivalSmoother::kLead100ns
    // takes part of it back on callback-anchored capture.
    static constexpr int64_t kMargin100ns=5000;
    struct SelectionAudit {
        size_t samples=0;
        int64_t readinessP95=-1,guard=kMargin100ns,target=-1,limit=0;
        bool warming=true;
    };
private:
    SelectionAudit selection_{};
public:
    const SelectionAudit& selectionAudit()const{return selection_;}
    void reset(){head_=count_=0;selected_=0;selection_={};}
    void observe(int64_t now,int64_t requiredDelay){
        // Zero is valid: a content-clock anchor can already cover all readiness.
        if(requiredDelay<0||requiredDelay>20000000)return;
        while(count_&&(count_>=samples_.size()||samples_[head_].time<now-10000000)){head_=(head_+1)%samples_.size();--count_;}
        samples_[(head_+count_++)%samples_.size()]={now,requiredDelay};
    }
    // Apply the floor from the actual pair's deadline geometry before
    // deciding if the same pair fits. Keep the original allowance cap.
    int64_t enforceAdmissionFloor(int64_t need,int64_t limit){
        selected_=std::min(std::max(selected_,need),limit);
        return selected_;
    }
    int64_t select(int64_t now,int64_t legacyDelay,int64_t interval,unsigned multiplier,int64_t samplingAllowance=0,int64_t requestedGuard=kMargin100ns){
        std::array<int64_t,64> current{};size_t count=0;
        for(size_t i=0;i<count_;++i){const auto& sample=samples_[(head_+i)%samples_.size()];if(sample.time>=now-10000000)current[count++]=sample.delay;}
        if(interval<=0||multiplier<2){selected_=0;selection_={};return legacyDelay;}
        const auto limit=legacyDelay+samplingAllowance;
        const auto guard=std::clamp(requestedGuard,int64_t(2500),kMargin100ns);
        selection_={count,-1,guard,-1,limit,count<8};
        // A gap in readiness observations is expected when pairs are skipped.
        // Retain the last phase until enough new samples arrive.
        if(count<8){
            selected_=std::min(selected_?selected_:legacyDelay,limit);
            return selected_;
        }
        const auto index=(count*95+99)/100-1;
        std::nth_element(current.begin(),current.begin()+index,current.begin()+count);
        // 0.5 ms covers wake-up jitter with MMCSS. The allowance lets readiness
        // move the phase past the legacy delay: deduplicated capture, where a
        // later sample can move readiness past a deadline on the continuous
        // content clock, and FG whose own interpolation does not fit in the
        // legacy delay (FgRecoveryBudget::phaseAllowance).
        const auto target=std::min(limit,current[index]+guard);
        selection_.readinessP95=current[index];selection_.target=target;
        const auto step=std::min<int64_t>(5000,interval/multiplier/8);
        selected_=std::clamp(selected_?selected_-step:legacyDelay,target,limit);
        return selected_;
    }
};
// Extra time on a capture-card FG pair's deadlines, on top of the learned phase,
// so that 99 of 100 generated frames are ready by their deadline, never more than
// 1 ms (user, 2026-11-13: automatic, up to 1 ms). The phase sits at the 95th
// percentile of readiness once the callback anchor's lead has taken its margin
// back, so one generated frame in twenty was still in the GPU at its deadline,
// 0.1-0.4 ms late on T911 4K60 NR + DLSS/VFG 2X, and those set the 1% low. This
// learns how much later the late ones are: it rises quickly (0.1 ms a quarter
// second) when frames miss and gives latency back more slowly (0.04 ms) when they
// do not. The first 2 s after a start learn nothing: feature warmup reached 0.9 ms
// there and took 6 s to come back down (T911 run, 2026-11-13). A sample is the slack the frame would have had without the extra; the
// caller passes only frames whose readiness it observed by the deadline, or that
// it saw unfinished after it, never ones it simply looked at late.
class LiveDeadlineMargin {
    struct Sample { int64_t time, slack; };
    std::array<Sample,240> samples_{};
    size_t head_=0,count_=0;
    int64_t extra_=kStart100ns,nextUpdate_=0,start_=0;
public:
    static constexpr int64_t kStart100ns=2500,kMax100ns=10000,kGuard100ns=500,kRise100ns=1000,kFall100ns=400;
    static constexpr int64_t kUpdate100ns=2500000,kFloor100ns=-20000,kAge100ns=40000000,kWarmup100ns=20000000;
    static constexpr size_t kSamples=240,kMinSamples=100;
    void reset(){head_=count_=0;extra_=kStart100ns;nextUpdate_=0;start_=0;}
    int64_t extra()const{return extra_;}
    // deadline already includes `extra`, the margin this pair was given.
    void observe(int64_t now,int64_t deadline,int64_t ready,int64_t extra){
        if(ready<=0||deadline<=0)return;
        if(!start_)start_=now;
        if(now<start_+kWarmup100ns)return;
        // One stall cannot outweigh the rest: no frame counts as more than 2 ms late.
        const auto slack=std::max(kFloor100ns,deadline-extra-ready);
        while(count_&&(count_>=kSamples||samples_[head_].time<now-kAge100ns)){head_=(head_+1)%samples_.size();--count_;}
        samples_[(head_+count_++)%samples_.size()]={now,slack};
        if(count_<kMinSamples||now<nextUpdate_)return;
        nextUpdate_=now+kUpdate100ns;
        std::array<int64_t,kSamples> values{};
        for(size_t i=0;i<count_;++i)values[i]=samples_[(head_+i)%samples_.size()].slack;
        const auto index=count_/100;
        std::nth_element(values.begin(),values.begin()+std::ptrdiff_t(index),values.begin()+count_);
        const auto target=std::clamp(kGuard100ns-values[index],int64_t(0),kMax100ns);
        extra_=target>extra_?std::min(target,extra_+kRise100ns):std::max(target,extra_-kFall100ns);
    }
};
// A capture pair whose FG the budget rejects still shows its real frame at the
// FG phase, so FG can resume without a jump. After a second of nothing but
// rejections that phase only costs latency: VFG High 3X at 4K on an RTX 5070
// (2026-11-13, 30 fps game) had 98% of its pairs rejected and showed 30 fps
// 38-46 ms after the callback, ~2 ms without FG. A held pair shows its real frame
// as soon as it is ready. A pair admitted while held (a budget probe) stays held:
// its generated frames are then past their deadlines and expire unshown, while
// the budget still measures its cost. FG resumes on its phase only after more
// pairs in a row are admitted than one budget probe runs (FgCostSettling: 2
// warmup groups plus up to 4 measured ones); with 3 every probe ended the hold
// on that 3X run, a second on the FG phase every 3-4 s and 1% low 26.8.
// Entering and leaving are one uneven interval each.
class LiveFgRejectionHold {
    int64_t rejectedSince_=0;
    unsigned admittedRun_=0;
    bool held_=false;
public:
    static constexpr int64_t kHold100ns=10000000;
    static constexpr unsigned kResumePairs=8;
    void reset(){*this=LiveFgRejectionHold{};}
    bool held()const{return held_;}
    // One call per capture pair after its FG decision; true: show this pair's
    // real frame without the FG phase.
    bool observe(int64_t now,bool admitted){
        if(admitted){
            rejectedSince_=0;
            if(held_&&++admittedRun_<kResumePairs)return true;
            held_=false;admittedRun_=0;
            return false;
        }
        admittedRun_=0;
        if(!rejectedSince_)rejectedSince_=now;
        if(now-rejectedSince_>=kHold100ns)held_=true;
        return held_;
    }
};
}
