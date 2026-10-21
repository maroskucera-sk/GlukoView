# Snímky obrazovky

`ui_shot` preloží to isté LVGL, ktoré kreslí firmvér (`src/dia_ui.cpp`, texty, fonty, logo), a uloží PNG. WiFi, doska a NVS sú len náhrada v pamäti. Pri kruhu sú rohy mimo kruhu priehľadné. Široká doska je celý obdĺžnik 480×320.

Prekladač je prenosný GCC (w64devkit 1.23) v `%LOCALAPPDATA%\ui_shot_toolchain\w64devkit`. Zip: `https://github.com/skeeto/w64devkit/releases/download/v1.23.0/w64devkit-1.23.0.zip`, po rozbalení má byť `w64devkit\bin\g++.exe`.

Spustenie z koreňa projektu:

```powershell
powershell -ExecutionPolicy Bypass -File tools\ui_shot\build.ps1
powershell -ExecutionPolicy Bypass -File tools\ui_shot\build.ps1 -Board ws35
powershell -ExecutionPolicy Bypass -File tools\ui_shot\build.ps1 -Board ws128
```

Bez parametra sa spravia obe dosky. Kruh ide do `docs/img/ws128/`, široká doska do `docs/img/ws35/`.

Návod vo Worde, ktorý tieto obrázky vloží:

```powershell
python tools\ui_shot\make_guide.py
```

Výstup je `docs/GlukoView-navod.docx`.

Spoločné snímky: `lang`, `splash`, `boot-connect`, `boot-setup`, `face-ok`, `face-low`, `face-high`, `face-critical`, `face-stale`, `face-clock`, `face-chart`, `face-source`, `menu-system`, `menu-system-2`, `menu-limits`, `menu-look`, `menu-bright`, `edit-low`, `info`, `confirm-erase`, `pin`, `ota`.

Len široká doska: `face-split`, `face-split-chart`, `menu-sound`. Hodnoty sú ukážkové (6,4 mmol, Nightscout, limity 3,9 / 10 / 15). Na širokej doske sú v ukážke dve osoby, Anna a Adam.
