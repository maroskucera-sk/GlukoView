#!/usr/bin/env python3
"""Podpíše firmware.bin kľúčom tools/ota_private.pem (ECDSA P-256, SHA-256)."""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path


def find_openssl() -> str:
    found = shutil.which("openssl")
    if found:
        return found
    for candidate in (
        Path(r"C:\Program Files\Git\usr\bin\openssl.exe"),
        Path(r"C:\Program Files\Git\mingw64\bin\openssl.exe"),
    ):
        if candidate.exists():
            return str(candidate)
    sys.exit("openssl sa nenašiel. Nainštaluj Git for Windows alebo OpenSSL a skús znova.")


def main() -> None:
    parser = argparse.ArgumentParser(description="Podpis GlukoView firmware.bin")
    parser.add_argument("firmware", type=Path)
    parser.add_argument("--key", type=Path, default=Path(__file__).with_name("ota_private.pem"))
    parser.add_argument("-o", type=Path, default=None)
    args = parser.parse_args()
    if not args.firmware.is_file():
        sys.exit(f"Chýba súbor {args.firmware}")
    if not args.key.is_file():
        sys.exit(
            f"Chýba súkromný kľúč {args.key}. "
            "Vygeneruj ho raz a zálohuj mimo repozitára:\n"
            "openssl ecparam -name prime256v1 -genkey -noout -out tools/ota_private.pem"
        )
    out = args.o or args.firmware.with_suffix(".sig")
    subprocess.check_call(
        [find_openssl(), "dgst", "-sha256", "-sign", str(args.key), "-out", str(out), str(args.firmware)]
    )
    print(out)


if __name__ == "__main__":
    main()
