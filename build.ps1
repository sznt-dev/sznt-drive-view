# Builds everything: tests, both plugins, the three installers and the release folder (dist\).
#   powershell -ExecutionPolicy Bypass -File build.ps1
#   powershell -ExecutionPolicy Bypass -File build.ps1 -SkipTests
# Needs mingw-w64 (g++, gcc, windres) on PATH, in $env:SZNT_MINGW, or in C:\msys64\mingw64\bin.
param([switch]$SkipTests)
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$src = "$root\src"
$build = "$root\build"
$sdk = "$root\sdk\include"

if (-not (Get-Command g++ -ErrorAction SilentlyContinue)) {
    foreach ($p in @($env:SZNT_MINGW, "C:\msys64\mingw64\bin")) {
        if ($p -and (Test-Path "$p\g++.exe")) { $env:PATH = "$p;$env:PATH"; break }
    }
}
if (-not (Get-Command g++ -ErrorAction SilentlyContinue)) { throw "g++ (mingw-w64) not found" }

$version = ([regex]::Match((Get-Content "$src\common\sznt_version.h" -Raw), 'SZNT_VERSION\s+"([^"]+)"')).Groups[1].Value
Write-Host "SZNT $version  ($((& g++ --version)[0]))" -ForegroundColor Cyan
New-Item -ItemType Directory -Force $build | Out-Null

function Run([string]$exe, [string[]]$a) {
    & $exe @a
    if ($LASTEXITCODE -ne 0) { throw "failed: $exe $($a -join ' ')" }
}

if (-not $SkipTests) {
    Write-Host "[tests]" -ForegroundColor Yellow
    $tests = @(
        @{ f = "drive_weight"; i = "drive"; a = @() },
        @{ f = "view_surface"; i = "view"; a = @() },
        @{ f = "view_cabin"; i = "view"; a = @() },
        @{ f = "controls_patch_test"; i = "setup"; a = @("$root\tests\fixtures") },
        @{ f = "ini_test"; i = "setup"; a = @($src) }
    )
    foreach ($t in $tests) {
        Run g++ @("-O2", "-std=c++17", "-Wall", "-Wextra", "-I$src\$($t.i)", "$root\tests\$($t.f).cpp", "-o", "$build\test_$($t.f).exe")
        Run "$build\test_$($t.f).exe" $t.a
    }
}

$dll = @("-O2", "-std=c++17", "-shared", "-static", "-static-libgcc", "-static-libstdc++", "-s", "-Wall", "-Wextra",
         "-Wno-missing-field-initializers", "-Wl,--no-insert-timestamp", "-I$sdk", "-I$src\common")
foreach ($m in @("drive", "view")) {
    Write-Host "[sznt-$m.dll]" -ForegroundColor Yellow
    Push-Location $src
    try { Run windres @("-Icommon", "$m/sznt_$m.rc", "-O", "coff", "-o", "../build/sznt_$m.res.o") } finally { Pop-Location }
    Run g++ ($dll + @("-I$src\$m", "-o", "$build\sznt-$m.dll", "$src\$m\sznt_$m.cpp", "$build\sznt_$m.res.o", "-lshell32", "-lole32", "-luuid", "-luser32"))
    foreach ($l in @("en", "pt", "es", "de")) { Copy-Item "$src\$m\sznt-$m.$l.ini" $build -Force }
}

# mingw links a default manifest that clashes with ours: shadow it with an empty object (path without spaces)
$noman = Join-Path $env:TEMP "sznt-noman"
New-Item -ItemType Directory -Force $noman | Out-Null
[IO.File]::WriteAllText("$noman\empty.c", "int sznt_no_default_manifest;`n")
Run gcc @("-c", "$noman\empty.c", "-o", "$noman\default-manifest.o")

