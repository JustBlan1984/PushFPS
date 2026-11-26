#pragma once
#include "veyra/engine/PresentTearingDetector.h"
#include "veyra/diagnostics/GpuTimer.h"
#include "veyra/gfx/PresentSink.h"
#include <dxgi1_6.h> // DXGI_HDR_METADATA_HDR10 (custom: HDR static metadata)
#include "veyra/pipeline/GpuPassUtils.h"
#include "veyra/pipeline/FramePacket.h" // custom: pipeline::DoviFrameInfo
#include "veyra/engine/PreviewView.h"
#include "veyra/engine/HdrOutputTuning.h"
#include "veyra/gfx/CommandSlotRing.h"
#include "veyra/gfx/ReflexSession.h"
#include "veyra/gfx/NvidiaVrr.h"
#include "veyra/engine/PresentationSettings.h"
#include "veyra/engine/PresentCapture.h"
#include "veyra/engine/DisplayStateService.h"
#include "veyra/engine/FullscreenFrameStats.h"
#include <chrono>
#include <array>
#include <optional>
#include <mutex>
#include <functional>
#include "veyra/engine/NativePresentWorker.h"
namespace veyra::gfx { class D3D12DeviceContext; class CommandSlotRing; }
namespace veyra::pipeline { class EnhanceGraph; }
namespace veyra::sink { struct RgbaImage; }
namespace veyra::engine {
class VideoPresenter {
public:
    // Live recording hook; the object must outlive every present() call.
    void setCapture(PresentCapture* capture){std::lock_guard presenterLock(presentationMutex_);capture_=capture;}
    // Shared refresh/VRR states (the engine's); must outlive the presenter.
    void setDisplayStates(DisplayStateService* states){std::lock_guard presenterLock(presentationMutex_);displayStates_=states;}
    ~VideoPresenter(){close();}
    // mediaClockPaced: the owner already waits on a media clock before each
    // source (file playback). Retained for diagnostics; XeLL low latency must
    // stay enabled or the provider refuses generation (result -15).
    struct PreparedOpening {
        pipeline::GraphicsPass blit;
        pipeline::ComputePass motion;
        std::array<Microsoft::WRL::ComPtr<ID3D12Resource>,2> motionTextures;
        std::unique_ptr<gfx::XessPresenter> xess;
    };
    // CPU/driver pipeline creation only. The rendering owner still opens and
    // attaches the swapchain to the window after the graph is ready.
    static bool prepareOpening(gfx::D3D12DeviceContext&,pipeline::EnhanceGraph&,PreparedOpening&);
    bool open(gfx::D3D12DeviceContext&, HWND, pipeline::EnhanceGraph&, bool captureCompatible=false,
              bool mediaClockPaced=false,std::unique_ptr<PreparedOpening> prepared={},HWND temporarySurface=nullptr);
    bool present(gfx::D3D12DeviceContext&,gfx::CommandSlotRing&,pipeline::EnhanceGraph&,unsigned slot,bool generated,bool referencesValid=true,int comparison=0,bool baseReference=false,float split=.5f,pipeline::FrameIdentity identity={},PreviewView view={},int64_t sourcePts100ns=-1,bool retainUnchanged=false);

    struct AsyncPresentation {
        NativePresentWorker::Handle ticket;
        std::shared_ptr<pipeline::FrameLease> lease;
        Microsoft::WRL::ComPtr<ID3D12Fence> fence;
        bool success=false,submitted=false;
        uint64_t consumerFence=0;
        int64_t begin100ns=0,end100ns=0,call100ns=0;
    };
    bool canAsyncPresent() const;
    std::shared_ptr<AsyncPresentation> queueNativePresent(
        gfx::D3D12DeviceContext&,gfx::CommandSlotRing&,pipeline::EnhanceGraph&,
        std::shared_ptr<pipeline::FrameLease>,bool generated,bool referencesValid,
        int comparison,bool baseReference,float split,pipeline::FrameIdentity,
        PreviewView,int64_t sourcePts100ns,int64_t deadline100ns,int64_t expiry100ns,
        uint64_t reflexFrame,bool hdrCoverage,std::function<bool()> valid);
    // Completion/fence publication stays on the graph owner, including cancel.
    bool finishNativePresent(const std::shared_ptr<AsyncPresentation>&,pipeline::EnhanceGraph&,bool cancel=false);

