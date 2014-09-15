# This script may be used for creating luafar_manual.html
# You must run tool.make_lua_chm.bat before calling this

$Script_Working_path = (Get-Location).Path

if ($htm_dir -eq "") {
  $htm_dir = "T:\VCProject\Maximus5\FarManager\Far30.4000\enc\build\lua\luafar_manual"
}
if ($toc_dir -eq "") {
  $toc_dir = Split-Path $htm_dir -Parent
}

function Get-Title([string]$file_path)
{
  $data = [System.IO.File]::ReadAllText($file_path, [System.Text.Encoding]::Default)
  $title = [regex] '(?<=<title>)([\S\s]*?)(?=</title>)'

  $prefix = "http://api.farmanager.com/ru/"
  $new_pf = "../../enc_rus3.work/meta/"
  # http://api.farmanager.com/ru/
  # \enc\build_lua\macroapi_manual.ru\
  # \enc\enc_rus3.work\meta
  if ($data.IndexOf("http://api.farmanager.com/ru/") -gt 0) {
    $data = $data.Replace($prefix, $new_pf)
    [System.IO.File]::WriteAllText($file_path, $data, [System.Text.Encoding]::Default)
  }

  return $title.Match($data).value.trim().Replace("&quot;","")
}

function Load-Files([string]$dir)
{
  $items = @()
  Get-ChildItem (Join-Path $dir "*.html") -File | foreach {
    $i = @{File=$_.Name; Title=(Get-Title $_.FullName)}
    $items += $i
  }
  return $items
}

function Create-TOC([string]$htm_dir="",[string]$toc_dir="")
{
  $items = Load-Files $htm_dir

  $name = (Split-Path $htm_dir -Leaf)
  $title = "Table of contents: $name"

  $toc = @('<html>',
    '<head>',
    '<meta http-equiv="Content-Type" content="text/html; charset=windows-1251">',
    '<title>',
    $title,
    '</title>',
    '</head>',
    '<body>',
    '<H2>' + $title + '</H2>')

  $items | sort {$_.Title} | foreach {
    $toc += @("<div><a href='$name/" + $_.File + "'>" + $_.Title + "</a></div>")
  }

  $toc += @('</body>', '</html>')

  $file = (Join-Path $toc_dir ($name + ".html"))
  Set-Content $file $toc
}


$Script_Working_path = (Get-Location).Path

$build_lua_dir = (Join-Path $Script_Working_path "..\build\lua")
$root_dir = (Join-Path $Script_Working_path "..\enc_lua_build")
if ([System.IO.Directory]::Exists($root_dir)) {
  Write-Host "Erasing old $root_dir"
  rd -recurse $root_dir
}

Write-Host -ForegroundColor Green "Creating: $build_lua_dir"
& cmd /c (Join-Path $Script_Working_path tool.make_lua_chm.bat)
copy -Recurse -Force $build_lua_dir $root_dir

$htm_dir = (Join-Path $root_dir "luafar_manual")
$toc_dir = Split-Path $htm_dir -Parent
Create-TOC $htm_dir $toc_dir

$htm_dir = (Join-Path $root_dir "macroapi_manual.ru")
$toc_dir = Split-Path $htm_dir -Parent
Create-TOC $htm_dir $toc_dir
