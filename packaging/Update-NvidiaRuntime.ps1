[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RuntimeRoot,
    [string]$CacheDirectory = (Join-Path $env:TEMP 'PushFPS-DLSS-310.9.16')
)
$ErrorActionPreference = 'Stop'
$commit = '9981c6350dc58c0ce21f024fd65e31e851c31689'
$version = '310.9.16'
$hashes = [ordered]@{
    'nvngx_dlss.dll' = 'D00B5B0AB29C3CA101B70C3A5ECB3A01B7EF33766A3383A28A18623083E6123E'
    'nvngx_dlssd.dll' = 'B49435AE346F227FEAFE7BBE937D88277EA0EE7323975EC06488F377FDB46CEE'
    'nvngx_dlssg.dll' = '8D59D49B36410DD6B62FEF2503A5871D347415BD85D943FA0848563A95DD50C7'
}
if (-not (Test-Path -LiteralPath $RuntimeRoot -PathType Container)) { throw 'Runtime staging root does not exist' }
$destination = Join-Path $RuntimeRoot 'runtime\experimental\dlss-current'
New-Item -ItemType Directory -Force $CacheDirectory | Out-Null

# Finish and verify every download before touching the private runtime stage.
# Preserve the base runtime DLLs: RTX 30/40 compatibility depends on those versions.
$verified = @()
foreach ($name in $hashes.Keys) {
    $cached = Join-Path $CacheDirectory $name
    if (-not (Test-Path -LiteralPath $cached -PathType Leaf) -or
        (Get-FileHash -LiteralPath $cached -Algorithm SHA256).Hash -ne $hashes[$name]) {
        $partial = Join-Path $CacheDirectory ($name + '.' + [guid]::NewGuid().ToString('N') + '.part')
        try {
            Invoke-WebRequest -UseBasicParsing -Uri "https://raw.githubusercontent.com/NVIDIA/DLSS/$commit/lib/Windows_x86_64/rel/$name" -OutFile $partial
            if ((Get-FileHash -LiteralPath $partial -Algorithm SHA256).Hash -ne $hashes[$name]) { throw "SHA-256 mismatch: $name" }
            Move-Item -LiteralPath $partial -Destination $cached -Force
        } finally {
            if (Test-Path -LiteralPath $partial) { Remove-Item -LiteralPath $partial -Force }
        }
    }
    $file = Get-Item -LiteralPath $cached
    if (($file.VersionInfo.FileVersion -replace ',\s*', '.') -ne "$version.0") { throw "Unexpected file version: $name" }
    $signature = Get-AuthenticodeSignature -LiteralPath $cached
    if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'O=NVIDIA Corporation') {
        throw "NVIDIA signature verification failed: $name"
    }
    $verified += [pscustomobject]@{
        name = $name; path = "runtime/experimental/dlss-current/$name"
        fileVersion = "$version.0"; sha256 = $hashes[$name]
        source = "https://github.com/NVIDIA/DLSS/blob/$commit/lib/Windows_x86_64/rel/$name"
    }
}
New-Item -ItemType Directory -Force $destination | Out-Null
foreach ($file in $verified) {
    Copy-Item -LiteralPath (Join-Path $CacheDirectory $file.name) -Destination (Join-Path $destination $file.name) -Force
    if ((Get-FileHash -LiteralPath (Join-Path $destination $file.name) -Algorithm SHA256).Hash -ne $file.sha256) { throw "Staged file differs: $($file.name)" }
}
@{schema=1; sdk=$version; sourceCommit=$commit; files=$verified; legacyRuntimePreserved=$true} |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $RuntimeRoot 'pushfps-nvidia-runtime.json') -Encoding UTF8
Write-Host "DLSS $version verified and staged; original compatibility runtimes preserved."
