<#
  Builds the PushFPS release zip from a finished build (used by .github/workflows/build-windows.yml).

  Zip layout:
    Install-PushFPS.cmd        double-click to install
    Install-PushFPS.ps1        the installer (Veyra runtime package hashes pinned in it)
    READ ME FIRST.txt
    PushFPS-files\             PushFPS.exe, its QML interface, shaders and licence files
    PushFPS-files\runtime\experimental\pushflow\   PushFlow: ONNX Runtime + DirectML and the RIFE models
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BuildDir,
    [Parameter(Mandatory)][string]$Version,
    [Parameter(Mandatory)][string]$UpstreamRepo,
    [Parameter(Mandatory)][string]$UpstreamTag,
    [Parameter(Mandatory)][string]$UpstreamSums,
    [string]$OutDir = 'dist',
    # PushFlow's runtime (scripts/pushflow/prepare_runtime.py). Without it PushFlow reports itself unavailable.
    [string]$PushFlowDir = ''
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot

# SHA-256 of Veyra's runtime packages, from the release's own SHA256SUMS.txt.
$upstreamVersion = $UpstreamTag.TrimStart('v')
$sums = @{}
foreach ($line in Get-Content $UpstreamSums) {
    if ($line -match '^([0-9a-fA-F]{64})\s+\*?(.+?)\s*$') { $sums[$Matches[2]] = $Matches[1].ToUpperInvariant() }
}
$hash = @{}
foreach ($gpu in 'NVIDIA', 'AMD') {
    $name = "Veyra-$upstreamVersion-$gpu-win64-portable.zip"
    if (-not $sums.ContainsKey($name)) { throw "SHA256SUMS.txt has no entry for $name" }
    $hash[$gpu] = $sums[$name]
}

$name = "PushFPS-$Version-win64"
# The exact source of this build: its release tag, else the commit CI built, else the main branch.
$sourceRef = if ($Version -match '^\d+\.\d+\.\d+$') { "v$Version" } elseif ($env:GITHUB_SHA) { $env:GITHUB_SHA } else { 'main' }
$stage = Join-Path $OutDir $name
$files = Join-Path $stage 'PushFPS-files'
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force $files | Out-Null

# The program and everything built next to it that the runtime package does not already have newer.
Copy-Item (Join-Path $BuildDir 'PushFPS.exe') $files
# PushFlowBench: PushFlow timings on the player's graphics card (docs/PUSHFLOW.md), when it was built.
$bench = Join-Path $BuildDir 'PushFlowBench.exe'
if (Test-Path $bench) { Copy-Item $bench $files } else { Write-Warning 'PushFlowBench.exe not built: not packaged' }
Copy-Item (Join-Path $repo 'qml') $files -Recurse
$shaders = Join-Path $BuildDir 'shaders'
if (-not (Test-Path $shaders)) { throw "Compiled shaders not found in $shaders" }
Copy-Item $shaders $files -Recurse
# Dolby Vision metadata parsing (libdovi, MIT), when this build has it: the library next to the program and its licence.
$dovi = Join-Path $BuildDir 'dolby_vision.dll'
if (Test-Path $dovi) {
    Copy-Item $dovi $files
    if ($env:LIBDOVI_ROOT -and (Test-Path (Join-Path $env:LIBDOVI_ROOT 'LICENSE'))) {
        New-Item -ItemType Directory -Force (Join-Path $repo 'licenses\libdovi') | Out-Null
        Copy-Item (Join-Path $env:LIBDOVI_ROOT 'LICENSE') (Join-Path $repo 'licenses\libdovi\LICENSE.txt')
    }
}
# The virtual camera: PushFPS's own DirectShow servers for 64 and 32 bit programs and their setup helper
# (src/virtualcam; licences in licenses\virtual-camera, copied with the licenses folder below). Nothing is
# registered by installing PushFPS: the user turns the camera on in Settings.
$camera = Join-Path $BuildDir 'virtualcam'
$cameraFiles = 'pushfps-virtualcam64.dll', 'pushfps-virtualcam32.dll', 'pushfps-camera-setup.exe'
if (@($cameraFiles | Where-Object { -not (Test-Path (Join-Path $camera $_)) }).Count -eq 0) {
    $cameraOut = Join-Path $files 'virtualcam'
    New-Item -ItemType Directory -Force $cameraOut | Out-Null
    foreach ($f in $cameraFiles) { Copy-Item (Join-Path $camera $f) $cameraOut }
} else { Write-Warning 'Virtual camera components not built: not packaged (the app then says the camera is unavailable)' }
foreach ($doc in 'LICENSE', 'NOTICE.md', 'THIRD_PARTY_NOTICES.md') { Copy-Item (Join-Path $repo $doc) $files }
Copy-Item (Join-Path $repo 'README.md') (Join-Path $files 'PushFPS-README.md')
Copy-Item (Join-Path $repo 'licenses') $files -Recurse
if ($PushFlowDir) {
    foreach ($f in 'onnxruntime.dll', 'DirectML.dll', 'models\pushflow-fast.onnx', 'models\pushflow-quality.onnx') {
        if (-not (Test-Path (Join-Path $PushFlowDir $f))) { throw "PushFlow runtime incomplete: $f missing in $PushFlowDir" }
    }
    $pushFlow = Join-Path $files 'runtime\experimental\pushflow'
    New-Item -ItemType Directory -Force $pushFlow | Out-Null
    Copy-Item (Join-Path $PushFlowDir '*') $pushFlow -Recurse
    Get-ChildItem $pushFlow -Recurse -File | Format-Table FullName, Length
} else {
    Write-Warning 'No -PushFlowDir: this package has no PushFlow runtime (PushFlow will report itself unavailable).'
}

