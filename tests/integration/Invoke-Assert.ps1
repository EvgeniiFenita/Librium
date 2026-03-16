# Assertion helpers. Dot-source before use.

$script:PassCount = 0
$script:FailCount = 0

function Enter-Section
{
    param([string]$Name)
    Write-Host ""
    Write-Host "  -- $Name" -ForegroundColor Cyan
}

function Assert-True
{
    param([bool]$Condition, [string]$Description,
          [string]$Expected = "", [string]$Actual = "")
    if ($Condition)
    {
        $script:PassCount++
        Write-Host "    [PASS] $Description" -ForegroundColor Green
    }
    else
    {
        $script:FailCount++
        Write-Host "    [FAIL] $Description" -ForegroundColor Red
        if ($Expected -ne "")
        {
            Write-Host "           Expected : $Expected" -ForegroundColor Yellow
            Write-Host "           Actual   : $Actual"   -ForegroundColor Yellow
        }
    }
}

function Assert-Equal
{
    param($Expected, $Actual, [string]$Description)
    Assert-True -Condition ($Expected -eq $Actual) -Description $Description `
                -Expected "$Expected" -Actual "$Actual"
}

function Assert-JsonField
{
    param([object]$Json, [string]$Field, [string]$Description)
    $has = $null -ne $Json.PSObject.Properties[$Field]
    Assert-True -Condition $has -Description $Description `
                -Expected "field '$Field' present" `
                -Actual   $(if ($has) { "present" } else { "missing" })
}

function Assert-ExitCode
{
    param([int]$Code, [int]$Expected = 0, [string]$Description)
    Assert-True -Condition ($Code -eq $Expected) -Description $Description `
                -Expected "exit $Expected" -Actual "exit $Code"
}

function Get-TestSummary
{
    return @{ Pass = $script:PassCount; Fail = $script:FailCount;
              Total = $script:PassCount + $script:FailCount }
}
