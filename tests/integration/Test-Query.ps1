# libquery test cases. Expects 10 books in DB.

function Invoke-LibQuery
{
    param([string]$OutFile, [string[]]$Args = @())
    $fullArgs = @("--db",$script:DbPath,"--output",$OutFile) + $Args
    $proc = Start-Process -FilePath $script:Query -ArgumentList $fullArgs `
                          -Wait -PassThru -NoNewWindow `
                          -RedirectStandardOutput "$($script:DataDir)\q_tmp.log"
    return $proc.ExitCode
}

function Test-QueryAll
{
    Enter-Section "Query: All Books (limit=0)"
    $out = Join-Path $script:DataDir "out\q_all.json"
    $exit = Invoke-LibQuery -OutFile $out -Args @("--limit","0")
    Assert-ExitCode -Code $exit -Expected 0 -Description "libquery exits 0"
    $json = Get-Content $out -Raw | ConvertFrom-Json
    Assert-Equal -Expected 10 -Actual $json.totalFound -Description "totalFound == 10"
    Assert-Equal -Expected 10 -Actual $json.books.Count -Description "10 books returned"
}

function Test-QueryByLanguage
{
    Enter-Section "Query: Language Filter"
    # ru: 200001,200002,200005,200007,200008,200009,200011 = 7
    $outRu = Join-Path $script:DataDir "out\q_ru.json"
    Invoke-LibQuery -OutFile $outRu -Args @("--language","ru","--limit","0") | Out-Null
    $ru = Get-Content $outRu -Raw | ConvertFrom-Json
    Assert-Equal -Expected 7 -Actual $ru.totalFound -Description "Language=ru: 7 books"
    $wrong = $ru.books | Where-Object { $_.language -ne "ru" }
    Assert-True -Condition ($null -eq $wrong) -Description "All ru results have language='ru'"

    # en: 200004,200006,200010 = 3
    $outEn = Join-Path $script:DataDir "out\q_en.json"
    Invoke-LibQuery -OutFile $outEn -Args @("--language","en","--limit","0") | Out-Null
    $en = Get-Content $outEn -Raw | ConvertFrom-Json
    Assert-Equal -Expected 3 -Actual $en.totalFound -Description "Language=en: 3 books"
}

function Test-QueryByAuthor
{
    Enter-Section "Query: Author Filter"
    # Толстой: 200001 + 200011 = 2
    $out = Join-Path $script:DataDir "out\q_tolstoy.json"
    Invoke-LibQuery -OutFile $out -Args @("--author","Толстой","--limit","0") | Out-Null
    $json = Get-Content $out -Raw | ConvertFrom-Json
    Assert-Equal -Expected 2 -Actual $json.totalFound -Description "Author=Толстой: 2 books"
    $ids = $json.books | Select-Object -ExpandProperty libId
    Assert-True -Condition ($ids -contains "200001") -Description "200001 in results"
    Assert-True -Condition ($ids -contains "200011") -Description "200011 in results"
}

function Test-QueryByGenre
{
    Enter-Section "Query: Genre Filter"
    # prose_classic: 200001,200002,200007,200009,200011 = 5
    $outC = Join-Path $script:DataDir "out\q_classic.json"
    Invoke-LibQuery -OutFile $outC -Args @("--genre","prose_classic","--limit","0") | Out-Null
    Assert-Equal -Expected 5 `
                 -Actual ((Get-Content $outC -Raw | ConvertFrom-Json).totalFound) `
                 -Description "Genre=prose_classic: 5 books"

    # thriller: 200004 only
    $outT = Join-Path $script:DataDir "out\q_thriller.json"
    Invoke-LibQuery -OutFile $outT -Args @("--genre","thriller","--limit","0") | Out-Null
    $jt = Get-Content $outT -Raw | ConvertFrom-Json
    Assert-Equal -Expected 1        -Actual $jt.totalFound       -Description "Genre=thriller: 1 book"
    Assert-Equal -Expected "200004" -Actual $jt.books[0].libId   -Description "Correct book (200004)"
}

function Test-QueryBySeries
{
    Enter-Section "Query: Series Filter"
    $out = Join-Path $script:DataDir "out\q_series.json"
    Invoke-LibQuery -OutFile $out -Args @("--series","Эпопея","--limit","0") | Out-Null
    $json = Get-Content $out -Raw | ConvertFrom-Json
    Assert-Equal -Expected 1        -Actual $json.totalFound     -Description "Series=Эпопея: 1 book"
    Assert-Equal -Expected "200001" -Actual $json.books[0].libId -Description "Correct book"
}

