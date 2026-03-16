#Requires -Version 5.1
<#
.SYNOPSIS
    Configures and builds the Librium project using CMake presets.

.PARAMETER Preset
    The CMake preset to use (e.g., x64-debug, x64-release). Default: x64-debug

.PARAMETER Clean
    If set, removes the build directory before configuring.

.EXAMPLE
    .\Build.ps1
    .\Build.ps1 -Preset x64-release
    .\Build.ps1 -Clean
#>
param(
    [string] $Preset = "x64-debug",
    [switch] $Clean
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# Determine Config from preset name (ends with -debug or -release)
$Config = "Debug"
if ($Preset -like "*-release") { $Config = "Release" }

$RepoRoot = $PSScriptRoot
$BuildDir = Join-Path $RepoRoot "out\build\$Preset"

Write-Host ""
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Librium -- Build Script"                                 -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Preset : $Preset"                                        -ForegroundColor DarkGray
Write-Host "  Config : $Config"                                        -ForegroundColor DarkGray
Write-Host ""

if ($Clean) {
    if (Test-Path $BuildDir) {
        Write-Host "  [CLEAN] Removing build directory: $BuildDir" -ForegroundColor Yellow
        Remove-Item -Recurse -Force $BuildDir
    }
}

# 1. Configure
Write-Host "  [1/2] Configuring project..." -ForegroundColor Cyan
cmake --preset $Preset

# 2. Build
Write-Host ""
Write-Host "  [2/2] Building all targets..." -ForegroundColor Cyan
cmake --build --preset $Preset --config $Config

Write-Host ""
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  BUILD COMPLETE"                                          -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host ""
