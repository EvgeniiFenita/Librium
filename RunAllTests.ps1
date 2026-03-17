#Requires -Version 5.1
<#
.SYNOPSIS
    Runs all project tests: Catch2 unit tests and integration E2E tests.

.PARAMETER Preset
    CMake preset name (e.g., x64-debug, x64-release). Default: x64-debug

.PARAMETER KeepData
    Do not delete test data after integration tests.

.PARAMETER UnitOnly
    Run Catch2 unit tests only.

.PARAMETER IntegrationOnly
    Run integration E2E tests only.
#>
param(
    [string] $Preset          = "x64-debug",
    [switch] $KeepData,
    [switch] $UnitOnly,
    [switch] $IntegrationOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# Determine Config from preset name
$Config = "Debug"
if ($Preset -like "*-release") { $Config = "Release" }

$RepoRoot       = $PSScriptRoot
$BuildDir       = Join-Path $RepoRoot "out\build\$Preset"

# Executables
$UnitTestExe    = Join-Path $BuildDir "tests\Unit\$Config\UnitTests.exe"
$LibriumExe     = Join-Path $BuildDir "apps\Librium\$Config\Librium.exe"
$IntegrationDir = Join-Path $RepoRoot "tests\Integration"
$UnitDir        = Join-Path $RepoRoot "tests\Unit"

# ------------------------------------------------------------------
# Banner
# ------------------------------------------------------------------
Write-Host ""
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Librium -- Test Runner"                                  -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Preset    : $Preset"                                     -ForegroundColor DarkGray
Write-Host "  Config    : $Config"                                     -ForegroundColor DarkGray
Write-Host "  Build dir : $BuildDir"                                   -ForegroundColor DarkGray
Write-Host ""

# Find Python
$py = ""
foreach ($t in @("python", "python3")) {
    $cmd = Get-Command $t -ErrorAction SilentlyContinue
    if ($cmd -and (Get-Item $cmd.Source).Length -gt 0) {
        $py = $cmd.Source
        break
    }
}

# Result tracking
$script:UnitPassed = $false
$script:IntPassed  = $false
$overallOk         = $true

# ------------------------------------------------------------------
# 1. Generate Unit Test Data
# ------------------------------------------------------------------
if (-not $IntegrationOnly)
{
    Write-Host "--- Generating unit test data..." -ForegroundColor Gray
    $genScript = Join-Path $UnitDir "CreateTestData.py"
    $unitDataDir = Join-Path $BuildDir "tests\Unit\data"
    
    if (-not (Test-Path $unitDataDir)) { New-Item -ItemType Directory -Path $unitDataDir -Force | Out-Null }
    
    if ($py) {
        Start-Process -FilePath $py -ArgumentList $genScript, $unitDataDir -Wait -NoNewWindow
    } else {
        Write-Host "  [WARN] Python not found, skipping data generation. Tests might fail." -ForegroundColor Yellow
    }
}

# ------------------------------------------------------------------
# 2. Catch2 unit tests
# ------------------------------------------------------------------
if (-not $IntegrationOnly)
{
    Write-Host "----------------------------------------------------------" -ForegroundColor Cyan
    Write-Host "  UNIT TESTS  (Catch2 - UnitTests.exe)"            -ForegroundColor White
    Write-Host "----------------------------------------------------------" -ForegroundColor Cyan
    Write-Host ""

    if (-not (Test-Path $UnitTestExe)) {
        Write-Host "[ERROR] Unit test executable not found: $UnitTestExe" -ForegroundColor Red
        $overallOk = $false
    } else {
        $unitProc = Start-Process -FilePath $UnitTestExe `
                                  -ArgumentList "--colour-mode","ansi"`
                                  -Wait -PassThru -NoNewWindow

        Write-Host ""
        if ($unitProc.ExitCode -eq 0) {
            $script:UnitPassed = $true
        } else {
            $overallOk = $false
        }
    }
}

# ------------------------------------------------------------------
# 3. Python integration tests
# ------------------------------------------------------------------
if (-not $UnitOnly)
{
    Write-Host ""
    Write-Host "----------------------------------------------------------" -ForegroundColor Cyan
    Write-Host "  INTEGRATION TESTS  (Python - RunIntegrationTests.py)"   -ForegroundColor White
    Write-Host "----------------------------------------------------------" -ForegroundColor Cyan

    if (-not $py) { 
        Write-Host "  [ERROR] Python not found" -ForegroundColor Red
        $overallOk = $false
    } elseif (-not (Test-Path $LibriumExe)) {
        Write-Host "  [ERROR] Librium.exe not found: $LibriumExe" -ForegroundColor Red
        $overallOk = $false
    } else {
        $testScript = Join-Path $IntegrationDir "RunIntegrationTests.py"
        $dataDir    = Join-Path $IntegrationDir "data_py_$Preset"

        $pyArgs = @($testScript, "--librium", $LibriumExe, "--data-dir", $dataDir)
        if ($KeepData) { $pyArgs += "--keep-data" }

        $intProc = Start-Process -FilePath $py `
                                 -ArgumentList $pyArgs `
                                 -Wait -PassThru -NoNewWindow `
                                 -WorkingDirectory $RepoRoot

        if ($intProc.ExitCode -eq 0) {
            $script:IntPassed = $true
        } else {
            $overallOk = $false
        }
    }
}

# ------------------------------------------------------------------
# Summary
# ------------------------------------------------------------------
Write-Host ""
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  SUMMARY (Preset: $Preset)"                               -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

if (-not $IntegrationOnly) {
    if ($script:UnitPassed) { Write-Host "  Unit tests        : PASSED" -ForegroundColor Green }
    else { Write-Host "  Unit tests        : FAILED" -ForegroundColor Red }
}
if (-not $UnitOnly) {
    if ($script:IntPassed) { Write-Host "  Integration tests : PASSED" -ForegroundColor Green }
    else { Write-Host "  Integration tests : FAILED" -ForegroundColor Red }
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host ""

exit $(if ($overallOk) { 0 } else { 1 })
