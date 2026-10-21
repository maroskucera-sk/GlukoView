# Generovanie LVGL fontov so slovenskou diakritikou
# Potrebuje: Node.js (https://nodejs.org) + TTF font (napr. Roboto / DejaVu Sans)
#
# Pouzitie:
#   1. Nainstaluj Node.js LTS
#   2. Stiahni TTF do tools\ (napr. Roboto-Regular.ttf)
#   3. Spusti:  powershell -ExecutionPolicy Bypass -File tools\generate_fonts.ps1
#
# Rozsahy: ASCII + Latin-1 (áäéíóúýô...) + Latin Extended-A (čďľňšťž...) + sipky + stupen

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$src = Join-Path $root "src"
$fontFile = Join-Path $PSScriptRoot "Roboto-Regular.ttf"

if (-not (Get-Command npm -ErrorAction SilentlyContinue)) {
    Write-Host "Chyba: npm/Node.js nie je nainstalovane. https://nodejs.org"
    exit 1
}

if (-not (Test-Path $fontFile)) {
    Write-Host "Chyba: nenajdeny $fontFile"
    Write-Host "Stiahni Roboto-Regular.ttf (Google Fonts) do priecinka tools\"
    exit 1
}

# ASCII + °± + Latin + trendove sipky ←↑→↓↖↗↘↙ (0x2190-0x2199)
$ranges = "0x20-0x7E,0xB0-0xB1,0xC0-0x17E,0x2190-0x2199"

Write-Host "Generujem font_sk_16.c (menu)..."
npx --yes lv_font_conv --font $fontFile --size 16 --bpp 4 --format lvgl --lv-include lvgl.h -r $ranges -o (Join-Path $src "font_sk_16.c") --force-fast-kern-format

Write-Host "Generujem font_sk_20.c (UI texty + sipky)..."
npx --yes lv_font_conv --font $fontFile --size 20 --bpp 4 --format lvgl --lv-include lvgl.h -r $ranges -o (Join-Path $src "font_sk_20.c") --force-fast-kern-format

Write-Host "Generujem font_sk_48.c (glykemia + sipky)..."
npx --yes lv_font_conv --font $fontFile --size 48 --bpp 4 --format lvgl --lv-include lvgl.h -r $ranges -o (Join-Path $src "font_sk_48.c") --force-fast-kern-format

Write-Host "Hotovo. V main.cpp pridaj LV_FONT_DECLARE(font_sk_16) a pouzi pre menu tlacidla."
Write-Host "Potom: pio run -e waveshare_s3_1_28"