$langs = @("EN", "PT", "ES", "DE")
$editions = @(
    @{ n = 0; exe = "SZNT-Setup"; mods = @("drive", "view") },
    @{ n = 1; exe = "SZNT-Drive-Setup"; mods = @("drive") },
    @{ n = 2; exe = "SZNT-View-Setup"; mods = @("view") }
)
foreach ($e in $editions) {
    Write-Host "[$($e.exe).exe]" -ForegroundColor Yellow
    $ed = "$build\ed$($e.n)"
    New-Item -ItemType Directory -Force $ed | Out-Null
    $items = @()
    foreach ($m in $e.mods) {
        $items += @{ id = "IDR_$($m.ToUpper())_DLL"; file = "sznt-$m.dll" }
        for ($i = 0; $i -lt 4; $i++) { $items += @{ id = "IDR_$($m.ToUpper())_INI_$($langs[$i])"; file = "sznt-$m.$($langs[$i].ToLower()).ini" } }
    }
    $h = "#pragma once`n#include `"setup_res.h`"`nstruct PayloadItem { int id; const char *sha256; };`nstatic const PayloadItem SZNT_PAYLOAD[] = {`n"
    $rc = "#include `"setup_res.h`"`n"
    foreach ($it in $items) {
        $hash = (Get-FileHash "$build\$($it.file)" -Algorithm SHA256).Hash.ToLower()
        $h += "    { $($it.id), `"$hash`" },`n"
        $rc += "$($it.id) RCDATA `"$(("$build\$($it.file)").Replace('\', '/'))`"`n"
    }
    [IO.File]::WriteAllText("$ed\payload_hash.h", $h + "};`n")
    [IO.File]::WriteAllText("$ed\payload.rc", $rc)
    Push-Location "$src\setup"
    try {
        Run windres @("-DSZNT_EDITION=$($e.n)", "-I.", "-I../common", "setup.rc", "-O", "coff", "-o", "../../build/ed$($e.n)/setup.res.o")
        Run windres @("-I.", "../../build/ed$($e.n)/payload.rc", "-O", "coff", "-o", "../../build/ed$($e.n)/payload.res.o")
    } finally { Pop-Location }
    Run g++ @("-B$($noman.Replace('\', '/'))/", "-O2", "-std=c++17", "-static", "-static-libgcc", "-static-libstdc++", "-s", "-Wall", "-Wextra",
              "-Wl,--no-insert-timestamp", "-municode", "-mwindows", "-DSZNT_EDITION=$($e.n)", "-I$src\common", "-I$src\setup", "-I$ed",
              "-o", "$ed\$($e.exe).exe", "$src\setup\setup.cpp", "$ed\setup.res.o", "$ed\payload.res.o",
              "-ld2d1", "-ldwrite", "-ldwmapi", "-lole32", "-luuid", "-lshell32", "-lbcrypt", "-lwinhttp", "-lversion", "-ladvapi32", "-luser32")
}

Write-Host "[dist]" -ForegroundColor Yellow
$dist = "$root\dist\SZNT-v$version"
New-Item -ItemType Directory -Force $dist | Out-Null
foreach ($e in $editions) { Copy-Item "$build\ed$($e.n)\$($e.exe).exe" "$dist\$($e.exe)-v$version.exe" -Force }
$manual = "$build\manual"
New-Item -ItemType Directory -Force "$manual\plugins", "$manual\ini" | Out-Null
Copy-Item "$build\sznt-drive.dll", "$build\sznt-view.dll" "$manual\plugins\" -Force
Copy-Item "$build\sznt-drive.en.ini" "$manual\plugins\sznt-drive.ini" -Force
Copy-Item "$build\sznt-view.en.ini" "$manual\plugins\sznt-view.ini" -Force
foreach ($l in @("pt", "es", "de")) { foreach ($m in @("drive", "view")) { Copy-Item "$build\sznt-$m.$l.ini" "$manual\ini\" -Force } }
Copy-Item "$root\docs\MANUAL-INSTALL.md" "$manual\README.txt" -Force
Copy-Item "$root\LICENSE" "$manual\LICENSE.txt" -Force
Copy-Item "$root\THIRD_PARTY.md" "$manual\THIRD_PARTY.txt" -Force
$zip = "$dist\SZNT-v$version-manual.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path "$manual\*" -DestinationPath $zip
$sums = Get-ChildItem $dist -File | Where-Object { $_.Name -ne "SHA256SUMS.txt" } |
        ForEach-Object { "$((Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLower())  $($_.Name)" }
[IO.File]::WriteAllText("$dist\SHA256SUMS.txt", ($sums -join "`n") + "`n")
Get-ChildItem $dist | Select-Object Name, Length | Format-Table -AutoSize
