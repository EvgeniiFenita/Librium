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

.EXAMPLE
    .\RunAllTests.ps1
    .\RunAllTests.ps1 -Preset x64-release
    .\RunAllTests.ps1 -UnitOnly
    .\RunAllTests.ps1 -KeepData
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

# Executables follow the configuration subfolder pattern for multi-config generators (MSVC)
$UnitTestExe    = Join-Path $BuildDir "tests\Unit\$Config\UnitTests.exe"
$IndexerExe     = Join-Path $BuildDir "apps\Indexer\$Config\Indexer.exe"
$QueryExe       = Join-Path $BuildDir "apps\Query\$Config\Query.exe"
$IntegrationDir = Join-Path $RepoRoot "tests\Integration"

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

# ------------------------------------------------------------------
# Pre-flight checks
# ------------------------------------------------------------------
$ok = $true

if (-not $IntegrationOnly)
{
    if (-not (Test-Path $UnitTestExe))
    {
        Write-Host "[ERROR] Unit test executable not found:"           -ForegroundColor Red
        Write-Host "        $UnitTestExe"                              -ForegroundColor Red
        Write-Host "        Build the project first: .\Build.ps1 -Preset $Preset" -ForegroundColor Yellow
        $ok = $false
    }
}

if (-not $UnitOnly)
{
    if (-not (Test-Path $IndexerExe))
    {
        Write-Host "[ERROR] Indexer.exe not found:"                    -ForegroundColor Red
        Write-Host "        $IndexerExe"                               -ForegroundColor Red
        $ok = $false
    }
    if (-not (Test-Path $QueryExe))
    {
        Write-Host "[ERROR] Query.exe not found:"                      -ForegroundColor Red
        Write-Host "        $QueryExe"                                 -ForegroundColor Red
        $ok = $false
    }
}

if (-not $ok) { Write-Host ""; exit 1 }

# ------------------------------------------------------------------
# Result tracking
# ------------------------------------------------------------------
$script:UnitPassed = $false
$script:IntPassed  = $false
$overallOk         = $true

# ------------------------------------------------------------------
# 1. Catch2 unit tests
# ------------------------------------------------------------------
if (-not $IntegrationOnly)
{
    Write-Host "----------------------------------------------------------" -ForegroundColor Cyan
    Write-Host "  UNIT TESTS  (Catch2 - UnitTests.exe)"            -ForegroundColor White
    Write-Host "----------------------------------------------------------" -ForegroundColor Cyan
    Write-Host ""

    $unitProc = Start-Process -FilePath $UnitTestExe `
                              -ArgumentList "--colour-mode","ansi"`
                              -Wait -PassThru -NoNewWindow

    Write-Host ""
    if ($unitProc.ExitCode -eq 0)
    {
        Write-Host "  [OK] Unit tests PASSED" -ForegroundColor Green
        $script:UnitPassed = $true
    }
    else
    {
        Write-Host "  [FAIL] Unit tests FAILED  (exit code $($unitProc.ExitCode))" -ForegroundColor Red
        $overallOk = $false
    }
}

# ------------------------------------------------------------------
# 2. Python integration tests
# ------------------------------------------------------------------
if (-not $UnitOnly)
{
    Write-Host ""
    Write-Host "----------------------------------------------------------" -ForegroundColor Cyan
    Write-Host "  INTEGRATION TESTS  (Python - RunIntegrationTests.py)"   -ForegroundColor White
    Write-Host "----------------------------------------------------------" -ForegroundColor Cyan

    $py = ""
    foreach ($t in @("python", "python3")) {
        $cmd = Get-Command $t -ErrorAction SilentlyContinue
        if ($cmd -and (Get-Item $cmd.Source).Length -gt 0) {
            $py = $cmd.Source
            break
        }
    }
    if (-not $py) { 
        Write-Host "  [ERROR] Python not found (or only Store aliases found)" -ForegroundColor Red
        $overallOk = $false
    }
    else {
        $testScript = Join-Path $IntegrationDir "RunIntegrationTests.py"
        $dataDir    = Join-Path $IntegrationDir "data_py_$Preset"

        $pyArgs = @($testScript, "--indexer", $IndexerExe, "--query", $QueryExe, "--data-dir", $dataDir)
        if ($KeepData) { $pyArgs += "--keep-data" }

        $intProc = Start-Process -FilePath $py `
                                 -ArgumentList $pyArgs `
                                 -Wait -PassThru -NoNewWindow `
                                 -WorkingDirectory $RepoRoot

        if ($intProc.ExitCode -eq 0)
        {
            $script:IntPassed = $true
        }
        else
        {
            Write-Host "  [FAIL] Integration tests FAILED (exit code $($intProc.ExitCode))" -ForegroundColor Red
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

if (-not $IntegrationOnly)
{
    if ($script:UnitPassed)
        { Write-Host "  Unit tests        : PASSED" -ForegroundColor Green }
    else
        { Write-Host "  Unit tests        : FAILED" -ForegroundColor Red   }
}
if (-not $UnitOnly)
{
    if ($script:IntPassed)
        { Write-Host "  Integration tests : PASSED" -ForegroundColor Green }
    else
        { Write-Host "  Integration tests : FAILED" -ForegroundColor Red   }
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host ""

exit $(if ($overallOk) { 0 } else { 1 })
