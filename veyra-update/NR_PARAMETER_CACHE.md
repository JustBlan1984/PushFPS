# NR scalar input cache candidate

Base: engine-opt.21, 6fa65b231dea9cd22cb667483cb8c04d624c7de1.
The current run uses community-SF-RTX20-RTX50 nvngx_dlssnr.dll 310.9.16.0, preset 2, internal 1920x1080, VFG Medium 2x and HDR.
An 88-second stable baseline reported representative software P95 25.257 ms, NR GPU P95 2.442 ms, optical flow 0.988 ms, VFG 2.655 ms and 60 submitted FPS; no added drops, expiry, history resets or errors.

## Actual optimization
For the community SF runtime, retain the 21 unchanged scalar INPUT parameters in each layer's persistent NGX parameter block instead of issuing identical Set calls every evaluation. Four resource bindings and the dynamic Reset flag are always written.
The scalar key includes dimensions, style, all four float controls, automatic mask and UI correction. Floats compare by exact wire bits. Settings/key changes and temporal history resets force full scalar writes; feature/parameter block replacement and close invalidate the cache. Failed Evaluate invalidates and never commits. Parameter-probe callbacks and other runtime routes retain the original write behavior.
After each full scalar refresh and successful evaluation, typed Get calls verify all 21 input entries are retained. A failed/mismatched read disables caching for that feature and keeps the original writes. CPU tests cover typed readback success, mutation, missing keys and wrong wire types. They prove input equivalence and cache lifecycle, not identical GPU output or behavior on later evaluations. The prior write path remains available for hardware A/B verification.

Set VEYRA_DISABLE_NR_PARAMETER_CACHE=1 only for a comparison launch after closing Veyra normally. Do not change global environment. Keep the standard 0.5 ms phase guard, NR model/settings, resolution, HDR, and VFG Medium identical. nr-parameter-cache reports full scalar applications and cache reuses per feature. No savings in neural kernel time are claimed; this targets CPU API overhead.

## Pass-cost measurement
nr-stage-cost reports existing same-frame optical flow, NR evaluate and residual GPU samples, plus the tail between NR Evaluate end and the enclosing layer end on the single-layer path. That tail includes parity decode and resource transitions; it is not added to the overlapping layer span. Unknown or mismatched/multilayer samples report -1. No new GPU queries, pixel readbacks, waits or shader changes.
Shader fusion is deferred until the measured conversion cost justifies it and hardware output comparisons can preserve FP16/HDR rounding.

## Validation and limits
CPU tests exercise all scalar key fields, typed 21-key payload, float bit identity, separate layers, failed-evaluation retry, invalidation, disabled/probe baseline, and missing/mismatched GPU timestamp ranges.
CI also builds the app and runs the existing engine checks, private startup and updater fixture. They do not prove actual RTX NR image equivalence, runtime mutation behavior, gameplay latency or HDR quality.
Existing SDKs/DLLs/models/driver/settings are preserved. Inactive NVIDIA Original NR 310.8.0 is not replaced by this candidate. No backups or rollback binaries are created.
