# Generates synthetic INPX + FB2 archives for integration tests.
# Requires Python 3.

$script:InpSep = [char]0x04

function New-InpLine
{
    param([string]$Authors, [string]$Genres, [string]$Title,
          [string]$Series="", [string]$SerNo="", [string]$FileId,
          [string]$Size="102400", [string]$Deleted="0", [string]$Ext="fb2",
          [string]$Date="2020-01-01", [string]$Lang="ru",
          [string]$Rating="", [string]$Keywords="")
    return (@($Authors,$Genres,$Title,$Series,$SerNo,
              $FileId,$Size,$FileId,$Deleted,$Ext,$Date,$Lang,$Rating,$Keywords,"") `
            -join $script:InpSep)
}

function New-MinimalFb2
{
    param([string]$Title, [string]$AuthorLastName,
          [string]$Annotation="", [string]$Publisher="", [string]$Year="")
    $annXml = if ($Annotation) {
        "<annotation><p>$([System.Security.SecurityElement]::Escape($Annotation))</p></annotation>"
    } else { "" }
    $pubXml = ""
    if ($Publisher -or $Year)
    {
        $pubXml  = "<publish-info>"
        $pubXml += if ($Publisher) { "<publisher>$([System.Security.SecurityElement]::Escape($Publisher))</publisher>" } else { "" }
        $pubXml += if ($Year)      { "<year>$Year</year>" } else { "" }
        $pubXml += "</publish-info>"
    }
    return @"
<?xml version="1.0" encoding="utf-8"?>
<FictionBook xmlns="http://www.gribuser.ru/xml/fictionbook/2.0">
  <description>
    <title-info>
      <author><last-name>$AuthorLastName</last-name></author>
      <book-title>$([System.Security.SecurityElement]::Escape($Title))</book-title>
      $annXml
    </title-info>
    $pubXml
  </description>
  <body><section><p>.</p></section></body>
</FictionBook>
"@
}

function Invoke-PythonZip
{
    param([string]$ZipPath, [hashtable]$Entries)
    $tmpFiles = @{}
    foreach ($kv in $Entries.GetEnumerator())
    {
        $tmp = [System.IO.Path]::GetTempFileName()
        [System.IO.File]::WriteAllText($tmp, $kv.Value, [System.Text.Encoding]::UTF8)
        $tmpFiles[$kv.Key] = $tmp
    }
    $pyLines = @(
        "import zipfile, os",
        ("os.makedirs(r'" + (Split-Path $ZipPath) + "', exist_ok=True)"),
        ("z = zipfile.ZipFile(r'" + $ZipPath + "','w',zipfile.ZIP_DEFLATED)")
    )
    foreach ($kv in $tmpFiles.GetEnumerator())
        { $pyLines += ("z.write(r'" + $kv.Value + "','" + $kv.Key + "')") }
    $pyLines += "z.close()"
    ($pyLines -join "`n") | python3 -
    foreach ($tmp in $tmpFiles.Values)
        { if (Test-Path $tmp) { Remove-Item $tmp -Force -ErrorAction SilentlyContinue } }
}

function Invoke-PythonInpx
{
    param([string]$InpxPath, [hashtable]$InpEntries)
    $tmpFiles = @{}
    foreach ($kv in $InpEntries.GetEnumerator())
    {
        $tmp = [System.IO.Path]::GetTempFileName()
        [System.IO.File]::WriteAllText($tmp, $kv.Value, [System.Text.Encoding]::UTF8)
        $tmpFiles[$kv.Key] = $tmp
    }
    $pyLines = @(
        "import zipfile, os",
        ("os.makedirs(r'" + (Split-Path $InpxPath) + "', exist_ok=True)"),
        ("z = zipfile.ZipFile(r'" + $InpxPath + "','w',zipfile.ZIP_DEFLATED)")
    )
    foreach ($kv in $tmpFiles.GetEnumerator())
        { $pyLines += ("z.write(r'" + $kv.Value + "','" + $kv.Key + "')") }
    $pyLines += "z.writestr('collection.info','Integration Test Library\ntest\n65536\nTest\n')"
    $pyLines += "z.writestr('version.info','20240101\r\n')"
    $pyLines += "z.close()"
    ($pyLines -join "`n") | python3 -
    foreach ($tmp in $tmpFiles.Values)
        { if (Test-Path $tmp) { Remove-Item $tmp -Force -ErrorAction SilentlyContinue } }
}

function Get-V1Inp1Content
{
    $lines = @(
        (New-InpLine -Authors "Толстой,Лев,Николаевич:" -Genres "prose_classic:"
            -Title "Война и мир" -Series "Эпопея" -SerNo "1"
            -FileId "200001" -Size "2048000" -Date "2020-01-15" -Lang "ru" -Rating "5"),
        (New-InpLine -Authors "Достоевский,Фёдор,Михайлович:" -Genres "prose_classic:"
            -Title "Преступление и наказание"
            -FileId "200002" -Size "1024000" -Date "2020-02-10" -Lang "ru" -Rating "5"),
        (New-InpLine -Authors "Удалённый,Автор,:" -Genres "unknown:"
            -Title "Удалённая книга" -FileId "200003" -Size "512"
            -Deleted "1" -Date "2020-03-01" -Lang "ru"),
        (New-InpLine -Authors "King,Stephen,:" -Genres "thriller:"
            -Title "The Shining"
            -FileId "200004" -Size "768000" -Date "2020-04-01" -Lang "en" -Rating "4"),
        (New-InpLine -Authors "Стругацкий,Аркадий,Натанович:Стругацкий,Борис,Натанович:"
            -Genres "sf:" -Title "Пикник на обочине"
            -Series "Мир Полудня" -SerNo "3"
            -FileId "200005" -Size "512000" -Date "2021-06-01" -Lang "ru" -Rating "5")
    )
    return ($lines -join "`r`n") + "`r`n"
}

function Get-V1Inp2Content
{
    $lines = @(
        (New-InpLine -Authors "Christie,Agatha,:" -Genres "detective:"
            -Title "Murder on the Orient Express"
            -FileId "200006" -Size "480000" -Date "2021-01-20" -Lang "en" -Rating "4"),
        (New-InpLine -Authors "Булгаков,Михаил,Афанасьевич:" -Genres "prose_classic:"
            -Title "Мастер и Маргарита"
            -FileId "200007" -Size "900000" -Date "2021-03-05" -Lang "ru" -Rating "5"),
        (New-InpLine -Authors "Неизвестен,,:" -Genres "sf:"
            -Title "Неизвестная книга"
            -FileId "200008" -Size "256000" -Date "2022-07-14" -Lang "ru" -Rating "3")
    )
    return ($lines -join "`r`n") + "`r`n"
}

function New-LibraryV1
{
    param([string]$OutDir)
    $archDir = Join-Path $OutDir "v1\archives"
    $null = New-Item -ItemType Directory -Force -Path $archDir

    $a1 = @{
        "200001.fb2" = (New-MinimalFb2 -Title "Война и мир" -AuthorLastName "Толстой"
            -Annotation "Великий роман о войне и мире." -Publisher "Худ. лит." -Year "1978")
        "200002.fb2" = (New-MinimalFb2 -Title "Преступление и наказание" -AuthorLastName "Достоевский")
        "200004.fb2" = (New-MinimalFb2 -Title "The Shining" -AuthorLastName "King"
            -Annotation "A terrifying tale.")
        "200005.fb2" = (New-MinimalFb2 -Title "Пикник на обочине" -AuthorLastName "Стругацкий"
            -Annotation "Классика советской фантастики.")
    }
    Invoke-PythonZip -ZipPath (Join-Path $archDir "fb2-v1-001.zip") -Entries $a1

    $a2 = @{
        "200006.fb2" = (New-MinimalFb2 -Title "Murder on the Orient Express" -AuthorLastName "Christie")
        "200007.fb2" = (New-MinimalFb2 -Title "Мастер и Маргарита" -AuthorLastName "Булгаков"
            -Annotation "Роман о дьяволе, посетившем Москву.")
        "200008.fb2" = (New-MinimalFb2 -Title "Неизвестная книга" -AuthorLastName "Неизвестен")
    }
    Invoke-PythonZip -ZipPath (Join-Path $archDir "fb2-v1-002.zip") -Entries $a2

    Invoke-PythonInpx -InpxPath (Join-Path $OutDir "v1\library.inpx") -InpEntries @{
        "fb2-v1-001.zip.inp" = (Get-V1Inp1Content)
        "fb2-v1-002.zip.inp" = (Get-V1Inp2Content)
    }
    Write-Host "  Library V1 ready: 7 valid books (1 deleted record)" -ForegroundColor DarkGray
}

function New-LibraryV2
{
    param([string]$OutDir)
    $archV1 = Join-Path $OutDir "v1\archives"
    $archV2 = Join-Path $OutDir "v2\archives"
    $null = New-Item -ItemType Directory -Force -Path $archV2

    Copy-Item (Join-Path $archV1 "fb2-v1-001.zip") $archV2
    Copy-Item (Join-Path $archV1 "fb2-v1-002.zip") $archV2

    $a3 = @{
        "200009.fb2" = (New-MinimalFb2 -Title "Вишнёвый сад" -AuthorLastName "Чехов"
            -Annotation "Пьеса о гибели дворянского уклада.")
        "200010.fb2" = (New-MinimalFb2 -Title "Foundation" -AuthorLastName "Asimov")
        "200011.fb2" = (New-MinimalFb2 -Title "Анна Каренина" -AuthorLastName "Толстой")
    }
    Invoke-PythonZip -ZipPath (Join-Path $archV2 "fb2-v2-001.zip") -Entries $a3

    $v2inp3Lines = @(
        (New-InpLine -Authors "Чехов,Антон,Павлович:" -Genres "prose_classic:"
            -Title "Вишнёвый сад"
            -FileId "200009" -Size "310000" -Date "2023-05-01" -Lang "ru" -Rating "4"),
        (New-InpLine -Authors "Asimov,Isaac,:" -Genres "sf:"
            -Title "Foundation" -Series "Foundation" -SerNo "1"
            -FileId "200010" -Size "620000" -Date "2023-06-15" -Lang "en" -Rating "5"),
        (New-InpLine -Authors "Толстой,Лев,Николаевич:" -Genres "prose_classic:"
            -Title "Анна Каренина"
            -FileId "200011" -Size "1800000" -Date "2023-08-20" -Lang "ru" -Rating "5")
    )
    $v2inp3 = ($v2inp3Lines -join "`r`n") + "`r`n"

    Invoke-PythonInpx -InpxPath (Join-Path $OutDir "v2\library.inpx") -InpEntries @{
        "fb2-v1-001.zip.inp" = (Get-V1Inp1Content)
        "fb2-v1-002.zip.inp" = (Get-V1Inp2Content)
        "fb2-v2-001.zip.inp" = $v2inp3
    }
    Write-Host "  Library V2 ready: +3 books in fb2-v2-001.zip" -ForegroundColor DarkGray
}
