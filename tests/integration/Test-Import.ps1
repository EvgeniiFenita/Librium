# Import and upgrade test cases.

function Build-Config
{
    param([string]$InpxPath, [string]$ArchivesDir, [string]$Mode)
    return @{
        database = @{ path = $script:DbPath }
        library  = @{
            inpxPath    = $InpxPath    -replace '\\', '/'
            archivesDir = $ArchivesDir -replace '\\', '/'
        }
        import  = @{ parseFb2=$true; threadCount=2; transactionBatchSize=500; mode=$Mode }
        filters = @{
            excludeLanguages=@(); includeLanguages=@()
            excludeGenres=@();    includeGenres=@()
            minFileSize=0; maxFileSize=0
            excludeAuthors=@(); excludeKeywords=@()
        }
        logging = @{ level="info"; file=""; progressInterval=100 }
    }
}

function Invoke-LibQuery
{
    param([string]$OutFile, [string[]]$Args = @())
    $fullArgs = @("--db", $script:DbPath, "--output", $OutFile) + $Args
    $proc = Start-Process -FilePath $script:Query -ArgumentList $fullArgs `
                          -Wait -PassThru -NoNewWindow `
                          -RedirectStandardOutput "$($script:DataDir)\q_tmp.log"
    return $proc.ExitCode
}

function Test-FullImport
{
    Enter-Section "Full Import (Library V1 — 7 valid books, 1 deleted)"

    $cfg = Build-Config `
        -InpxPath    (Join-Path $script:DataDir "v1\library.inpx") `
        -ArchivesDir (Join-Path $script:DataDir "v1\archives") `
        -Mode        "full"
    $cfgPath = Join-Path $script:DataDir "config_v1.json"
    $cfg | ConvertTo-Json -Depth 5 | Set-Content $cfgPath -Encoding UTF8

    $proc = Start-Process -FilePath $script:Indexer `
                          -ArgumentList "import","--config",$cfgPath `
                          -Wait -PassThru -NoNewWindow `
                          -RedirectStandardOutput "$($script:DataDir)\import_v1.log" `
                          -RedirectStandardError  "$($script:DataDir)\import_v1.err"

    Assert-ExitCode -Code $proc.ExitCode -Expected 0 `
                    -Description "libindexer import exits with code 0"

    $out = Join-Path $script:DataDir "out\all_v1.json"
    Invoke-LibQuery -OutFile $out -Args @("--limit","0") | Out-Null
    $json = Get-Content $out -Raw | ConvertFrom-Json

    Assert-Equal -Expected 7 -Actual $json.totalFound `
                 -Description "DB has 7 valid books (deleted book skipped)"

    $del = $json.books | Where-Object { $_.libId -eq "200003" }
    Assert-True -Condition ($null -eq $del) `
                -Description "Deleted book (libId=200003) absent from DB"
}

function Test-StatsOutput
{
    Enter-Section "Stats Command"

    $log = Join-Path $script:DataDir "stats.log"
    $proc = Start-Process -FilePath $script:Indexer `
                          -ArgumentList "stats","--db",$script:DbPath `
                          -Wait -PassThru -NoNewWindow `
                          -RedirectStandardOutput $log

    Assert-ExitCode -Code $proc.ExitCode -Expected 0 `
                    -Description "libindexer stats exits with code 0"
    Assert-True -Condition ((Get-Content $log -Raw) -match "7") `
                -Description "Stats output contains count 7"
}

function Test-UpgradeImport
{
    Enter-Section "Upgrade Import (Library V2 — +3 new books)"

    $cfg = Build-Config `
        -InpxPath    (Join-Path $script:DataDir "v2\library.inpx") `
        -ArchivesDir (Join-Path $script:DataDir "v2\archives") `
        -Mode        "upgrade"
    $cfgPath = Join-Path $script:DataDir "config_v2.json"
    $cfg | ConvertTo-Json -Depth 5 | Set-Content $cfgPath -Encoding UTF8

    $proc = Start-Process -FilePath $script:Indexer `
                          -ArgumentList "import","--config",$cfgPath `
                          -Wait -PassThru -NoNewWindow `
                          -RedirectStandardOutput "$($script:DataDir)\import_v2.log" `
                          -RedirectStandardError  "$($script:DataDir)\import_v2.err"

    Assert-ExitCode -Code $proc.ExitCode -Expected 0 `
                    -Description "libindexer upgrade exits with code 0"

    $out = Join-Path $script:DataDir "out\all_v2.json"
    Invoke-LibQuery -OutFile $out -Args @("--limit","0") | Out-Null
    $json = Get-Content $out -Raw | ConvertFrom-Json

    Assert-Equal -Expected 10 -Actual $json.totalFound `
                 -Description "DB has 10 books after upgrade (7 + 3)"
}

function Test-UpgradeIdempotency
{
    Enter-Section "Upgrade Idempotency (second upgrade adds 0 books)"

    $cfgPath = Join-Path $script:DataDir "config_v2.json"
    $proc = Start-Process -FilePath $script:Indexer `
                          -ArgumentList "import","--config",$cfgPath `
                          -Wait -PassThru -NoNewWindow `
                          -RedirectStandardOutput "$($script:DataDir)\import_v2b.log" `
                          -RedirectStandardError  "$($script:DataDir)\import_v2b.err"

    Assert-ExitCode -Code $proc.ExitCode -Expected 0 `
                    -Description "Second upgrade exits with code 0"

    $out = Join-Path $script:DataDir "out\all_v2b.json"
    Invoke-LibQuery -OutFile $out -Args @("--limit","0") | Out-Null
    $json = Get-Content $out -Raw | ConvertFrom-Json

    Assert-Equal -Expected 10 -Actual $json.totalFound `
                 -Description "Book count unchanged (idempotent)"
}
