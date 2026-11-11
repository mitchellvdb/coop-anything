<#
.SYNOPSIS
  Signs DDCoop kits for the launcher's update offer (launcher/UPDATE.md).

  sign_kit.ps1 -NewKey                      creates the owner's signing key once and prints its public key
  sign_kit.ps1 -PublicKey                   prints the public key (128 hex: X||Y) for LR_KIT_PUBKEY
  sign_kit.ps1 -Sign <kit dir> -Version v   writes <kit dir>\ddcoop-kit.sig for DDCoop.exe, SDL2.dll and
                                            DDCoop\ddcoop-runtime.dll in that folder

  The private key is an ECDSA P-256 key, stored encrypted with Windows DPAPI for the current Windows user
  (default %USERPROFILE%\DDCoop-signing\kit-signing.key). It never goes into git or a kit. Anyone who can
  log in as this Windows user can sign kits; a lost key means building a launcher with a new public key and
  installing that once by hand.
#>
param(
    [switch]$NewKey,
    [switch]$PublicKey,
    [string]$Sign,
    [string]$Version,
    [string]$KeyPath = (Join-Path $env:USERPROFILE 'DDCoop-signing\kit-signing.key')
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Security
Add-Type -AssemblyName System.Core

function Load-Key {
    if (-not (Test-Path $KeyPath)) { throw "No signing key at $KeyPath (run -NewKey once)." }
    $blob = [System.Security.Cryptography.ProtectedData]::Unprotect([IO.File]::ReadAllBytes($KeyPath), $null,
        [System.Security.Cryptography.DataProtectionScope]::CurrentUser)
    return [System.Security.Cryptography.CngKey]::Import($blob, [System.Security.Cryptography.CngKeyBlobFormat]::EccPrivateBlob)
}
function Public-Hex($key) {
    $pub = $key.Export([System.Security.Cryptography.CngKeyBlobFormat]::EccPublicBlob)
    if ($pub.Length -ne 72) { throw 'unexpected public key blob' }
    return (($pub[8..71] | ForEach-Object { $_.ToString('x2') }) -join '')
}
function Hex($bytes) { return (($bytes | ForEach-Object { $_.ToString('x2') }) -join '') }

if ($NewKey) {
    if (Test-Path $KeyPath) { throw "A signing key already exists at $KeyPath; not replaced." }
    $p = New-Object System.Security.Cryptography.CngKeyCreationParameters
    $p.ExportPolicy = [System.Security.Cryptography.CngExportPolicies]::AllowPlaintextExport
    # An ephemeral key ([NullString] keeps PowerShell from turning $null into a persisted key named ""):
    # the only stored copy is the DPAPI-encrypted file below.
    $key = [System.Security.Cryptography.CngKey]::Create([System.Security.Cryptography.CngAlgorithm]::ECDsaP256, [NullString]::Value, $p)
    if (-not $key.IsEphemeral) { $key.Delete(); throw 'CNG created a persisted key; refused.' }
    $blob = $key.Export([System.Security.Cryptography.CngKeyBlobFormat]::EccPrivateBlob)
    $enc = [System.Security.Cryptography.ProtectedData]::Protect($blob, $null,
        [System.Security.Cryptography.DataProtectionScope]::CurrentUser)
    New-Item -ItemType Directory -Force (Split-Path $KeyPath) | Out-Null
    [IO.File]::WriteAllBytes($KeyPath, $enc)
    Write-Output (Public-Hex $key)
    exit 0
}
if ($PublicKey) { Write-Output (Public-Hex (Load-Key)); exit 0 }
if ($Sign) {
    if ($Version -notmatch '^[a-z0-9.\-]{1,31}$' -or $Version -notmatch '-test([1-9][0-9]{0,5})$') {
        throw "Version must look like 0.5.0-test77 (duo and dev builds are not signed for updates)."
    }
    $serial = [int]$Matches[1]
    $files = @(
        @('DDCoop.exe', (Join-Path $Sign 'DDCoop.exe')),
        @('SDL2.dll', (Join-Path $Sign 'SDL2.dll')),
        @('ddcoop-runtime.dll', (Join-Path $Sign 'DDCoop\ddcoop-runtime.dll'))
    )
    $sha = [System.Security.Cryptography.SHA256]::Create()
    $lines = @('DDCoop kit 1', "version=$Version", "serial=$serial")
    $entries = @()
    foreach ($f in $files) {
        $bytes = [IO.File]::ReadAllBytes($f[1])
        if ($bytes.Length -le 0 -or $bytes.Length -gt 16MB) { throw "$($f[0]) has an unsupported size" }
        $entries += "file=$($f[0]) $($bytes.Length) $(Hex $sha.ComputeHash($bytes))"
        if ($f[0] -eq 'ddcoop-runtime.dll') { $kit = (Hex $sha.ComputeHash($bytes)).Substring(0, 8) }
    }
    $lines += "kit=$kit"
    $lines += $entries
    $signed = [Text.Encoding]::ASCII.GetBytes((($lines -join "`n") + "`n"))
    $ecdsa = New-Object System.Security.Cryptography.ECDsaCng (Load-Key)
    $sig = $ecdsa.SignData($signed, [System.Security.Cryptography.HashAlgorithmName]::SHA256)
    if ($sig.Length -ne 64 -or -not $ecdsa.VerifyData($signed, $sig, [System.Security.Cryptography.HashAlgorithmName]::SHA256)) {
        throw 'signature check failed'
    }
    $all = $signed + [Text.Encoding]::ASCII.GetBytes("signature=$(Hex $sig)`n")
    [IO.File]::WriteAllBytes((Join-Path $Sign 'ddcoop-kit.sig'), $all)
    Write-Output "signed $Version serial $serial kit $kit"
    exit 0
}
throw 'Use -NewKey, -PublicKey or -Sign <dir> -Version <version>.'
