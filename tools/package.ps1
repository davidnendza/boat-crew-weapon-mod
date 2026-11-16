# Builds the release folder and zip: release\BoatCrewWeaponMod-<version>\ and its .zip, from out\version.dll (run
# build.bat first), README.md, LICENSE, models\ and sounds\ (no source since 0.9.6). The version is kModVersion in weaponmod.cpp;
# a "-dev" version is refused unless -Dev is given. Run from anywhere: powershell -File tools\package.ps1
param([switch]$Dev)
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$cpp = Join-Path $root "src\weaponmod.cpp"
$m = Select-String -Path $cpp -Pattern 'kModVersion = "([^"]+)"' | Select-Object -First 1
if (-not $m) { throw "kModVersion not found in $cpp" }
$ver = $m.Matches[0].Groups[1].Value
if ($ver -like "*-dev*" -and -not $Dev) { throw "kModVersion is '$ver'; set the release version first (or pass -Dev)" }

$dll = Join-Path $root "out\version.dll"
if (-not (Test-Path $dll)) { throw "out\version.dll is missing; run build.bat" }
$newest = Get-ChildItem (Join-Path $root "src") -File | Where-Object { ($_.Extension -eq ".cpp" -or $_.Extension -eq ".h") -and $_.Name -notlike "*before*" } | Sort-Object LastWriteTime | Select-Object -Last 1
if ((Get-Item $dll).LastWriteTime -lt $newest.LastWriteTime) { throw "out\version.dll is older than $($newest.Name); run build.bat" }

# LICENSE is not kept in the repo root; it comes from the newest earlier release folder
$license = Join-Path $root "LICENSE"
if (-not (Test-Path $license)) {
  $license = Get-ChildItem (Join-Path $root "release") -Directory | Where-Object { Test-Path (Join-Path $_.FullName "LICENSE") } |
             Sort-Object LastWriteTime -Descending | Select-Object -First 1 | ForEach-Object { Join-Path $_.FullName "LICENSE" }
  if (-not $license) { throw "no LICENSE found" }
}

$name = "BoatCrewWeaponMod-$ver"
$out = Join-Path $root "release\$name"
$zip = "$out.zip"
if (Test-Path $out) { Remove-Item $out -Recurse -Force }
if (Test-Path $zip) { Remove-Item $zip -Force }
New-Item -ItemType Directory -Force (Join-Path $out "BoatCrewMod\models"), (Join-Path $out "BoatCrewMod\sounds") | Out-Null

Copy-Item $dll $out
Copy-Item (Join-Path $root "README.md") $out
Copy-Item $license (Join-Path $out "LICENSE")
Copy-Item (Join-Path $root "models\*") (Join-Path $out "BoatCrewMod\models")
Copy-Item (Join-Path $root "sounds\*") (Join-Path $out "BoatCrewMod\sounds")

Compress-Archive -Path (Join-Path $out "*") -DestinationPath $zip
$files = (Get-ChildItem $out -Recurse -File).Count
$hash = (Get-FileHash $dll -Algorithm MD5).Hash.Substring(0, 8)
Write-Host "Packaged $name : $files files, $((Get-Item $zip).Length) bytes, version.dll $hash" -ForegroundColor Green
Write-Host $zip
