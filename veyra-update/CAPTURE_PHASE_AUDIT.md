# Capture readiness and phase reserve audit

Based on installed engine-opt.20, source 4ee61fdde9d5c22c5741a264b5389b9c6c008a1b. Both investigations are included in one build.

## Submission and completion
live-submit-ready logs CPU callback-to-graph-call, graph-call duration, graph-return-to-watch-registration, first fence poll, and first observed completion of the frame setting phase. It also reports existing GPU graph span and FG durations. Graph return is not a queue submission timestamp: command lists can execute before the graph call returns. CPU intervals overlap GPU work. Do not subtract FG time from phase requirement and call the remainder GPU queue latency. Unknown/reordered intervals report -1. One qualifying frame is logged per second, not a percentile.

## Phase reserve
live-phase-reserve reports the learner's sample count and warmup, readiness P95, guard, target, actual selected base phase, excess above target, adaptive deadline extra, total phase and cap. The audit leaves the default quantile, 0.5 ms guard, sample window, warmup, downward slew, admission floors, cap and deadline-extra policy unchanged. It does not discard tail samples.

## Independent guard comparison
The default launch and Start-Veyra-Standard-Guard.cmd use the existing 0.5 ms guard.
After closing Veyra normally, Start-Veyra-250us-Guard.cmd sets VEYRA_TEST_CONTENT_PHASE_GUARD_US=250 only for that launch and its children. The comparison applies only to physical capture with the smoothed content clock; other paths retain 0.5 ms. This is a fixed guard experiment, not a new adaptive policy.
The deadline-extra controller remains active and may offset some or all of the 0.25 ms change. No improvement is guaranteed. Quantile, admission floors and cap remain.
Close Veyra normally before each launcher. The scripts do not stop processes or modify global environment/settings. The usual desktop shortcut starts the standard policy unless launched from an environment explicitly carrying the experiment variable.
Keep Medium 2x, HDR, capture format, output cap and display sync identical. Warm up and compare multiple stable windows. Reject the smaller guard if expired generated frames, drops or uneven delivery increase. Submitted FPS and CPU timings are not physical scanout or controller-to-screen measurements.

Tests cover CPU interval order/missing values, exact guard delta on a deterministic steady workload, clamped minimum guard, unchanged warmup, slew, phase caps, P95 and reset. CPU/startup/updater tests are not GPU gameplay proof.
No backups/rollback binaries are created. Existing NVIDIA SDKs, VFG, driver, FFmpeg, settings and image processing are preserved.
