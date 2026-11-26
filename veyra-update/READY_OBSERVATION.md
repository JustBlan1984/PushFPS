# Early fence readiness observation

Parent: installed 2.2.1-engine-opt.19, source commit 49edeb4b46f9ea8680d20e1668e45f2830464404.

The engine checked frame fences after GPU profiling collection. If a fence completed during resolveGeneration, readiness could fall back to a later host timestamp after SDK/cost bookkeeping. Both are valid upper bounds but can teach CPU observation delay to the adaptive phase learner.

For physical capture only, a cheap fence-observation pass now precedes timing collection and presentation advancement. A second check immediately after successful generation resolution captures completion before profiling and bookkeeping. No CPU fence waits, GPU prediction, additional worker threads, queue growth, source timestamp changes or reduced safety margins are introduced.

FrameReadyObservation freezes the first completed observation, records the last incomplete observation, and reports their bracket. An unbracketed observation remains an upper bound with unknown delay; bracket width is not actual GPU latency. Incomplete fences are never marked ready. Existing fallback timestamps, frame validity, warmup exclusions, phase caps, quantile and slew rules remain.

The once-per-second live-phase-readiness diagnostic separates generated and real frame phase requirements, maximum poll bracket, unbracketed frames and fallback counts. It reports one qualifying sample per second, not a percentile or GPU completion timestamp.

Set VEYRA_TEST_LEGACY_READY_OBSERVATION=1 before a normal app launch to restore the old polling order in the same executable. Leave VEYRA_TEST_LEGACY_CONTENT_PHASE unset to keep the already measured coordinate correction. Never change global environment or interrupt an active session for a comparison.

CPU test covers immutable first completion, repeated pending polls, unknown brackets, clock ordering, synthetic delayed polling propagated through the real phase learner, and reset. Synthetic 4 ms metadata delay is test input, not a claim of gameplay improvement.

Baseline: the prior stable Medium 2x HDR capture sample reported representative software P95 22.196 ms with 60 submitted FPS, no additional capture drops/expired generated frames/errors during that window. This candidate needs actual RTX 5070 Ti gameplay measurement before a performance claim. CI uses a private startup data directory and an updater fixture; the running installation and settings remain unchanged until the user installs the candidate.
