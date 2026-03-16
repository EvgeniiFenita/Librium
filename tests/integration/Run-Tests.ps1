#Requires -Version 5.1
<#
.SYNOPSIS
    End-to-end integration tests for libindexer and libquery.

.PARAMETER Build
    CMake build preset directory name. Default: windows-debug

.PARAMETER Config
    Build configuration. Default: Debug

.PARAMETER KeepData
    Do not delete generated test data after run.

.EXAMPLE
    .\tests\integration\Run-Tests.ps1
    .\tests\integration\Run-Tests.ps1 -Build windows-release -Config Release
#>
param(
    [string] $Build    = "windows-debug",
    [string] $Config   = "Debug",
    [switch] $KeepData
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$BuildDir = Join-Path $RepoRoot "build\$Build"

$script:Indexer = Join-Path $BuildDir "apps\indexer\$Config\libindexer.exe"
$script:Query   = Join-Path $BuildDir "apps\query\$Config\libquery.exe"
$script:DataDir = Join-Path $PSScriptRoot "data"
$script:DbPath  = Join-Path $script:DataDir "library.db"

Write-Host ""
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "  libindexer - End-to-End Integration Test Suite"           -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host ""

$ok = $true

if (-not (Test-Path $script:Indexer))
{
    Write-Host "[ERROR] libindexer.exe not found:" -ForegroundColor Red
    Write-Host "        $($script:Indexer)"        -ForegroundColor Red
    Write-Host ""
    Write-Host "        Build the project first:" -ForegroundColor Yellow
    Write-Host "          cmake --preset $Build"  -ForegroundColor Yellow
    Write-Host "          cmake --build build\$Build --config $Config" -ForegroundColor Yellow
    $ok = $false
}

if (-not (Test-Path $script:Query))
{
    Write-Host "[ERROR] libquery.exe not found:" -ForegroundColor Red
    Write-Host "        $($script:Query)"        -ForegroundColor Red
    $ok = $false
}

if (-not (Get-Command python3 -ErrorAction SilentlyContinue))
{
    Write-Host "[ERROR] python3 not found on PATH." -ForegroundColor Red
    Write-Host "        Python 3 is required to generate test ZIP/INPX files." -ForegroundColor Yellow
    $ok = $false
}

if (-not $ok) { Write-Host ""; exit 1 }

Write-Host "  libindexer : $($script:Indexer)" -ForegroundColor DarkGray
Write-Host "  libquery   : $($script:Query)"   -ForegroundColor DarkGray
Write-Host "  data dir   : $($script:DataDir)" -ForegroundColor DarkGray
Write-Host ""

if (Test-Path $script:DataDir)
    { Remove-Item $script:DataDir -Recurse -Force }
$null = New-Item -ItemType Directory -Force -Path (Join-Path $script:DataDir "out")

. (Join-Path $PSScriptRoot "Invoke-Assert.ps1")
. (Join-Path $PSScriptRoot "New-TestLibrary.ps1")
. (Join-Path $PSScriptRoot "Test-Import.ps1")
. (Join-Path $PSScriptRoot "Test-Query.ps1")

Write-Host "Generating synthetic library..." -ForegroundColor DarkGray
try
{
    New-LibraryV1 -OutDir $script:DataDir
    New-LibraryV2 -OutDir $script:DataDir
}
catch
{
    Write-Host ""
    Write-Host "[ERROR] Library generation failed: $_" -ForegroundColor Red
    exit 1
}
Write-Host ""

Write-Host "Import / Upgrade tests" -ForegroundColor White
Test-FullImport
Test-StatsOutput
Test-UpgradeImport
Test-UpgradeIdempotency

Write-Host ""
Write-Host "Query tests" -ForegroundColor White
Test-QueryAll
Test-QueryByLanguage
Test-QueryByAuthor
Test-QueryByGenre
Test-QueryBySeries
Test-QueryWithAnnotation
Test-QueryByRating
Test-QueryPagination
Test-QueryDateRange
Test-QuerySingleBook
Test-QueryJsonStructure

if (-not $KeepData)
    { Remove-Item $script:DataDir -Recurse -Force }

$s = Get-TestSummary
Write-Host ""
Write-Host "==========================================================" -ForegroundColor Cyan
if ($s.Fail -eq 0)
{
    Write-Host ("  ALL TESTS PASSED  ({0} / {1})" -f $s.Pass, $s.Total) -ForegroundColor Green
}
else
{
    Write-Host ("  {0} TEST(S) FAILED  ({1} passed, {0} failed)" -f $s.Fail, $s.Pass) `
               -ForegroundColor Red
}
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host ""

exit $(if ($s.Fail -eq 0) { 0 } else { 1 })
