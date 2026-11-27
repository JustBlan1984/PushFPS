[CmdletBinding()]
param(
    [string]$Exe=(Join-Path $env:USERPROFILE 'Downloads\GTA6.Xbox.v1.0.2.1-CYBERLEEK\game_scarlett_master.exe'),
    [ValidateSet('Play','PrepareCache')][string]$Action='Play',
    [ValidateSet('Optimized','Safe')][string]$Mode='Optimized',
    [switch]$Windowed,
    [switch]$Profile,
    [switch]$CompareRefresh,
    [switch]$CompareCopyOrder,
    [switch]$RayTracing=$true
)
$ErrorActionPreference='Stop'
if($CompareCopyOrder -and ($CompareRefresh -or $Profile -or $Action -ne 'Play')){
    throw 'Use -CompareCopyOrder by itself on a gameplay run.'
}
$binaryProfile=if($Profile -or $CompareRefresh -or $CompareCopyOrder -or $Action -eq 'PrepareCache'){'diag'}else{'release'}
$runtime=Join-Path $PSScriptRoot "target\$binaryProfile\victory.exe"
if(-not (Test-Path -LiteralPath $runtime)){throw "Missing runtime: $runtime"}
$exePath=(Resolve-Path -LiteralPath $Exe).Path
$gameDir=Split-Path -Parent $exePath
foreach($required in @('MicrosoftGame.Config','shaders.rpf','XboxBvhBuilder.dll','amd_fidelityfx_upscaler_xbox.dll')){
    if(-not (Test-Path -LiteralPath (Join-Path $gameDir $required))){throw "Game package incomplete beside executable: $required"}
}
$logs=Join-Path $PSScriptRoot 'logs'
New-Item -ItemType Directory -Force $logs | Out-Null
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
$log=Join-Path $logs "$Action-$Mode-$stamp.log"
# Keep the user's existing cache and saves; never change global environment.
if(-not $env:VICTORY_CACHE_DIR){$env:VICTORY_CACHE_DIR=Join-Path $env:USERPROFILE 'AppData\Local\victory\cache'}
if(-not $env:VICTORY_DATA_DIR){$env:VICTORY_DATA_DIR=Join-Path $env:USERPROFILE 'AppData\Local\victory\data'}
$preserve=@('VICTORY_CONFIG','VICTORY_GAME_DIR','VICTORY_CACHE_DIR','VICTORY_DATA_DIR','VICTORY_FIDELITYFX_DIR')
Get-ChildItem Env:VICTORY_* | Where-Object {$_.Name -notin $preserve} | ForEach-Object {Remove-Item "Env:$($_.Name)"}
$settings=@{
    VICTORY_D3D12_ADAPTER='RTX 5070 Ti'; VICTORY_OUTPUT='2560x1440'
    VICTORY_WAVE_WIDE='0'; VICTORY_HOST_RDNA='0'
    VICTORY_FULLSCREEN=$(if($Windowed){'0'}else{'1'})
    VICTORY_TEXTURE_NO_WAIT='1'; VICTORY_RELAY='2'; VICTORY_WIDE_BACK='1'
    VICTORY_CARD_REHASH_RUNS='1'; VICTORY_JOIN_COPIES='1'; VICTORY_INDIRECT_WINDOW='1'
    VICTORY_RELAY_COPY='1'; VICTORY_CARD_STEPS='2'; VICTORY_INDIRECT_BATCH='2'
    VICTORY_MAP_THREADS='1'; VICTORY_TITLE_PRIORITIES='0'; VICTORY_CARD_AWAKE='1'
    VICTORY_SHOW_AFTER='1'; VICTORY_AMBIENT='0.8'; VICTORY_D3D12_DRED='light'
}
if($Mode -eq 'Safe'){
    $settings.VICTORY_UNDER_WIDE='0'; $settings.VICTORY_LAND_ECHO='0'
}else{
    # The pinned fork's faster profile; residency guards remain enabled.
    $settings.VICTORY_THIN='1'; $settings.VICTORY_JOIN_PARTS='8'; $settings.VICTORY_UPLOAD_AHEAD='1'
    # Let the GPU use the shared graphics constants heap when its driver accepts it.
    $settings.VICTORY_PORT_CONSTANTS='1'
    $settings.VICTORY_FRAME_INTERVAL_MS='16.667'
}
if($RayTracing){$settings.VICTORY_BVH_BUILDER='1'}
if($Action -eq 'Play'){
    $settings.VICTORY_PIPELINE_BOOK='1'
    $settings.VICTORY_TIMER_LOG='1'
    $settings.VICTORY_MEMORY_CATEGORIES='1'
    if($CompareCopyOrder){
        $settings.VICTORY_AB='20,VICTORY_PLACED_ORDER'
        Write-Host 'Copy-order comparison enabled. Hold the same view for two minutes in gameplay.'
    }
    if($CompareRefresh){
        $settings.VICTORY_AB='20,VICTORY_PORT_GUARD'
        Write-Host 'Refresh comparison enabled. Keep the same gameplay scene for at least two minutes.'
    }
    if($Profile){$settings.VICTORY_PROFILE="60,120,20,$logs\profile-$stamp.txt"}
}
foreach($key in $settings.Keys){Set-Item "Env:$key" $settings[$key]}
Write-Host "VIctory: RTX 5070 Ti, 2560x1440, ray tracing=$RayTracing, profile=$Mode, executable=$binaryProfile"
Write-Host "Cache: $env:VICTORY_CACHE_DIR"
Write-Host "Log: $log"
Write-Host 'Run from a normal, non-administrator terminal. Leave this window open.'
Push-Location $PSScriptRoot
try{
    # Native stderr carries INFO messages too. Continue prevents Windows
    # PowerShell's NativeCommandError wrapper from stopping the pipeline.
    $ErrorActionPreference='Continue'
    if($Action -eq 'PrepareCache'){
        & $runtime shaders (Join-Path $gameDir 'shaders.rpf') --jobs 12 2>&1 | Tee-Object -FilePath $log
        if($LASTEXITCODE -ne 0){throw "Shader precompile failed: $LASTEXITCODE"}
        $book=Join-Path $env:VICTORY_CACHE_DIR 'pipelines\known-pipelines.bin'
        if(Test-Path -LiteralPath $book){
            & $runtime warm --used --jobs 4 2>&1 | Tee-Object -FilePath $log -Append
            if($LASTEXITCODE -ne 0){throw "Pipeline warm-up failed: $LASTEXITCODE"}
        }else{
            Write-Host 'No recorded pipeline book yet. Play once with this launcher, close the game, then run PrepareCache again.'
        }
    }else{
        & $runtime run $exePath --execute 2>&1 | Tee-Object -FilePath $log
        if($LASTEXITCODE -ne 0){throw "VIctory exited with code $LASTEXITCODE; see $log"}
    }
}finally{Pop-Location}
