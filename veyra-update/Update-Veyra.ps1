[CmdletBinding()]
param(
    [string]$InstallRoot = (Join-Path $env:LOCALAPPDATA 'Programs\Veyra (NVIDIA)'),
    [switch]$Rollback
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$root = [IO.Path]::GetFullPath($InstallRoot).TrimEnd('\')
if (-not (Test-Path -LiteralPath (Join-Path $root 'current-version.txt'))) { throw 'Veyra installation not found.' }
$version = (Get-Content -LiteralPath (Join-Path $root 'current-version.txt') -Raw).Trim()
if ($version -ne '2.2.1') { throw 'This update is for Veyra 2.2.1 only.' }
$app = Join-Path $root 'versions\2.2.1'
foreach ($dir in @($root,(Join-Path $root 'versions'),$app)) {
    if ((Get-Item -LiteralPath $dir).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'A linked installation needs manual review.' }
}
$running = Get-Process Veyra,veyra_qml_ui,veyra_update_worker -ErrorAction SilentlyContinue | Where-Object {
    try { $_.Path -and $_.Path.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase) } catch { $true }
}
if ($running) { throw 'Close Veyra normally, then run this update again. Your session will not be stopped automatically.' }
$backupRoot = Join-Path $root 'sdk-backups'
function TargetPath([string]$relative) {
    $target = [IO.Path]::GetFullPath((Join-Path $app $relative))
    if (-not $target.StartsWith($app+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid update path.' }
    $probe = Split-Path -Parent $target
    while ($probe -and $probe.StartsWith($app,[StringComparison]::OrdinalIgnoreCase)) {
        if ((Test-Path -LiteralPath $probe) -and ((Get-Item -LiteralPath $probe).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'A linked target needs manual review.' }
        if ($probe -eq $app) { break }
        $probe = Split-Path -Parent $probe
    }
    if ((Test-Path -LiteralPath $target) -and ((Get-Item -LiteralPath $target).Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'A linked file needs manual review.' }
    return $target
}
function RestoreFiles($backup,$receipt) {
    foreach ($file in $receipt.files) {
        $target = TargetPath $file.path
        if ($file.existed) {
            $original = Join-Path $backup ('original\'+$file.path)
            if ((Get-FileHash -LiteralPath $original).Hash -ne $file.originalSha256) { throw 'Backup verification failed.' }
            Copy-Item -LiteralPath $original -Destination $target -Force
        } elseif (Test-Path -LiteralPath $target) {
            Remove-Item -LiteralPath $target -Force
        }
    }
}
if ($Rollback) {
    $backup = Get-ChildItem -LiteralPath $backupRoot -Directory | Sort-Object Name -Descending | Select-Object -First 1
    if (-not $backup) { throw 'No SDK update backup was found.' }
    $receipt = Get-Content -LiteralPath (Join-Path $backup.FullName 'receipt.json') -Raw | ConvertFrom-Json
    RestoreFiles $backup.FullName $receipt
    Write-Host 'Original Veyra files restored. Settings and driver were not changed.'
    exit 0
}
$gpus = @(Get-CimInstance Win32_VideoController | Where-Object { $_.Name -match 'NVIDIA.*RTX\s*50\d\d' })
if (-not $gpus.Count) { throw 'This native DLSS runtime update is prepared for RTX 50-series GPUs.' }
foreach ($gpu in $gpus) {
    $parts = $gpu.DriverVersion.Split('.')
    if ($parts.Count -ne 4 -or [int](($parts[2]+$parts[3]).Substring(($parts[2]+$parts[3]).Length-5)) -lt 61000) {
        throw 'Video Codec SDK 13.1 requires NVIDIA Windows driver 610 or newer.'
    }
}
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$payload = Join-Path $here 'payload'
$manifest = Get-Content -LiteralPath (Join-Path $here 'update-manifest.json') -Raw | ConvertFrom-Json
$entries = @()
foreach ($file in $manifest.files) {
    if ($file.path -notmatch '^(veyra_qml_ui\.exe|shaders[/\\].+\.dxil|qml[/\\].+|i18n[/\\].+)$') { throw 'Unexpected payload file.' }
    $null = TargetPath $file.path
    $source = [IO.Path]::GetFullPath((Join-Path $payload $file.path))
    if (-not $source.StartsWith($payload+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid payload source.' }
    if ((Get-FileHash -LiteralPath $source).Hash -ne $file.sha256) { throw "Payload verification failed: $($file.path)" }
    $entries += [pscustomobject]@{path=$file.path;source=$source;sha256=$file.sha256}
}
if (-not @($entries | Where-Object { $_.path -eq 'veyra_qml_ui.exe' }).Count) { throw 'Updated executable missing.' }
$stage = Join-Path $env:TEMP ('Veyra-sdk-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage | Out-Null
try {
    $commit = '9981c6350dc58c0ce21f024fd65e31e851c31689'
    $dlls = [ordered]@{
        'nvngx_dlss.dll'='D00B5B0AB29C3CA101B70C3A5ECB3A01B7EF33766A3383A28A18623083E6123E'
        'nvngx_dlssg.dll'='8D59D49B36410DD6B62FEF2503A5871D347415BD85D943FA0848563A95DD50C7'
    }
    foreach ($name in $dlls.Keys) {
        $source = Join-Path $stage $name
        Invoke-WebRequest -UseBasicParsing -Uri "https://raw.githubusercontent.com/NVIDIA/DLSS/$commit/lib/Windows_x86_64/rel/$name" -OutFile $source -TimeoutSec 180
        if ((Get-FileHash -LiteralPath $source).Hash -ne $dlls[$name]) { throw "NVIDIA download hash mismatch: $name" }
        $sig = Get-AuthenticodeSignature -LiteralPath $source
        if ($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Subject -notmatch 'O=NVIDIA Corporation') { throw "NVIDIA signature check failed: $name" }
        if (((Get-Item -LiteralPath $source).VersionInfo.FileVersion -replace ',\s*','.') -ne '310.9.16.0') { throw 'Unexpected NVIDIA DLL version.' }
        $entries += [pscustomobject]@{path=('runtime/experimental/'+$name);source=$source;sha256=$dlls[$name]}
    }
    $backup = Join-Path $backupRoot ((Get-Date -Format 'yyyyMMdd-HHmmss')+'-'+[guid]::NewGuid().ToString('N').Substring(0,6))
    New-Item -ItemType Directory -Path $backup -Force | Out-Null
    $records = @()
    foreach ($file in $entries) {
        $target = TargetPath $file.path
        $existed = Test-Path -LiteralPath $target -PathType Leaf
        $hash = ''
        if ($existed) {
            $hash = (Get-FileHash -LiteralPath $target).Hash
            $original = Join-Path $backup ('original\'+$file.path)
            New-Item -ItemType Directory -Path (Split-Path -Parent $original) -Force | Out-Null
            Copy-Item -LiteralPath $target -Destination $original
            if ((Get-FileHash -LiteralPath $original).Hash -ne $hash) { throw 'Backup copy differs from original.' }
        }
        $records += [pscustomobject]@{path=$file.path;existed=$existed;originalSha256=$hash;updatedSha256=$file.sha256}
    }
    $receipt = @{baseVersion='2.2.1';dlssSdk='310.9.16';videoCodecSdk='13.1.15';files=$records;source=$manifest.source}
    $receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $backup 'receipt.json') -Encoding UTF8
    try {
        foreach ($file in $entries) {
            $target = TargetPath $file.path
            New-Item -ItemType Directory -Path (Split-Path -Parent $target) -Force | Out-Null
            Copy-Item -LiteralPath $file.source -Destination $target -Force
            if ((Get-FileHash -LiteralPath $target).Hash -ne $file.sha256) { throw 'Installed file verification failed.' }
        }
    } catch {
        RestoreFiles $backup $receipt
        throw 'Update failed; original files were restored.'
    }
    Write-Host "Veyra updated. Backup: $backup"
    Write-Host 'DLSS 310.9.16 and Video Codec SDK 13.1.15. Your settings, VFG engine, patched FFmpeg and driver were preserved.'
} finally {
    $fullStage = [IO.Path]::GetFullPath($stage)
    $tempPrefix = [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\')+'\Veyra-sdk-'
    if ($fullStage.StartsWith($tempPrefix,[StringComparison]::OrdinalIgnoreCase)) { Remove-Item -LiteralPath $fullStage -Recurse -Force }
}
