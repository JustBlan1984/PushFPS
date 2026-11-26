VIctory trial for Intel Core i9-14900K / NVIDIA RTX 5070 Ti

Public compatibility-layer source:
https://github.com/Belobug/VIctory-Belobug-AMD/tree/cd8dfc06ace7ccb91a19431e74bc2d0de2a10145
GPL-3.0-or-later; corresponding source at the pinned link. Cargo.lock and compiler identity included.
NO game executable, game data, proprietary console DLLs or shader archives are included or uploaded.
CPU instructions are selected for the 14900K and may not run on the CI runner or older PCs.
GPU code is Direct3D 12, with upstream NVIDIA paths and runtime capability checks. GPU performance is unverified until tested locally.

First start:
- Keep the supplied game executable beside the matching game archives and console DLLs.
- Set VICTORY_CONFIG to a local config file whose cache/data paths are OUTSIDE the source checkout.
- Run Start-VIctory-NVIDIA.ps1 -Exe <path to game_scarlett_master.exe>.
- Default is the upstream Safe launch with RTX 5070 Ti selected, 1080p output, experimental wide waves off, AMD numeric mode off and ray tracing off.
- -Optimized selects the upstream faster launch profile after the baseline boots.
- -RayTracing is an optional later comparison. No FSR 4-only or AMD vendor path is forced.
- All environment changes apply to the launched process, not persistent user/system environment.
- Run non-administratively. Saves and diagnostics belong in the configured local data directory.
- Inspect/configuration/shader translation must succeed before treating a game boot as verified.
- This project is experimental; upstream AMD benchmark figures do not establish NVIDIA performance.
