#!/usr/bin/env python3
"""Remaster assets/audio/beep.wav for the tiny 3.5B speaker.

Original is ~300 Hz — the speaker barely reproduces that, so you only hear thin HF.
We keep full duration and mix:
  - original (body)
  - +1 octave layer (what the speaker can actually play)
Then soft-limit and write beep_play.wav + include/audio_beep_wav.h
"""
from __future__ import annotations

import struct
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "assets" / "audio" / "beep.wav"
OUT_WAV = ROOT / "assets" / "audio" / "beep_play.wav"
OUT_H = ROOT / "include" / "audio_beep_wav.h"


def read_mono(path: Path):
    with wave.open(str(path), "rb") as w:
        ch, width, rate, nframes, *_ = w.getparams()
        raw = w.readframes(nframes)
    samples = list(struct.unpack(f"<{nframes * ch}h", raw))
    if ch == 2:
        samples = [(samples[i] + samples[i + 1]) // 2 for i in range(0, len(samples), 2)]
    elif ch != 1:
        raise SystemExit(f"unsupported channels: {ch}")
    if width != 2:
        raise SystemExit("need 16-bit PCM")
    return rate, samples


def pitch_ratio(samples: list[int], ratio: float) -> list[float]:
    new_n = max(1, int(len(samples) / ratio))
    out: list[float] = []
    for i in range(new_n):
        pos = i * ratio
        i0 = int(pos)
        f = pos - i0
        if i0 + 1 < len(samples):
            out.append(samples[i0] * (1 - f) + samples[i0 + 1] * f)
        else:
            out.append(float(samples[min(i0, len(samples) - 1)]))
    return out


def stretch_to(samples: list[float], n: int) -> list[float]:
    if n <= 1 or len(samples) <= 1:
        return [samples[0] if samples else 0.0] * n
    out: list[float] = []
    for i in range(n):
        pos = i * (len(samples) - 1) / (n - 1)
        i0 = int(pos)
        f = pos - i0
        if i0 + 1 < len(samples):
            out.append(samples[i0] * (1 - f) + samples[i0 + 1] * f)
        else:
            out.append(samples[-1])
    return out


def main() -> int:
    rate, orig = read_mono(SRC)
    # +1 oktáva, potom natiahnuť na pôvodnú dĺžku (pitch up, duration preserved)
    hi = stretch_to(pitch_ratio(orig, 2.0), len(orig))
    # +čistá kvinta (~1.5×) – výplň stredu pásma malého repro
    mid = stretch_to(pitch_ratio(orig, 1.5), len(orig))

    mix: list[float] = []
    for a, b, c in zip(orig, mid, hi):
        # viac počuteľných vrstiev, menej nepočuteľného basu
        mix.append(0.22 * a + 0.38 * b + 0.55 * c)

    peak = max(abs(v) for v in mix) or 1.0
    scale = 0.78 * 32767.0 / peak
    out: list[int] = []
    for v in mix:
        x = (v * scale) / 32767.0
        if x > 1.0:
            x = 1.0
        if x < -1.0:
            x = -1.0
        # soft clip (menej plechový než hard clip)
        x = 1.5 * x - 0.5 * x * x * x
        out.append(int(max(-32767, min(32767, x * 32767.0 * 0.96))))

    fade = int(rate * 0.025)
    for i in range(fade):
        g = i / max(1, fade)
        out[i] = int(out[i] * g)
        out[-1 - i] = int(out[-1 - i] * g)

    with wave.open(str(OUT_WAV), "w") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(struct.pack(f"<{len(out)}h", *out))

    lines = [
        "#pragma once",
        f"// Auto: octave-double remaster of beep.wav for tiny speaker — {rate} Hz mono",
        "#include <stdint.h>",
        "",
        f"static constexpr int kBeepWavSampleRate = {rate};",
        f"static constexpr int kBeepWavSampleCount = {len(out)};",
        "static const int16_t kBeepWavPcm[kBeepWavSampleCount] = {",
    ]
    row: list[str] = []
    for i, s in enumerate(out):
        row.append(str(s))
        if len(row) == 16 or i == len(out) - 1:
            lines.append("    " + ", ".join(row) + ",")
            row = []
    lines.append("};")
    lines.append("")
    OUT_H.write_text("\n".join(lines), encoding="utf-8")

    peak_i = max(abs(v) for v in out)
    print(
        f"beep remaster: {len(out)} samples, dur={len(out)/rate:.2f}s, "
        f"peak={peak_i}/32767 ({100*peak_i/32767:.0f}% FS)"
    )
    print(f"wrote {OUT_WAV}")
    print(f"wrote {OUT_H}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
