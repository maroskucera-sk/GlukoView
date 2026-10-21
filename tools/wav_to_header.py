#!/usr/bin/env python3
"""Embed assets/audio/*.wav → include/audio_*_wav.h (16 kHz mono PCM16).

Usage:
  python tools/wav_to_header.py                  # startup + beep
  python tools/wav_to_header.py assets/audio/beep.wav
"""
from __future__ import annotations

import struct
import sys
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
AUDIO_DIR = ROOT / "assets" / "audio"
INCLUDE_DIR = ROOT / "include"


def stem_to_symbol(stem: str) -> str:
    parts = "".join(c if c.isalnum() else "_" for c in stem).split("_")
    return "".join(p[:1].upper() + p[1:] for p in parts if p) or "Wav"


def convert_one(src: Path) -> int:
    if not src.is_file():
        print(f"missing: {src}", file=sys.stderr)
        return 1

    with wave.open(str(src), "rb") as w:
        ch, width, rate, nframes, _, _ = w.getparams()
        raw = w.readframes(nframes)

    if width != 2:
        print(f"{src.name}: need 16-bit PCM WAV", file=sys.stderr)
        return 1
    if rate != 16000:
        print(f"{src.name}: warning: sample rate is {rate} Hz (firmware expects 16000)", file=sys.stderr)

    samples = list(struct.unpack(f"<{nframes * ch}h", raw))
    if ch == 2:
        samples = [((samples[i] + samples[i + 1]) // 2) for i in range(0, len(samples), 2)]
    elif ch != 1:
        print(f"{src.name}: unsupported channels: {ch}", file=sys.stderr)
        return 1

    peak = max((abs(s) for s in samples), default=0)
    print(
        f"{src.name}: {len(samples)} samples @ {rate} Hz, "
        f"peak={peak}/32767 ({100.0 * peak / 32767:.1f}% FS), "
        f"dur={len(samples) / float(rate):.2f}s"
    )

    sym = stem_to_symbol(src.stem)
    out = INCLUDE_DIR / f"audio_{src.stem}_wav.h"
    lines = [
        "#pragma once",
        f"// Auto-generated from {src.as_posix()} — {rate} Hz mono 16-bit PCM",
        "#include <stdint.h>",
        "",
        f"static constexpr int k{sym}WavSampleRate = {rate};",
        f"static constexpr int k{sym}WavSampleCount = {len(samples)};",
        f"static const int16_t k{sym}WavPcm[k{sym}WavSampleCount] = {{",
    ]
    row = []
    for i, s in enumerate(samples):
        row.append(str(s))
        if len(row) == 16 or i == len(samples) - 1:
            lines.append("    " + ", ".join(row) + ",")
            row = []
    lines.append("};")
    lines.append("")
    out.write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {out}")
    return 0


def main() -> int:
    if len(sys.argv) > 1:
        return convert_one(Path(sys.argv[1]))
    rc = 0
    for name in ("startup.wav", "beep.wav"):
        rc |= convert_one(AUDIO_DIR / name)
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
