# Regeneruj LVGL fonty pre Dia Monitor (SK/CZ/EN/ES/DE).
# Vyžaduje: npm i -g lv_font_conv
$ErrorActionPreference = "Stop"
$Root = Split-Path $PSScriptRoot -Parent
$Ttf = Join-Path $Root ".pio\libdeps\waveshare_s3_3_5\lvgl\scripts\built_in_font\Montserrat-Medium.ttf"
$Out = Join-Path $Root "src"
if (-not (Test-Path $Ttf)) { throw "Chyba TTF: $Ttf" }

$symsUi = "°±←↑→↓↖↗↘↙¿¡"
$range = "32-126,160-383,8592-8601"
$symsLg = "0123456789.:-+ ±←↑→↓↖↗↘↙"

function Gen($size, $file, $full) {
    $args = @("--bpp", "1", "--size", "$size", "--no-compress", "--font", $Ttf, "--format", "lvgl", "-o", (Join-Path $Out $file))
    if ($full) {
        $args += @("--symbols", $symsUi, "--range", $range)
    } else {
        $args += @("--symbols", $symsLg)
    }
    Write-Host ">> $file ($size px, full=$full)"
    & lv_font_conv @args
}

Gen 16 "font_sk_16.c" $true
Gen 20 "font_sk_20.c" $true
Gen 24 "font_sk_24.c" $true
Gen 48 "font_sk_48.c" $false
Gen 72 "font_sk_72.c" $false
Gen 88 "font_sk_88.c" $false

# PlatformIO používa LV_CONF_INCLUDE_SIMPLE → #include "lvgl.h"
$header = @"
#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif
"@
Get-ChildItem (Join-Path $Out "font_sk_*.c") | ForEach-Object {
    $c = Get-Content $_.FullName -Raw
    $c2 = [regex]::Replace($c, '(?s)#ifdef __has_include.*?#include "lvgl/lvgl\.h"\r?\n#endif', $header)
    if ($c2 -eq $c) { $c2 = $c -replace '#include "lvgl/lvgl\.h"', '#include "lvgl.h"' }
    Set-Content $_.FullName -Value $c2 -NoNewline
}
Get-ChildItem (Join-Path $Out "font_sk_*.c") | Select-Object Name, Length