function Test-QueryWithAnnotation
{
    Enter-Section "Query: --with-annotation Filter"
    # With annotation: 200001,200004,200005,200007,200009 = 5
    $out = Join-Path $script:DataDir "out\q_ann.json"
    Invoke-LibQuery -OutFile $out -Args @("--with-annotation","--limit","0") | Out-Null
    $json = Get-Content $out -Raw | ConvertFrom-Json
    Assert-Equal -Expected 5 -Actual $json.totalFound -Description "--with-annotation: 5 books"
    $empty = $json.books | Where-Object { $_.annotation -eq "" }
    Assert-True -Condition ($null -eq $empty) -Description "All have non-empty annotation"
}

function Test-QueryByRating
{
    Enter-Section "Query: Rating Filter"
    # rating>=5: 200001,200002,200005,200007,200010,200011 = 6
    $out = Join-Path $script:DataDir "out\q_rating5.json"
    Invoke-LibQuery -OutFile $out -Args @("--rating-min","5","--limit","0") | Out-Null
    $json = Get-Content $out -Raw | ConvertFrom-Json
    Assert-Equal -Expected 6 -Actual $json.totalFound -Description "Rating>=5: 6 books"
    $below = $json.books | Where-Object { $_.rating -lt 5 }
    Assert-True -Condition ($null -eq $below) -Description "All have rating >= 5"
}

function Test-QueryPagination
{
    Enter-Section "Query: Pagination"
    $out1 = Join-Path $script:DataDir "out\q_p1.json"
    $out2 = Join-Path $script:DataDir "out\q_p2.json"
    Invoke-LibQuery -OutFile $out1 -Args @("--limit","3","--offset","0") | Out-Null
    Invoke-LibQuery -OutFile $out2 -Args @("--limit","3","--offset","3") | Out-Null
    $j1 = Get-Content $out1 -Raw | ConvertFrom-Json
    $j2 = Get-Content $out2 -Raw | ConvertFrom-Json
    Assert-Equal -Expected 3  -Actual $j1.books.Count -Description "Page1: 3 books"
    Assert-Equal -Expected 10 -Actual $j1.totalFound  -Description "Page1: totalFound=10"
    Assert-Equal -Expected 3  -Actual $j2.books.Count -Description "Page2: 3 books"
    $ids1 = $j1.books | Select-Object -ExpandProperty libId
    $ids2 = $j2.books | Select-Object -ExpandProperty libId
    $overlap = $ids1 | Where-Object { $ids2 -contains $_ }
    Assert-True -Condition ($null -eq $overlap) -Description "Pages don't overlap"
}

function Test-QueryDateRange
{
    Enter-Section "Query: Date Range"
    # Added >= 2023-01-01: 200009,200010,200011 = 3
    $out = Join-Path $script:DataDir "out\q_date.json"
    Invoke-LibQuery -OutFile $out -Args @("--date-from","2023-01-01","--limit","0") | Out-Null
    $json = Get-Content $out -Raw | ConvertFrom-Json
    Assert-Equal -Expected 3 -Actual $json.totalFound -Description "date-from 2023: 3 books"
    $old = $json.books | Where-Object { $_.dateAdded -lt "2023-01-01" }
    Assert-True -Condition ($null -eq $old) -Description "All dateAdded >= 2023-01-01"
}

function Test-QuerySingleBook
{
    Enter-Section "Query: Single Book by lib-id"
    $out = Join-Path $script:DataDir "out\q_single.json"
    Invoke-LibQuery -OutFile $out -Args @("--lib-id","200007") | Out-Null
    $json = Get-Content $out -Raw | ConvertFrom-Json
    Assert-Equal -Expected 1           -Actual $json.totalFound      -Description "1 book found"
    Assert-Equal -Expected "200007"    -Actual $json.books[0].libId  -Description "Correct libId"
    Assert-Equal -Expected "Мастер и Маргарита" -Actual $json.books[0].title -Description "Correct title"
}

function Test-QueryJsonStructure
{
    Enter-Section "Query: JSON Structure"
    $out = Join-Path $script:DataDir "out\q_struct.json"
    Invoke-LibQuery -OutFile $out -Args @("--limit","3") | Out-Null
    $json = Get-Content $out -Raw | ConvertFrom-Json
    Assert-JsonField -Json $json -Field "totalFound" -Description "Top: totalFound"
    Assert-JsonField -Json $json -Field "books"      -Description "Top: books"
    Assert-JsonField -Json $json -Field "query"      -Description "Top: query"
    foreach ($book in $json.books)
    {
        foreach ($f in @("libId","title","authors","genres","language",
                         "dateAdded","rating","fileSize","archiveName",
                         "annotation","publisher","isbn","publishYear"))
        {
            Assert-JsonField -Json $book -Field $f -Description "Book.$f present"
        }
        $hasCover = $null -ne $book.PSObject.Properties["cover"]
        Assert-True -Condition (-not $hasCover) -Description "Book has NO 'cover' field"
    }
}