# Installer with the pinned values filled in.
$installer = Get-Content (Join-Path $PSScriptRoot 'Install-PushFPS.ps1') -Raw
$installer = $installer.Replace('@PUSHFPS_VERSION@', $Version)
$installer = $installer.Replace('@UPSTREAM_REPO@', $UpstreamRepo)
$installer = $installer.Replace('@UPSTREAM_TAG@', $UpstreamTag)
$installer = $installer.Replace('@SHA256_NVIDIA@', $hash['NVIDIA'])
$installer = $installer.Replace('@SHA256_AMD@', $hash['AMD'])
if ($installer -match '@[A-Z0-9_]+@') { throw "Unfilled placeholder in installer: $($Matches[0])" }
# UTF-8 with BOM so Windows PowerShell 5.1 reads non-ASCII text correctly.
[IO.File]::WriteAllText((Join-Path $stage 'Install-PushFPS.ps1'), $installer, (New-Object Text.UTF8Encoding $true))
Copy-Item (Join-Path $PSScriptRoot 'Install-PushFPS.cmd') $stage
Copy-Item (Join-Path $PSScriptRoot 'Update-NvidiaRuntime.ps1') $stage

# Where the installer puts this build (Install-PushFPS.ps1): a release in the usual folder, a test build beside it.
$installsTo = 'installs PushFPS to %LOCALAPPDATA%\Programs\PushFPS with Start menu and desktop shortcuts.'
$removeBy = 'or delete %LOCALAPPDATA%\Programs\PushFPS and the PushFPS shortcuts.'
if ($Version -notmatch '^\d+\.\d+\.\d+$') {
    $installsTo = 'installs this test build to %LOCALAPPDATA%\Programs\PushFPS-test with shortcuts of its own ("PushFPS test"). The PushFPS you have is not changed; the test build starts with a copy of its settings.'
    $removeBy = 'or delete %LOCALAPPDATA%\Programs\PushFPS-test and the "PushFPS test" shortcuts.'
}

@"
PushFPS $Version
================

Install
  1. Extract this whole zip to a folder (right-click, Extract All).
  2. Double-click Install-PushFPS.cmd.

  The installer downloads Veyra's official runtime package (about 750 MB, NVIDIA or AMD to match your
  graphics card) from https://github.com/$UpstreamRepo/releases/tag/$UpstreamTag , checks its SHA-256,
  and $installsTo
  No administrator rights are needed.

  Windows may warn that the file comes from the internet or an unknown publisher (PushFPS is not code
  signed). Choose "More info" and "Run anyway" if you downloaded this zip from
  https://github.com/DosukaSOL/PushFPS/releases .

Uninstall
  Run Install-PushFPS.cmd -Uninstall from a command prompt in this folder,
  $removeBy

Pinned runtime package hashes (SHA-256)
  NVIDIA: $($hash['NVIDIA'])
  AMD:    $($hash['AMD'])

License
  PushFPS is free software under the GNU GPL-3.0 and comes with ABSOLUTELY NO WARRANTY. It is a modified
  version of Veyra by Likely7 (https://github.com/Likely7/Veyra-NRVideo). The license, the modification
  notice and third-party notices are in PushFPS-files (LICENSE, NOTICE.md, THIRD_PARTY_NOTICES.md).
  Source code of this build: https://github.com/$($env:GITHUB_REPOSITORY)/tree/$sourceRef
"@ | Set-Content -Path (Join-Path $stage 'READ ME FIRST.txt') -Encoding UTF8

$zip = Join-Path $OutDir "$name.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal
$sha = (Get-FileHash $zip -Algorithm SHA256).Hash
"$sha  $name.zip" | Set-Content -Path (Join-Path $OutDir "$name.zip.sha256") -Encoding ASCII
Write-Host "Created $zip"
Write-Host "SHA-256 $sha"
