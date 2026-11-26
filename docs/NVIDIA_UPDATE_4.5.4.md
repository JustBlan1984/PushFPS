# Full PushFPS 4.5.4 NVIDIA update

Base: official v4.5.4, eb85717f34c504ee9475f86ae61a82696d512e26.
This is a full build, with recording and streaming retained.

- Build the direct-NGX integration with NVIDIA DLSS SDK v310.9.16, commit 9981c6350dc58c0ce21f024fd65e31e851c31689.
- Download the matching release DLLs directly from that immutable NVIDIA commit during NVIDIA installation. Verify SHA-256, file version and NVIDIA signature before copying into the private runtime stage. No proprietary binaries or headers are committed or republished.
- Use runtime/experimental/dlss-current as the NGX priority path on native NVIDIA configurations, including RTX 5070 Ti, for both normal and performance-mode frame generation.
- Retain RTX 30/40 version-specific compatibility paths. Preserve the original DLLs; incomplete new directories do not replace the working default.
- Keep VFG SDK 1.3.0, FFmpeg 9.0.1 ABI, capture, HDR and temporal input contracts.
- Include a CPU runtime-selection regression test and installer validation in CI. Hardware SR/FG quality, HDR and end-to-end latency are not verified by CPU CI.
- Use a non-release version and the existing separate PushFPS-test installation behavior. Official PushFPS remains available.

Validation pending: GitHub Actions full build, runtime verification, startup and CPU tests.
