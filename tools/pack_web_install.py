#!/usr/bin/env python3
"""Zbalí docs/install z práve preložených obrazov Solo a Duo.

Nespúšťa sa u zákazníka. Najprv pio run pre obe dosky, potom tento skript.
"""
from __future__ import annotations

import hashlib
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BOOT_APP0_CANDIDATES = (
    Path.home()
    / ".platformio/packages/framework-arduinoespressif32@3.20009.0/tools/partitions/boot_app0.bin",
    Path.home() / ".platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin",
)
ENVS = {
    "solo": ("waveshare_s3_1_28", "GVBOARD:solo", "GVBOARD:duo", "GlukoView Solo"),
    "duo": ("waveshare_s3_3_5", "GVBOARD:duo", "GVBOARD:solo", "GlukoView Duo"),
}


def firmware_version() -> str:
    major = minor = None
    for line in (ROOT / "platformio.ini").read_text(encoding="utf-8").splitlines():
        if "FIRMWARE_VERSION_MAJOR=" in line:
            major = line.rsplit("=", 1)[-1].strip()
        elif "FIRMWARE_VERSION_MINOR=" in line:
            minor = line.rsplit("=", 1)[-1].strip()
    if major is None or minor is None:
        raise SystemExit("Vo platformio.ini chýba verzia firmvéru.")
    return f"{int(major)}.{int(minor)}"


def boot_app0() -> Path:
    for path in BOOT_APP0_CANDIDATES:
        if path.exists():
            return path
    raise SystemExit("Nenašiel som boot_app0.bin v PlatformIO.")


def require(data: bytes, needle: str, label: str, present: bool) -> None:
    found = needle.encode() in data
    if found != present:
        state = "chýba" if present else "tam nemá byť"
        raise SystemExit(f"{label}: {state} {needle}")


def main() -> None:
    version = firmware_version()
    app0 = boot_app0()
    out_root = ROOT / "docs" / "install"
    boot_hashes = {}
    for key, (env, tag, other, title) in ENVS.items():
        build = ROOT / ".pio" / "build" / env
        firmware = (build / "firmware.bin").read_bytes()
        bootloader = (build / "bootloader.bin").read_bytes()
        partitions = (build / "partitions.bin").read_bytes()
        require(firmware, tag, key, True)
        require(firmware, other, key, False)
        require(firmware, "Abbott", key, True)
        if len(firmware) < 500_000 or len(bootloader) < 1_000 or len(partitions) < 64:
            raise SystemExit(f"{key}: podozrivá veľkosť súborov")
        dest = out_root / key
        dest.mkdir(parents=True, exist_ok=True)
        (dest / "firmware.bin").write_bytes(firmware)
        (dest / "bootloader.bin").write_bytes(bootloader)
        (dest / "partitions.bin").write_bytes(partitions)
        shutil.copyfile(app0, dest / "boot_app0.bin")
        manifest = {
            "name": title,
            "version": version,
            "builds": [
                {
                    "chipFamily": "ESP32-S3",
                    "parts": [
                        {"path": "bootloader.bin", "offset": 0},
                        {"path": "partitions.bin", "offset": 32768},
                        {"path": "boot_app0.bin", "offset": 57344},
                        {"path": "firmware.bin", "offset": 65536},
                    ],
                }
            ],
        }
        (dest / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        boot_hashes[key] = hashlib.sha256(bootloader).hexdigest()
        print(f"{key} {version} firmware={len(firmware)} bootloader={len(bootloader)}")
    if boot_hashes["solo"] == boot_hashes["duo"]:
        print("Bootloader je pre obe dosky rovnaký. Líši sa firmvér, ten sa nesmie vymeniť.")
    print("webovy instalator je v docs/install")


if __name__ == "__main__":
    main()
