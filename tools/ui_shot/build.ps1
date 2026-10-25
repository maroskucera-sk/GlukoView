# Build and run the screenshot host for one or both boards.
# Needs the portable GCC from %LOCALAPPDATA%\ui_shot_toolchain\w64devkit
# (tools/ui_shot does not compile with the ESP32 toolchain).
#   powershell -File tools\ui_shot\build.ps1            # both
#   powershell -File tools\ui_shot\build.ps1 -Board ws35

param(
    [ValidateSet("ws128", "ws35", "both")]
    [string]$Board = "both"
)

$ErrorActionPreference = "Stop"
if ($Board -eq "both") {
    & $PSCommandPath -Board ws128
    & $PSCommandPath -Board ws35
    return
}
$boardDefine = if ($Board -eq "ws35") { "-DDIA_BOARD_WS_3_5" } else { "-DDIA_BOARD_WS_1_28" }
$imgRel = if ($Board -eq "ws35") { "docs/img/ws35" } else { "docs/img/ws128" }
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$bin = Join-Path $env:LOCALAPPDATA "ui_shot_toolchain\w64devkit\bin"
$gxx = Join-Path $bin "g++.exe"
$gcc = Join-Path $bin "gcc.exe"
if (-not (Test-Path $gxx)) {
    throw "g++ not found at $gxx"
}
$env:PATH = "$bin;" + $env:PATH

$build = Join-Path $PSScriptRoot ("build\" + $Board)
$obj = Join-Path $build "obj"
if (Test-Path $obj) { Remove-Item -Recurse -Force $obj }
New-Item -ItemType Directory -Force -Path $obj | Out-Null

$lvgl = Join-Path $root ".pio\libdeps\waveshare_s3_1_28\lvgl"
function Quote-Arg([string]$s) {
    if ($s -match '[\s"]') { return '"' + ($s -replace '"', '\"') + '"' }
    return $s
}
$incs = @(
    ("-I" + (Quote-Arg $PSScriptRoot)),
    ("-I" + (Quote-Arg (Join-Path $PSScriptRoot "stub"))),
    ("-I" + (Quote-Arg (Join-Path $root "include"))),
    ("-I" + (Quote-Arg $lvgl))
)
$defs = @(
    "-DLV_CONF_INCLUDE_SIMPLE",
    $boardDefine,
    "-DFIRMWARE_VERSION_MAJOR=2",
    "-DFIRMWARE_VERSION_MINOR=85",
    "-D_USE_MATH_DEFINES"
)
$cxx = @("-O1", "-std=c++17", "-finput-charset=UTF-8") + $incs + $defs + @(
    "-Wno-unused-parameter", "-Wno-unused-function", "-Wno-missing-field-initializers", "-Wno-sign-compare"
)
$cc = @("-O1", "-std=c11") + $incs + $defs + @("-w")

$sources = New-Object System.Collections.Generic.List[object]
function Add-Src($compiler, $flags, $path) {
    $script:sources.Add([pscustomobject]@{ Compiler = $compiler; Flags = $flags; Path = $path })
}

Add-Src $gxx $cxx (Join-Path $PSScriptRoot "host_main.cpp")
Add-Src $gxx $cxx (Join-Path $PSScriptRoot "host_stub.cpp")
Add-Src $gxx $cxx (Join-Path $PSScriptRoot "png_write.cpp")
Add-Src $gxx $cxx (Join-Path $root "src\dia_ui.cpp")
Add-Src $gxx $cxx (Join-Path $root "src\dia_i18n.cpp")
Add-Src $gxx $cxx (Join-Path $root "src\dia_legal.cpp")
Add-Src $gxx $cxx (Join-Path $root "src\dia_glucose.cpp")
foreach ($f in @("font_sk_16.c","font_sk_20.c","font_sk_24.c","font_sk_48.c","font_sk_72.c","font_sk_88.c","logo_boot.c")) {
    Add-Src $gcc $cc (Join-Path $root "src\$f")
}
Get-ChildItem -Path (Join-Path $lvgl "src") -Recurse -Filter *.c | ForEach-Object {
    Add-Src $gcc $cc $_.FullName
}

$running = New-Object System.Collections.Generic.List[object]
$failed = New-Object System.Collections.Generic.List[string]
$n = 0
foreach ($s in $sources) {
    while (@($running | Where-Object { -not $_.Proc.HasExited }).Count -ge 8) {
        Start-Sleep -Milliseconds 40
    }
    foreach ($done in @($running | Where-Object { $_.Proc.HasExited })) {
        $done.Proc.WaitForExit() | Out-Null
        $errText = ""
        if (Test-Path $done.Err) { $errText += Get-Content -Raw $done.Err -ErrorAction SilentlyContinue }
        $outFile = "$($done.Err).out"
        if (Test-Path $outFile) { $errText += Get-Content -Raw $outFile -ErrorAction SilentlyContinue }
        $code = $done.Proc.ExitCode
        if (-not (Test-Path $done.Obj) -or ($null -ne $code -and $code -ne 0) -or ($errText -match '(?m)error:')) {
            $failed.Add("$($done.Path)`n$errText")
        }
        $running.Remove($done) | Out-Null
    }
    $n++
    $o = Join-Path $obj ("o{0}.o" -f $n)
    $err = Join-Path $obj ("o{0}.err" -f $n)
    $compileArgs = @($s.Flags) + @("-c", (Quote-Arg $s.Path), "-o", (Quote-Arg $o))
    $p = Start-Process -FilePath $s.Compiler -ArgumentList $compileArgs -PassThru -NoNewWindow -RedirectStandardError $err -RedirectStandardOutput "$err.out"
    $running.Add([pscustomobject]@{ Proc = $p; Err = $err; Path = $s.Path; Obj = $o })
}
foreach ($done in $running) {
    $done.Proc.WaitForExit() | Out-Null
    $errText = ""
    if (Test-Path $done.Err) { $errText += Get-Content -Raw $done.Err -ErrorAction SilentlyContinue }
    $outFile = "$($done.Err).out"
    if (Test-Path $outFile) { $errText += Get-Content -Raw $outFile -ErrorAction SilentlyContinue }
    $code = $done.Proc.ExitCode
    if (-not (Test-Path $done.Obj) -or ($null -ne $code -and $code -ne 0) -or ($errText -match '(?m)error:')) {
        $failed.Add("$($done.Path)`n$errText")
    }
}
if ($failed.Count -gt 0) {
    $failed | Select-Object -First 8 | ForEach-Object { Write-Output $_ }
    throw "$($failed.Count) compile errors"
}

$rsp = Join-Path $build "link.rsp"
$lines = Get-ChildItem $obj -Filter *.o | ForEach-Object { '"' + ($_.FullName -replace '\\', '/') + '"' }
Set-Content -Path $rsp -Value ($lines -join "`n") -Encoding ascii
$exe = Join-Path $build "ui_shot.exe"
& $gxx "@$rsp" "-o" $exe "-static-libgcc" "-static-libstdc++"
if ($LASTEXITCODE -ne 0) { throw "link failed" }

$env:PATH = "$bin;" + $env:PATH
Push-Location $root
try {
    & $exe $imgRel
    if ($LASTEXITCODE -ne 0) { throw "ui_shot exited $LASTEXITCODE" }
} finally {
    Pop-Location
}
Write-Output "screenshots in $imgRel"
