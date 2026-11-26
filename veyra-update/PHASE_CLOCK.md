# Content-clock phase coordinate correction

The installed engine-opt.15 baseline learns readiness relative to the smoothed content anchor, but enforces admission's phase floor relative to the raw callback. Final presentation adds that selected phase to the content anchor. Positive content-anchor offsets can therefore be reserved twice.

This change uses the same per-pair content anchor for admission, the required phase and final presentation, limited to the existing content-clock path.
The required phase solves the actual generated deadline inequality. Existing GPU cost estimates, phase cap, 0.5 ms readiness margin, 0-1 ms learned extra, slew limit, ordering, queue bounds, reset rules and source timestamps remain.
A zero required-delay observation is valid when the content anchor already supplies the necessary time; the learner still retains its readiness margin.
No negative phase is introduced. Transport-anchored and provider-owned paths retain their previous floor calculation.

New CPU tests use the real PresentationScheduler to cover positive/negative clock offsets, interpolation spacing, readiness feasibility, insufficient phase caps, zero-delay learning and downward slew.
A separate Python arithmetic check passed 100000 deterministic random feasibility cases before compilation. This is not GPU or gameplay evidence.
The new live-phase-clock diagnostic reports the predicted readiness bound, actual generated deadline and anchor offset once per second.

Baseline in the user's Medium 2X/HDR/uncapped race: approximately 60 submitted FPS and 27.6 ms representative reported P95 capture-callback-to-Present-return latency over 94 seconds, no added capture drops/expired generated frames/errors in that sample.
Performance after this correction is unmeasured until the new candidate is installed and run on the user's RTX 5070 Ti.
The existing installed build and settings are unchanged by CI. No backups or rollback files are created by the updater.
