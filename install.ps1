# WEAPON MOD: copies the freshly built out\version.dll into the Boat Crew game folder and starts a fresh log.
# This REPLACES the first-person mod's or the gun probe's version.dll; only one version.dll can be installed at a time.
# Run from PowerShell with the game CLOSED:
#     .\install.ps1                      finds the game through Steam
#     .\install.ps1 -GamePath "D:\Games\Boat Crew"   if it cannot
param([string]$GamePath, [string]$Source)

function Find-BoatCrew {
  param([string]$Hint)
  $candidates = @()
  if ($Hint) { $candidates += $Hint }
  $steam = $null
  try { $steam = (Get-ItemProperty "HKCU:\Software\Valve\Steam" -ErrorAction Stop).SteamPath } catch {}
  if ($steam) {
    $candidates += (Join-Path $steam "steamapps\common\Boat Crew")
    $vdf = Join-Path $steam "steamapps\libraryfolders.vdf"
    if (Test-Path $vdf) {
      foreach ($m in [regex]::Matches((Get-Content $vdf -Raw), '"path"\s+"([^"]+)"')) {
        $candidates += (Join-Path ($m.Groups[1].Value -replace '\\', '\') "steamapps\common\Boat Crew")
      }
    }
  }
  $candidates += "C:\Program Files (x86)\Steam\steamapps\common\Boat Crew"
  foreach ($c in $candidates) { if ($c -and (Test-Path (Join-Path $c "Boat Crew.exe"))) { return $c } }
  return $null
}

$game = Find-BoatCrew $GamePath
if (-not $game) { Write-Host "Could not find Boat Crew. Run again with -GamePath pointing at the folder that holds 'Boat Crew.exe'." -ForegroundColor Yellow; return }
$src = if ($Source) { $Source } else { Join-Path $PSScriptRoot "out\version.dll" }
$dst = Join-Path $game "version.dll"

if (Get-Process -Name "Boat Crew" -ErrorAction SilentlyContinue) { Write-Host "Boat Crew is running. Close it first, then run this again." -ForegroundColor Yellow; return }
if (-not (Test-Path $src)) { Write-Host "No build found at $src. Run build.bat first." -ForegroundColor Yellow; return }

Copy-Item $src $dst -Force
# the last session's log is kept as weaponmod.prev.log (an install right after closing the game must not lose it)
$log = Join-Path $game "BoatCrewMod\weaponmod.log"
if (Test-Path $log) { Move-Item $log (Join-Path $game "BoatCrewMod\weaponmod.prev.log") -Force }
# gun models (model= in weapons.ini): copied into BoatCrewMod\models, never deleting models the player added
$models = Join-Path $PSScriptRoot "models"
if (Test-Path $models) {
  $mdst = Join-Path $game "BoatCrewMod\models"
  New-Item -ItemType Directory -Force $mdst | Out-Null
  Copy-Item (Join-Path $models "*") $mdst -Force
}
# sounds the mod plays itself (the chomper torpedoes' chomp and gulp): BoatCrewMod\sounds, the same way
$sounds = Join-Path $PSScriptRoot "sounds"
if (Test-Path $sounds) {
  $sdst = Join-Path $game "BoatCrewMod\sounds"
  New-Item -ItemType Directory -Force $sdst | Out-Null
  Copy-Item (Join-Path $sounds "*") $sdst -Force
}

$built = (Get-FileHash $src -Algorithm MD5).Hash
$there = (Get-FileHash $dst -Algorithm MD5).Hash
if ($built -eq $there) {
  Write-Host "Installed to $game" -ForegroundColor Green
  Write-Host "version.dll matches the build ($($built.Substring(0,8))..., $((Get-Item $dst).Length) bytes)."
  Write-Host "Start the game. Items are defined in BoatCrewMod\weapons.ini (created on first launch); the log is BoatCrewMod\weaponmod.log."
} else {
  Write-Host "The copy does not match the build. Installed $($there.Substring(0,8))..., built $($built.Substring(0,8))...." -ForegroundColor Red
}
