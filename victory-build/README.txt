VIctory 14900K / RTX 5070 Ti trial - revision 3

Source: https://github.com/Belobug/VIctory-Belobug-AMD/tree/cd8dfc06ace7ccb91a19431e74bc2d0de2a10145
License: GPL-3.0-or-later. Local modifications are supplied in fullscreen.patch.
The build-scripts commit is in build-manifest.json. No game files are supplied.

Extract this package over your existing VIctory-14900K-5070Ti folder while
VIctory is closed. Keep the target and scripts subfolders in place. Existing
shader/pipeline caches and saves under AppData\Local\victory are reused.

Launch-VIctory-1440p-RT.cmd:
  2560x1440 output, RTX 5070 Ti, ray tracing ON, borderless fullscreen with
  centered aspect-preserving presentation and black side bars on ultrawide.
  Uses the pinned fork's faster profile (THIN=1, JOIN_PARTS=8, UPLOAD_AHEAD=1).
  This is an upstream optimization profile, not a verified NVIDIA speedup.
  Memory-residency protection remains enabled. Alt+Tab returns to the desktop.
  Fullscreen uses the original Americas window on the Windows primary display.
  To compare memory refresh strategies, open a terminal in this folder and run
    .\Launch-VIctory-1440p-RT.cmd -CompareRefresh
  Play the same scene for at least two minutes. The log prints median and p90
  frame times for alternating 20-second phases with the guard off and on.
  For the separate copy-order comparison use the same launcher:
    .\Launch-VIctory-1440p-RT.cmd -CompareCopyOrder
  This tests whether the submission can proceed when a conflicting copy comes
  later in command order. Compare one switch at a time during a steady scene.

Launch-VIctory-Safe-1440p-RT.cmd:
  Same output, ray tracing and fullscreen, with the earlier conservative
  upstream profile for comparison. Quality and resolution remain the same.

Prepare-VIctory-Cache.cmd:
  Translates shaders.rpf using the same NVIDIA wave/key settings as play.
  Also warms previously recorded, used pipelines, with four worker processes.
  The old launcher did not record a pipeline book. Play once with revision 2,
  close the game, then prepare the cache again for offline pipeline warm-up.
  Do not run cache preparation and the game at the same time.

Launch-VIctory-Profile-1440p-RT.cmd:
  Optimized profile plus optional CPU stack sampling from 60 to 120 seconds.
  Sampling adds some overhead; use it for diagnosis, not FPS comparisons.

Every launcher saves a timestamped run log in this package's logs folder.
Logs report timing, shader/pipeline cache activity, and memory categories.
Keep all logs and shader/pipeline files local; they may contain game details.

Game executable default:
  %USERPROFILE%\Downloads\GTA6.Xbox.v1.0.2.1-CYBERLEEK\game_scarlett_master.exe
It must be beside its matching game archives and console DLLs.
For a different path, use Start-VIctory-NVIDIA.ps1 -Exe <path>.
For a normal window, add -Windowed to the PowerShell launcher.
Launch normally, not as administrator, to keep driver-cache behavior consistent.

This compatibility layer is experimental. CPU-targeted compilation, cache
preparation and an NVIDIA-selected adapter do not guarantee stable rendering
or a particular frame rate. Ray tracing remains experimental on this GPU.