    void close();
    // Owner-thread graph replacement boundary. Native DXGI remains attached to
    // its HWND; provider-owned presentation must be shut down before its graph.
    bool prepareGraphChange();
    // Explicit lifecycle regression: simulate an overlay retaining the chain.
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChainForTest()const{std::lock_guard presenterLock(presentationMutex_);return sink_.swapChain();}
    // Present-sink providers (XeSS) block inside Present while pacing
    // their generated frames. A helper-thread Present was tried on
    // 2026-09-22 (X3): the provider then estimated its period from the
    // longer hand-off intervals and the source rate fell 51 -> 24/s at 44%
    // GPU. The synchronous call stays; see FG_INDEPENDENT_REPAIR_EXECUTION.
    // stallUs > 0: a live source; a driver low-latency Sleep longer than this,
    // repeatedly, ends NVIDIA Low Latency for the session (ReflexSession).
    bool beginSourceInput(uint64_t stallUs=0);
    bool beginSourceProcessing();
    void sourceProcessed(pipeline::FrameIdentity);
    // Custom: Dolby Vision level-1 brightness of the source frame that is about
    // to be presented. The owner thread sets it after the frame leaves the
    // graph and before present(); an invalid value means "no RPU", which keeps
    // the follow path from changing anything.
    void setFrameDovi(const pipeline::DoviFrameInfo& d){std::lock_guard presenterLock(presentationMutex_);latestDovi_=d;}
    pipeline::DoviFrameInfo frameDovi() const {std::lock_guard presenterLock(presentationMutex_);return latestDovi_;}
    PresentationSettings configurePresentation(gfx::D3D12DeviceContext&,PresentationSettings,bool fg,std::wstring& status);
    bool presentationReady(){std::lock_guard presenterLock(presentationMutex_);return sink_.presentationReady();}
    gfx::PresentSink::RefreshGrid refreshGrid()const{std::lock_guard presenterLock(presentationMutex_);return sink_.refreshGrid();}
    bool waitPresentationReady(unsigned ms){std::lock_guard presenterLock(presentationMutex_);return sink_.waitPresentationReady(ms);}
    uint64_t beginReflex(){std::lock_guard presenterLock(presentationMutex_);const auto id=reflexInputFrame_;reflexInputFrame_=0;reflex_.renderStart(id);return id;}
    void endReflex(uint64_t id){std::lock_guard presenterLock(presentationMutex_);reflex_.renderEnd(id);}
    void cancelReflexInput(){std::lock_guard presenterLock(presentationMutex_);if(reflexInputFrame_)reflex_.mark(reflexInputFrame_,1);reflexInputFrame_=0;}
    void reflexFrame(uint64_t id){std::lock_guard presenterLock(presentationMutex_);reflexFrame_=id;}
    // True while XeSS owns Present: the low-latency queue and the output
    // cap cannot reach provider-generated frames, so the UI greys them out.
    bool providerOwnedPresentation()const{std::lock_guard presenterLock(presentationMutex_);return providerOwnedPresentation_;}
    // Native presentation has its own command ring and a GPU-side producer
    // fence wait. XeSS shares the graph ring and must not re-enter it.
    bool independentQueue()const{std::lock_guard presenterLock(presentationMutex_);return presentationQueue_!=nullptr;}

