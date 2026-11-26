[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Exe,
    [switch]$Optimized,
    [switch]$RayTracing=$true
)
$ErrorActionPreference='Stop'
$exePath=(Resolve-Path -LiteralPath $Exe).Path
$gameDir=Split-Path -Parent $exePath
foreach($required in @('MicrosoftGame.Config','shaders.rpf','XboxBvhBuilder.dll','amd_fidelityfx_upscaler_xbox.dll')){
    if(-not (Test-Path -LiteralPath (Join-Path $gameDir $required))){throw "Game package incomplete beside executable: $required"}
}
$settings=@{
    VICTORY_D3D12_ADAPTER='RTX 5070 Ti'
    VICTORY_OUTPUT='1920x1080'
    VICTORY_WAVE_WIDE='0'
    VICTORY_HOST_RDNA='0'
}
$arguments=@{Exe=$exePath;Build='diag';More=$settings}
if(-not $Optimized){$arguments.Safe=$true}
if($RayTracing){$arguments.RayTracing=$true}
& (Join-Path $PSScriptRoot 'scripts\play.ps1') @arguments
