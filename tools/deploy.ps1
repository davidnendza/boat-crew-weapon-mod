# DEV ONLY (not part of a release): builds, then installs the moment Boat Crew is closed, so a fix waits for the end of
# the play session instead of a "game closed, install" round trip. Optionally starts the game again through Steam.
#     powershell -ExecutionPolicy Bypass -File tools\deploy.ps1            build, wait for the game to close, install
#     powershell -ExecutionPolicy Bypass -File tools\deploy.ps1 -NoBuild   install the existing out\version.dll
#     powershell -ExecutionPolicy Bypass -File tools\deploy.ps1 -Launch    and start Boat Crew afterwards
# The build is copied to out\staged\ when this starts, so a newer build made while it waits is not swept in unreviewed.
param([switch]$NoBuild, [switch]$Launch)
$root = Split-Path $PSScriptRoot -Parent

if (-not $NoBuild) {
  $out = & cmd /c "`"$root\build.bat`"" 2>&1
  $out | Select-String -Pattern " error |Built" | ForEach-Object { $_.Line }
  if ($LASTEXITCODE -ne 0) { Write-Host "Build failed; nothing installed." -ForegroundColor Red; exit 1 }
}
$staged = Join-Path $root "out\staged"
New-Item -ItemType Directory -Force $staged | Out-Null
Copy-Item "$root\out\version.dll" "$staged\version.dll" -Force
$hash = (Get-FileHash "$staged\version.dll" -Algorithm MD5).Hash.Substring(0, 8)

$waited = $false
while (Get-Process -Name "Boat Crew" -ErrorAction SilentlyContinue) {
  if (-not $waited) { Write-Host "$(Get-Date -Format HH:mm:ss) build $hash staged; Boat Crew is running, installing as soon as it closes"; $waited = $true }
  Start-Sleep -Seconds 2
}
if ($waited) { Start-Sleep -Seconds 3 }                               # let the closing game release version.dll
Write-Host "$(Get-Date -Format HH:mm:ss) installing build $hash"
& "$root\install.ps1" -Source "$staged\version.dll"
if ($Launch) { Start-Process "steam://rungameid/1633370"; Write-Host "Started Boat Crew through Steam." }
