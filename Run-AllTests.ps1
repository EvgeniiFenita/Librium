#Requires -Version 5.1
<#
.SYNOPSIS
    Zapusk vsekh testov proekta: Catch2 unit-testy i integratsionnye E2E testy.

.PARAMETER Preset
    CMake-preset (papka vnutri out\build\). Default: x64-debug

.PARAMETER KeepData
    Ne udalyat' testovye dannye posle integratsionnykh testov.

.PARAMETER UnitOnly
    Tol'ko Catch2-testy.

.PARAMETER IntegrationOnly
    Tol'ko integratsionnye testy.

.EXAMPLE
    .\Run-AllTests.ps1
    .\Run-AllTests.ps1 -Preset x64-release
    .\Run-AllTests.ps1 -UnitOnly
    .\Run-AllTests.ps1 -KeepData
#>
param(
    [string] $Preset          = "x64-debug",
    [switch] $KeepData,
    [switch] $UnitOnly,
    [switch] $IntegrationOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot       = $PSScriptRoot
$Config         = if ($Preset -like "*debug*") { "Debug" } else { "Release" }
$BuildDir       = Join-Path $RepoRoot "out\build\$Preset"
$UnitTestExe    = Join-Path $BuildDir "tests\$Config\libindexer_tests.exe"
$IndexerExe     = Join-Path $BuildDir "apps\indexer\$Config\libindexer.exe"
$QueryExe       = Join-Path $BuildDir "apps\query\$Config\libquery.exe"
$IntegrationDir = Join-Path $RepoRoot "tests\integration"

# ------------------------------------------------------------------
# Banner
# ------------------------------------------------------------------
Write-Host ""
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  libindexer -- Test Runner"                               -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  Preset    : $Preset"                                     -ForegroundColor DarkGray
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
        Write-Host "[ERROR] Ne najden ispolnyaemyj fajl testov:" -ForegroundColor Red
        Write-Host "        $UnitTestExe"                        -ForegroundColor Red
        Write-Host "        Sberite proekt: cmake --build out\build\$Preset" -ForegroundColor Yellow
        $ok = $false
    }
}

if (-not $UnitOnly)
{
    if (-not (Test-Path $IndexerExe))
    {
        Write-Host "[ERROR] Ne najden libindexer.exe:" -ForegroundColor Red
        Write-Host "        $IndexerExe"               -ForegroundColor Red
        $ok = $false
    }
    if (-not (Test-Path $QueryExe))
    {
        Write-Host "[ERROR] Ne najden libquery.exe:" -ForegroundColor Red
        Write-Host "        $QueryExe"               -ForegroundColor Red
        $ok = $false
    }
    $py = Get-Command python3 -ErrorAction SilentlyContinue
    if (-not $py)
    {
        $py = Get-Command python -ErrorAction SilentlyContinue
        if ($py)
        {
            Set-Alias python3 python -Scope Script
            Write-Host "  [INFO] python3 ne najden, ispol'zuem python ($($py.Source))" -ForegroundColor DarkYellow
        }
        else
        {
            Write-Host "[ERROR] python3 (ili python) ne najden na PATH." -ForegroundColor Red
            $ok = $false
        }
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
    Write-Host "  UNIT TESTS  (Catch2 - libindexer_tests.exe)"            -ForegroundColor White
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
    Write-Host "  INTEGRATION TESTS  (Python - run_integration_tests.py)" -ForegroundColor White
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
        return
    }

    $testScript = Join-Path $IntegrationDir "run_integration_tests.py"
    $dataDir    = Join-Path $IntegrationDir "data_py"

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

# ------------------------------------------------------------------
# Summary
# ------------------------------------------------------------------
Write-Host ""
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  SUMMARY"                                                  -ForegroundColor Cyan
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
