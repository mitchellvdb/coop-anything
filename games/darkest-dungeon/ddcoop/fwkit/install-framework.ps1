<#
  Puts the co-op framework (SDL2.dll + ddcoop.ini) into the EXISTING test copy
  (%USERPROFILE%\DD-coop-test\DarkestDungeon). Your real install and saves are not touched.
      powershell -ExecutionPolicy Bypass -File .\install-framework.ps1
  The DLL that was there is kept as SDL2-previous.dll; an existing ddcoop.ini is kept as ddcoop.ini.bak.
#>
$ErrorActionPreference = "Stop"
$ExpectedFramework = "88a7917710e8a2641c9a6aa64e9d2bd81904ecef9911de36fe4f388bb5cb42ef"
$bin = Join-Path $env:USERPROFILE "DD-coop-test\DarkestDungeon\_windows\win64"
$KitDir = Split-Path -Parent $MyInvocation.MyCommand.Path

if (-not (Test-Path -LiteralPath (Join-Path $bin "SDL2_orig.dll"))) {
    throw "No test copy with SDL2_orig.dll found in $bin. Run install-testkit.ps1 from test kit 1 first."
}
$dll = Join-Path $KitDir "SDL2.dll"
if ((Get-FileHash -Algorithm SHA256 -LiteralPath $dll).Hash.ToLower() -ne $ExpectedFramework) {
    throw "The SDL2.dll in this kit does not match its expected hash. Nothing was changed."
}
$current = Join-Path $bin "SDL2.dll"
if (Test-Path -LiteralPath $current) { Copy-Item -LiteralPath $current -Destination (Join-Path $bin "SDL2-previous.dll") -Force }
Copy-Item -LiteralPath $dll -Destination $current -Force
$ini = Join-Path $bin "ddcoop.ini"
if (Test-Path -LiteralPath $ini) { Copy-Item -LiteralPath $ini -Destination "$ini.bak" -Force }
Copy-Item -LiteralPath (Join-Path $KitDir "ddcoop.ini") -Destination $ini -Force
Write-Host "Co-op framework 0.2 installed in the test copy: $bin"
