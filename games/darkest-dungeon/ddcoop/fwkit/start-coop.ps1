<#
  Starts the TEST COPY of Darkest Dungeon for co-op, after backing up all Darkest Dungeon saves.
      powershell -ExecutionPolicy Bypass -File .\start-coop.ps1            (test mode: -nosave, the game saves nothing)
  The backup zips Steam\userdata\<account>\262060\remote (every campaign slot) into
  %USERPROFILE%\DD-coop-test\backups before the game starts. It only reads your saves.
#>
$ErrorActionPreference = "Stop"
$TestRoot = Join-Path $env:USERPROFILE "DD-coop-test"
$bin = Join-Path $TestRoot "DarkestDungeon\_windows\win64"
$exe = Join-Path $bin "Darkest.exe"
if (-not (Test-Path -LiteralPath (Join-Path $bin "SDL2_orig.dll"))) { throw "No test copy found in $bin." }

# --- backup of every save slot ---
$SteamPath = (Get-ItemProperty -Path "HKCU:\Software\Valve\Steam" -Name SteamPath -ErrorAction SilentlyContinue).SteamPath
$backupDir = Join-Path $TestRoot "backups"
New-Item -ItemType Directory -Force -Path $backupDir | Out-Null
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$found = $false
if ($SteamPath) {
    foreach ($user in Get-ChildItem -Directory -Path (Join-Path $SteamPath "userdata") -ErrorAction SilentlyContinue) {
        $remote = Join-Path $user.FullName "262060\remote"
        if (-not (Test-Path -LiteralPath $remote)) { continue }
        $zip = Join-Path $backupDir "saves-$($user.Name)-$stamp.zip"
        Compress-Archive -Path $remote -DestinationPath $zip
        Write-Host "Saves backed up to $zip"
        $found = $true
    }
}
if (-not $found) {
    Write-Host "No Steam save folder found to back up (Steam\userdata\*\262060\remote)."
    $answer = Read-Host "Start without a backup? (y/n)"
    if ($answer -ne "y") { exit 1 }
}

# --- start the game ---
Remove-Item -LiteralPath (Join-Path $bin "ddcoop-fw.log") -ErrorAction SilentlyContinue
Write-Host "Starting the test copy with -nosave. This window continues when the game has closed."
Start-Process -FilePath $exe -ArgumentList "-nosave" -WorkingDirectory $bin -Wait
Write-Host "Log: $bin\ddcoop-fw.log"
