# Compiler cache configuration

Compiler cache: sccache v0.20.1, Mozilla-Actions/sccache-action v0.0.11 pinned to fc920bf0ec8de6ee65d409111f7ec508035751ba.
The CMake C and C++ compiler launchers use sccache. Cache storage is the GitHub Actions backend, with the veyra-msvc-v1 namespace.

Verification: https://github.com/JustBlan1984/PushFPS/actions/runs/45759023670
Two separate windows-2022 jobs used the same MSVC compiler and a fixed-path CMake object target.
The second runner reported 1 C/C++ cache hit and 0 misses, GitHub Actions cache storage, and the same compiled-object SHA-256 as the first runner.
This proves cross-runner cache reuse for the probe; it does not claim a measured full-Veyra speedup.

The first full build with this configuration must populate the application's object cache. Later builds reuse matching objects.
Compiler flags, source/header contents, compiler and SDK changes are allowed to invalidate entries normally; no unsafe cache-key shortcuts are enabled.
Version-stamped translation units, shaders, generated resources, linking, downloads and uncached dependencies can still take time.
Full build logs and a separate statistics artifact report actual cache hits and misses.

vcpkg binary caches now use compiler/Windows SDK and recipe fingerprints instead of a fresh key for every run.
The old dependency-cache prefix remains a restore fallback. Existing caches were not deleted.

The configuration is applied after the completed engine-optimization build.
No source or binary change is needed in the user's running app to enable caching of future CI builds.
The installer completion text now reports the manifest's actual build version rather than the old SDK-only message.
