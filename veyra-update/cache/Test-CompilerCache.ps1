[CmdletBinding()]
param([switch]$RequireHit,[string]$ExpectedCompiler,[string]$ExpectedObject)
$ErrorActionPreference = 'Stop'
$root = 'D:\veyra-cache-probe'
$source = Join-Path $root 'source'
$build = Join-Path $root 'build'
New-Item -ItemType Directory -Force $source | Out-Null
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'CMakeLists.txt'),(Join-Path $PSScriptRoot 'cache_probe.cpp') -Destination $source
$compiler = (Get-Command cl.exe).Source
$compilerHash = (Get-FileHash -LiteralPath $compiler -Algorithm SHA256).Hash
if ($RequireHit -and $compilerHash -ne $ExpectedCompiler) { throw 'MSVC changed between runners; cross-run cache proof is inconclusive.' }
cmake -S $source -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER_LAUNCHER=sccache -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
sccache --zero-stats
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build $build --parallel 2
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$stats = (sccache --show-stats | Out-String)
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Write-Host $stats
$stats | Set-Content -LiteralPath (Join-Path $env:GITHUB_WORKSPACE 'compiler-cache-proof.txt') -Encoding UTF8
if ($stats -notmatch '(?im)^\s*Cache location\s+.*(?:GHA|GitHub)') { throw 'The probe is not using the GitHub Actions cache backend.' }
$hitMatch = [regex]::Match($stats,'(?m)^\s*Cache hits\s+([0-9]+)(?:\s|$)')
if (-not $hitMatch.Success) { throw 'Cannot read the actual sccache hit count.' }
$hits = [int]$hitMatch.Groups[1].Value
$objectPath = Join-Path $build 'CMakeFiles\cache_probe.dir\cache_probe.cpp.obj'
$objectHash = (Get-FileHash -LiteralPath $objectPath -Algorithm SHA256).Hash
if ($RequireHit -and $hits -lt 1) { throw 'A fresh runner did not reuse the cached compiler output.' }
if ($RequireHit -and $objectHash -ne $ExpectedObject) { throw 'Reused object differs from the first runner.' }
"compiler=$compilerHash" | Out-File -Append $env:GITHUB_OUTPUT
"object=$objectHash" | Out-File -Append $env:GITHUB_OUTPUT
"hits=$hits" | Out-File -Append $env:GITHUB_OUTPUT
if ($RequireHit) { Write-Host 'PASS: a fresh runner reused identical compiler output from the GitHub Actions cache.' }