    bool reflexActive()const{std::lock_guard presenterLock(presentationMutex_);return reflex_.active();}
    bool reflexDisablePending()const{std::lock_guard presenterLock(presentationMutex_);return reflex_.disablePending();}
    bool reflexFailed()const{std::lock_guard presenterLock(presentationMutex_);return reflex_.failed();}
    bool reflexStalled()const{std::lock_guard presenterLock(presentationMutex_);return reflex_.stalled();}
    // Host time (steady clock, 100 ns) of the last swap-chain Present call, taken
    // just before it as RTSS and PresentMon do: the 1% low counts these, not
    // returns, whose spacing also carries the call's own 0.2-0.6 ms jitter.
    int64_t lastPresentCallHost100ns()const{std::lock_guard presenterLock(presentationMutex_);return lastPresentCallHost100ns_;}
    bool pacingActive()const{std::lock_guard presenterLock(presentationMutex_);return sink_.pacingActive();}
    uint64_t generation()const{std::lock_guard presenterLock(presentationMutex_);return generation_;}
    // Explicit integration-test capture only; never called by playback/export.
    bool readPresentedFrameForTest(gfx::D3D12DeviceContext&,gfx::CommandSlotRing&,sink::RgbaImage&);
    // A visible HDR panel can request sparse GPU statistics. No pixel readback.
    void observeHdrCoverage(bool on){std::lock_guard presenterLock(presentationMutex_);probeObserved_=on;if(!on)liveCoverage_={};}
    HdrCurveCoverage liveCurveCoverage() const {std::lock_guard presenterLock(presentationMutex_); return liveCoverage_; }
    // Test-only buffer reference for lossless HDR readback. Drains the private
    // queue; caller drains the shared queue and releases before resize/close.
    Microsoft::WRL::ComPtr<ID3D12Resource> presentedResourceForTest();
    uint64_t submittedCount() const {std::lock_guard presenterLock(presentationMutex_);return sink_.presentCount();}
    uint64_t consumerFenceValue(uint64_t sharedValue)const{std::lock_guard presenterLock(presentationMutex_);return presentationQueue_?presentationRing_.lastSignaledValue():sharedValue;}
    double cpuWaitMilliseconds(const gfx::CommandSlotRing& shared)const{std::lock_guard presenterLock(presentationMutex_);return presentationQueue_?presentationRing_.cpuWaitMilliseconds():shared.cpuWaitMilliseconds();}
    uint64_t xessGeneratedCount() const {std::lock_guard presenterLock(presentationMutex_);return sink_.xess()?sink_.xess()->generatedCount():0;}
    uint64_t xessPresentedCount() const {std::lock_guard presenterLock(presentationMutex_);return sink_.xess()?sink_.xess()->presentedCount():0;}
    bool xessActive() const {std::lock_guard presenterLock(presentationMutex_);return sink_.xess()!=nullptr;}
    bool xessFailed() const {std::lock_guard presenterLock(presentationMutex_);return xessFailed_;}
    // Sustained under-rate may suppress SDK-owned XeSS-FG generation over a
    // stable interval (xefgSwapChainSetEnabled); re-enabling goes through the
    // per-frame history reset, never a per-frame toggle.
    void setXessGenerationSuppressed(bool v){std::lock_guard presenterLock(presentationMutex_);xessGenerationSuppressed_=v;}
    bool xessGenerationSuppressed() const {std::lock_guard presenterLock(presentationMutex_);return xessGenerationSuppressed_;}
    // Monitor refresh for the live output budget and 跟随显示器, from the shared
    // DisplayStateService. It is read again only on events: a new presentation
    // chain, the window moving to another monitor, a display change or
    // 重新检测, and one VRR confirmation (vrrConfirmAt_); never on a timer.
    // This accessor never waits on QueryDisplayConfig or NVIDIA's driver calls.
    double displayRefreshHz();
    // Variable refresh state of that monitor (NVIDIA only), read with it.
    gfx::VrrState displayVrr()const{std::lock_guard presenterLock(presentationMutex_);return vrr_;}
    // The 可变刷新 rule: the switch is on, Automatic is selected and VRR is
    // possible on the monitor, so Automatic presents like explicit Vsync.
    // vrrSync() is what the last configurePresentation applied.
    static bool vrrSyncWanted(const PresentationSettings& requested,const gfx::VrrState& vrr){
        return requested.variableRefresh&&requested.display==DisplaySync::Automatic&&gfx::vrrLevel(vrr)>=3;
    }
    bool vrrSync()const{std::lock_guard presenterLock(presentationMutex_);return vrrSync_;}
    TearingHint tearingHint()const {std::lock_guard presenterLock(presentationMutex_); return tearingHint_; }
    int displayCompositionMode()const{std::lock_guard presenterLock(presentationMutex_);return displayCompositionMode_;}
    void setFullscreenFrameStats(int corner){std::lock_guard presenterLock(presentationMutex_);frameStatsCorner_=corner;}
    bool frameStatsNeedsUpdate() const {std::lock_guard presenterLock(presentationMutex_);
        return frameStatsCorner_>=0&&frameStats_.needsUpdate();
    }
    void updateFrameStats(std::optional<double> fps,std::optional<double> low){std::lock_guard presenterLock(presentationMutex_);
        if(frameStatsCorner_>=0)frameStats_.update(fps,low);
    }
    diagnostics::GpuSample blitTiming(ID3D12Fence* f,uint64_t revision=0,uint64_t epoch=0){std::lock_guard presenterLock(presentationMutex_);gpuTimer_.collect(presentationFence_?presentationFence_.Get():f);if((revision&&gpuTimer_.last().identity.settingsRevision!=revision)||(epoch&&gpuTimer_.last().identity.epoch!=epoch)){diagnostics::GpuSample pending;pending.state=diagnostics::SampleState::Pending;return pending;}return gpuTimer_.last().gpu[size_t(diagnostics::GpuStage::Blit)];}
    // Application-side frame-generation timing for present-sink backends
    // (XeSS): the copies, barriers and provider prepare work recorded on
    // our list. collect() is idempotent, so this is safe alongside blitTiming.
    diagnostics::GpuSample fgTiming(ID3D12Fence* f,uint64_t revision=0,uint64_t epoch=0){std::lock_guard presenterLock(presentationMutex_);gpuTimer_.collect(f);if((revision&&gpuTimer_.last().identity.settingsRevision!=revision)||(epoch&&gpuTimer_.last().identity.epoch!=epoch)){diagnostics::GpuSample pending;pending.state=diagnostics::SampleState::Pending;return pending;}return gpuTimer_.last().gpu[size_t(diagnostics::GpuStage::FgBatch)];}
std::vector<diagnostics::GpuFrameTiming> takeGpuTimings(ID3D12Fence* fence){std::lock_guard presenterLock(presentationMutex_);gpuTimer_.collect(presentationFence_?presentationFence_.Get():fence);return gpuTimer_.takeCompleted();}
void recordGpuTimings(){std::lock_guard presenterLock(presentationMutex_);gpuTimer_.recordCompleted();}
private:
    mutable std::recursive_mutex presentationMutex_;
    NativePresentWorker nativeWorker_;
    template<class Graph> bool presentFrame(gfx::D3D12DeviceContext&,gfx::CommandSlotRing&,Graph&,unsigned,bool,bool,int,bool,float,pipeline::FrameIdentity,PreviewView,int64_t,bool);
    // Custom: HDR10 static metadata for the display (MaxCLL / MaxFALL / mastering
    // peak). Sent only when it changes, and cleared when the HDR10 path stops.
    template<class Graph> void applyHdrMetadata(Graph& graph,const HdrMetadataSettings& metadata,const HdrCurveSettings& resolvedCurve,pipeline::FrameIdentity identity);
    bool metadataSent_=false,metadataDirty_=false;
    // The source's mastering/CLL values are absent on many frames; holding the last
    // real value keeps the reported metadata steady instead of flapping to the
    // fallback (which re-sent it on nearly every frame).
    HdrSourceMetadata sourceMetadata_{};
    gfx::PresentSink::DisplayPeakInfo hdrPeakInfo_{};
    std::chrono::steady_clock::time_point metadataAttempt_{};
    DXGI_HDR_METADATA_HDR10 lastMetadata_{};
    gfx::ReflexSession reflex_;
    uint64_t reflexFrame_=0,reflexInputFrame_=0,generation_=0;
    int64_t lastPresentCallHost100ns_=0;
    int displayCompositionMode_=-1;
    bool providerOwnedPresentation_=false;
    PresentCapture* capture_=nullptr;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> presentationQueue_;
    Microsoft::WRL::ComPtr<ID3D12Fence> presentationFence_;
    HANDLE presentationEvent_=nullptr;
    gfx::CommandSlotRing presentationRing_;
    diagnostics::GpuTimer gpuTimer_;
    gfx::PresentSink sink_;
    pipeline::GraphicsPass pass_;
    pipeline::ComputePass presentMotionPass_;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>,2> presentMotion_;
    PreviewView previousXessView_{};
    unsigned previousXessWidth_=0,previousXessHeight_=0;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvs_;
    // One bounded GPU-only composition surface, shared by sequential lists on
    // the presentation queue. Recreated only after that queue has drained.
    Microsoft::WRL::ComPtr<ID3D12Resource> compositeTarget_;
    Microsoft::WRL::ComPtr<ID3D12Resource> exclusionUiTarget_,exclusionHudlessTarget_;
    bool exclusionUiEnabled_=false;
    bool copyBeforePresent_=false,copyEligible_=false;
    unsigned inc_=0;
    static constexpr unsigned kProbeWidth=64,kProbeHeight=36,kProbeStatsBytes=5*sizeof(uint32_t);
    pipeline::ComputePass probePass_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> probeCpuViews_;
    Microsoft::WRL::ComPtr<ID3D12Resource> probeStats_,probeReadback_;
    uint64_t probeFenceValue_=0;
    bool probePending_=false,probeObserved_=false,probeFailed_=false;
    HdrCurveSettings probeCurve_{};
    pipeline::FrameIdentity probeIdentity_{};
    std::chrono::steady_clock::time_point probeNext_{};
    HdrCurveCoverage liveCoverage_{};
    // Custom: Dolby Vision level 1 for the frame being presented. Purely
    // additive: the blit uses it only when the follow switch is on.
    pipeline::DoviFrameInfo latestDovi_{};
    // Exponential smoothing for the follow inputs. A sliding window was used here
    // first and was wrong: halving a running sum and its integer count keeps the
    // ratio only when the count is even, so on odd counts the average gained a
    // fraction each time the window overflowed and drifted above every input it had
    // seen - a peak reading of 1.11 after inputs capped at 1.0. EMA has no such
    // accumulation and a reset path for scene cuts.
    double avgEma_=0.0; bool avgEmaSeeded_=false;
    double peakEma_=0.0; bool peakEmaSeeded_=false;
    // Custom: the knee also runs through clamp() and min(user); those flatten or
    // step, so a smoothed input can still reach the blit as a jump. The value that
    // actually ships is smoothed here too.
    float smoothedStartNits_=0.0f;
    bool ensureCoverageProbe(ID3D12Device*);
    void publishLiveCoverage(const HdrCurveSettings&,pipeline::FrameIdentity);
    unsigned lastBuffer_=0;bool hasPresented_=false;
    struct RetainedFrame {
        ID3D12Fence* producer=nullptr;
        uint64_t fence=0;
        unsigned slot=0,width=0,height=0;
        bool generated=false,referencesValid=false,baseReference=false,visible=false;
        int comparison=0;float split=.5f;
        PreviewView view{};
        pipeline::FrameIdentity identity{};
        HMONITOR monitor=nullptr;
        HdrCurveSettings displayCurve{};
        int frameStatsCorner=-1;
        bool operator==(const RetainedFrame&)const=default;
    };
    std::optional<RetainedFrame> retainedFrame_;
    HWND window_=nullptr;
    HWND temporarySurface_=nullptr;
    ID3D12Device* presentationDevice_=nullptr;
    DXGI_FORMAT presentationFormat_=DXGI_FORMAT_UNKNOWN;
    bool captureCompatible_=false;
    std::chrono::steady_clock::time_point lastResize_{};
    unsigned viewWidth_=0,viewHeight_=0;
    HMONITOR bufferMonitor_=nullptr;
    unsigned monitorWidth_=0,monitorHeight_=0;
    std::chrono::steady_clock::time_point nextCostLog_{};
    std::chrono::steady_clock::time_point scanoutSampleAt_{};
    PresentTearingDetector tearingDetector_;
    TearingHint tearingHint_ = TearingHint::None;
    DisplaySync scanoutSync_ = DisplaySync::Automatic;
    bool scanoutFullscreen_ = false;
    std::chrono::steady_clock::time_point lastXessFrame_{};
    pipeline::FrameIdentity lastXessIdentity_{};
    int64_t lastXessPts100ns_=-1;
    bool xessWasEnabled_=false;
    bool xessFailed_=false;
    bool xessGenerationSuppressed_=false;
    HMONITOR refreshMonitor_=nullptr;double refreshHz_=0;gfx::VrrState vrr_{};bool refreshQueried_=false,vrrSync_=false;
    DisplayStateService* displayStates_=nullptr;
    uint64_t displayStateSerial_=0,displayGeneration_=0,observedDisplayGeneration_=0;
    // VRR presentation may start or stop after it is requested or when the
    // window enters/leaves fullscreen: one forced query 3 s after that settles.
    std::optional<std::chrono::steady_clock::time_point> vrrConfirmAt_;
    void startDisplayState();
    void applyDisplayState(const DisplayState&);
    uint32_t xessInputId_=0;
    struct XessWork {pipeline::FrameIdentity identity{};uint32_t id=0;};
    std::array<XessWork,4> xessWork_{};
    size_t xessWorkPosition_=0;
    bool refresh(ID3D12Device*);
    bool ensureXessUiSurfaces(ID3D12Device*,D3D12_RESOURCE_DESC);
    FullscreenFrameStats frameStats_;
    int frameStatsCorner_=-1;
    bool xessFrameStatsFailed_=false;
};
}
