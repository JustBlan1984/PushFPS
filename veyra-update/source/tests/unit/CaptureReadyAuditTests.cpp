#include "veyra/engine/CaptureReadyAudit.h"
#include "veyra/engine/LivePairLatency.h"
#include <cassert>
#include <iostream>
using namespace veyra::engine;
static int64_t converge(int64_t guard){
    LivePairLatency phase;int64_t selected=0;
    for(int i=0;i<150;++i){
        const int64_t now=10000000+int64_t(i)*333333;
        phase.observe(now,40000);
        const auto previous=selected;
        selected=phase.select(now,333333,333333,2,0,guard);
        if(i>=8)assert(selected>=previous-5000);
    }
    const auto audit=phase.selectionAudit();
    assert(!audit.warming&&audit.samples>=8&&audit.readinessP95==40000);
    assert(audit.guard>=2500&&audit.guard<=5000);
    assert(audit.target==selected&&audit.limit==333333);
    assert(selected>=audit.readinessP95+audit.guard);
    const auto capped=phase.enforceAdmissionFloor(1000000,45000);
    assert(capped==45000); // preserve admission limit
    phase.reset();assert(phase.selectionAudit().warming&&phase.selectionAudit().readinessP95==-1);
    assert(phase.select(90000000,333333,333333,2,0,guard)==333333);
    return selected;
}
int main(){
    constexpr auto a=CaptureReadyAudit::measure(10000,12000,17000,18000,19000,22000);
    static_assert(a.callbackToGraph==2000&&a.graphCall==5000&&a.graphReturnToWatch==1000);
    static_assert(a.graphReturnToFirstPoll==2000&&a.graphReturnToReady==5000);
    constexpr auto missing=CaptureReadyAudit::measure(0,0,0,0,0,0);
    static_assert(missing.graphCall==-1&&missing.graphReturnToReady==-1);
    static_assert(CaptureReadyAudit::elapsed(200,100)==-1);
    static_assert(CaptureReadyAudit::elapsed(200,200)==0);
    assert(converge(5000)-converge(2500)==2500);
    assert(converge(-10000)==converge(2500)); // experiment cannot remove all guard
    assert(converge(999999)==converge(5000));
    // Quantile, warmup and cap remain conservative during overload and jitter.
    LivePairLatency p;
    for(int i=0;i<30;++i)p.observe(10000000+i*1000,i<27?40000:80000);
    const auto value=p.select(10030000,50000,333333,2,0,2500);
    assert(p.selectionAudit().readinessP95==80000&&value==50000);
    assert(p.selectionAudit().target==50000);
    std::cout<<"PASS capture readiness intervals and phase audit: missing/order bounds, guard A/B, slew, reset, P95 and overload caps\n";
}
