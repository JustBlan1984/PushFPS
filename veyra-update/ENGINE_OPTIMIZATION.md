# Veyra 2.2.1 engine optimization experiment

Base: upstream Veyra b9cc86aa9f68462ea740afe6104db4d5ce227aeb plus the user's verified NVIDIA SDK integration.
This is an experiment, not a measured performance improvement.

Changes:
- One ready leased frame can be delivered by a dedicated native DXGI presentation worker while the graph owner processes the next frame.
- The worker owns no enhancement graph: output resources, colour/HDR settings, and source metadata are copied on the graph owner.
- Consumer fences are published on the graph owner after completion; cancellation waits for the callback before releasing a lease.
- Native HDR may use this worker. HDR never falls through to synchronous presentation from inside graph processing.
- Recording, replay, virtual camera, screen-text consumers, custom output caps, files, screen capture, and provider-owned XeSS retain their existing path.
- Enhancement checkpoints try the presenter lock and skip busy calls instead of blocking graph submission.
- UI video-cover keys exclude offscreen/duplicate regions and have deterministic order. Rounded region geometry is preserved; repeated region diagnostics are limited to four per second unless verbose logs are enabled.
- Pair-phase and deadline-margin sample storage uses fixed-capacity arrays. Timing targets, warmup, quantiles, and latency limits are unchanged.

Checks:
- Native worker CPU test covers cancellation, stale validity, one-job capacity, callback lifetime, exception completion, reuse, and no early calls.
- Main CI builds the full app and runs existing scheduling, timing, preview, repair, pipeline, and encoder checks.
- Private software startup and the no-backup updater fixture are required before artifact upload.
- CI has no NVIDIA GPU. Real SR/FG, HDR pixels, GPU synchronization validation, recording/virtual-camera interaction, stability, and physical end-to-end latency are NOT certified here.
- Existing local log samples are historical evidence only; they are not post-change GPU validation.
- VEYRA_DISABLE_NATIVE_PRESENT_WORKER=1 selects the original owner-thread delivery for an eventual same-build comparison.

The installed application and user settings have not been changed by this source/build task.
The updater creates no backups, rollback copies, or rollback receipts.
