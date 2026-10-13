#!/usr/bin/env python3
"""Synthesises the framework's soft feedback sounds as small WAV files.

Everything is generated from sine partials and gently filtered noise, so the
sounds are original works released with the project (no third-party samples).
Every voice starts with a rounded attack (no click), is low-passed, and gets a
short, quiet room so it sits softly rather than snapping.

    python3 tools/make_sounds.py framework/qt/sounds             # every theme
    python3 tools/make_sounds.py out/ --theme kalimba            # one theme

Each theme is written to its own folder (sounds/water/tap_1.wav, ...).
"""

import argparse
import math
import random
import struct
import wave
from pathlib import Path

RATE = 24000

# Major pentatonic steps used for completion chimes and the win arpeggio.
PENTATONIC = [1, 9 / 8, 5 / 4, 3 / 2, 5 / 3, 2, 9 / 4, 5 / 2]


def envelope(t, attack, decay):
    """Raised-cosine attack (no click), then exponential decay."""
    if t < attack:
        return 0.5 - 0.5 * math.cos(math.pi * t / attack)
    return math.exp(-(t - attack) / decay)


def render(duration, voice):
    n = int(duration * RATE)
    return [voice(i / RATE) for i in range(n)]


def lowpass(samples, cutoff):
    alpha = 1 - math.exp(-2 * math.pi * cutoff / RATE)
    out, state = [], 0.0
    for s in samples:
        state += alpha * (s - state)
        out.append(state)
    return out


def noise(seed, n):
    rng = random.Random(seed)
    return [rng.uniform(-1, 1) for _ in range(n)]


def room(samples, wet=0.18, tail=0.6):
    """A small, soft room: four damped combs into an all-pass, mixed in quietly."""
    n = len(samples) + int(tail * RATE)
    dry = samples + [0.0] * (n - len(samples))
    wet_sum = [0.0] * n
    for delay_ms, feedback in [(29.7, 0.72), (37.1, 0.70), (41.1, 0.68), (43.7, 0.66)]:
        d = int(delay_ms * RATE / 1000)
        buf = [0.0] * n
        damp = 0.0
        for i in range(n):
            back = buf[i - d] if i >= d else 0.0
            damp += 0.35 * (back - damp)
            buf[i] = dry[i] + feedback * damp
            wet_sum[i] += buf[i] * 0.25
    d, g = int(5.0 * RATE / 1000), 0.5
    allpass = [0.0] * n
    for i in range(n):
        x_d = wet_sum[i - d] if i >= d else 0.0
        y_d = allpass[i - d] if i >= d else 0.0
        allpass[i] = -g * wet_sum[i] + x_d + g * y_d
    out = [dry[i] + wet * allpass[i] for i in range(n)]
    fade = int(0.08 * RATE)
    for i in range(fade):
        out[n - 1 - i] *= i / fade
    return out


def mix(parts, duration):
    n = int(duration * RATE)
    out = [0.0] * n
    for offset, samples in parts:
        start = int(offset * RATE)
        for i, s in enumerate(samples):
            if start + i < n:
                out[start + i] += s
    return out


def trim_tail(samples, floor_db=-54, fade=0.03):
    """Drops the inaudible end of a reverb tail (everything after the signal last
    rises above `floor_db` relative to its peak), with a short fade. Keeps the
    files, and so the web download, small without changing what is heard."""
    peak = max(1e-9, max(abs(s) for s in samples))
    threshold = peak * 10 ** (floor_db / 20)
    last = max((i for i, s in enumerate(samples) if abs(s) > threshold), default=len(samples) - 1)
    end = min(len(samples), last + 1 + int(fade * RATE))
    out = samples[:end]
    n = min(int(fade * RATE), len(out))
    for i in range(n):
        out[len(out) - 1 - i] *= i / n
    return out


def write(path, samples, gain):
    samples = trim_tail(list(samples))
    peak = max(1e-9, max(abs(s) for s in samples))
    scale = gain * 0.9 / peak
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1, min(1, s * scale)) * 32767)) for s in samples))


# --- Voices -----------------------------------------------------------------
# Each set supplies a "note" voice (freq, length) -> samples. Taps, chimes and
# the win arpeggio are that voice at different pitches and lengths.


def felt_note(freq, length):
    """A felt-mallet marimba: warm fundamental, a quiet fast-fading 4th partial."""

    def voice(t):
        return (envelope(t, 0.007, length * 0.35) * math.sin(2 * math.pi * freq * t)
                + 0.12 * envelope(t, 0.005, 0.05) * math.sin(2 * math.pi * freq * 3.93 * t)
                + 0.05 * envelope(t, 0.004, 0.03) * math.sin(2 * math.pi * freq * 9.2 * t))

    return lowpass(render(length, voice), 3200)


def kalimba_note(freq, length):
    """A thumb-piano pluck: round tone with a soft inharmonic shimmer."""

    def voice(t):
        return (envelope(t, 0.005, length * 0.3) * math.sin(2 * math.pi * freq * t)
                + 0.22 * envelope(t, 0.004, length * 0.12) * math.sin(2 * math.pi * freq * 5.95 * t)
                + 0.08 * envelope(t, 0.004, 0.04) * math.sin(2 * math.pi * freq * 2.01 * t))

    return lowpass(render(length, voice), 4200)


def driftwood_note(freq, length):
    """A hollow wooden block, softened: a falling sine with a brushed edge."""
    grain = lowpass(noise(int(freq), int(length * RATE)), 1400)

    def voice(t):
        i = min(int(t * RATE), len(grain) - 1)
        body = math.sin(2 * math.pi * freq * t * (1 - 0.06 * min(t, 0.5)))
        return envelope(t, 0.006, length * 0.22) * body + 0.35 * envelope(t, 0.004, 0.012) * grain[i]

    return lowpass(render(length, voice), 2600)


def water_note(freq, length):
    """A droplet into still water: a sine whose pitch rises as the bubble closes,
    over a quiet resonant tone at the note's pitch."""
    rise = 0.035

    def voice(t):
        glide = freq * 0.7 * (1 + 0.9 * min(t, rise) / rise)
        phase = 2 * math.pi * freq * 0.7 * (t + 0.9 * min(t, rise) ** 2 / (2 * rise) + 0.9 * max(0.0, t - rise))
        drop = envelope(t, 0.004, 0.045) * math.sin(phase)
        pool = 0.35 * envelope(t, 0.02, length * 0.3) * math.sin(2 * math.pi * glide * t)
        return drop + pool

    return lowpass(render(length, voice), 3000)


def chime_note(freq, length):
    """A small tubular wind chime: the inharmonic partials of a free bar,
    struck softly, with a slow shimmer between two close fundamentals."""
    partials = [(1.0, 1.0, 1.0), (2.76, 0.32, 0.45), (5.40, 0.1, 0.22)]

    def voice(t):
        beat = 0.5 * (math.sin(2 * math.pi * freq * t) + math.sin(2 * math.pi * (freq + 1.7) * t))
        rest = sum(amp * envelope(t, 0.004, length * life * 0.4) * math.sin(2 * math.pi * freq * ratio * t)
                   for ratio, amp, life in partials[1:])
        return envelope(t, 0.005, length * 0.4) * beat + rest

    return lowpass(render(length, voice), 6000)


# Theme id -> its voice, tap pitches (Hz), chime and win root (Hz), room level.
# The app lists themes in this order.
SETS = {
    "water": dict(voice=water_note, taps=[660.0, 700.0, 740.0], root=523.25, wet=0.22,
                  win=[(0, 1), (0.17, 3 / 2), (0.29, 5 / 4), (0.48, 2), (0.61, 5 / 3), (0.86, 5 / 2)]),
    "chimes": dict(voice=chime_note, taps=[880.0, 987.8, 1046.5], root=659.25, wet=0.2,
                   win=[(0, 3 / 2), (0.21, 1), (0.33, 2), (0.62, 5 / 4), (0.8, 5 / 3), (1.15, 9 / 4)]),
    "felt": dict(voice=felt_note, taps=[392.0, 415.3, 440.0], root=523.25, wet=0.16),
    "kalimba": dict(voice=kalimba_note, taps=[523.25, 554.37, 587.33], root=587.33, wet=0.2),
    "driftwood": dict(voice=driftwood_note, taps=[262.0, 277.2, 294.0], root=392.0, wet=0.24),
}


def brush(seed, length, cutoff, attack, decay):
    """Filtered noise with a slow attack: a soft brush or paper sound."""
    grain = lowpass(lowpass(lowpass(noise(seed, int(length * RATE)), cutoff), cutoff), cutoff)
    return [envelope(i / RATE, attack, decay) * s for i, s in enumerate(grain)]


def build(name, out):
    """Writes one theme's 17 sounds into `out`."""
    spec = SETS[name]
    note, wet = spec["voice"], spec["wet"]
    out.mkdir(parents=True, exist_ok=True)

    for i, f in enumerate(spec["taps"]):
        write(out / f"tap_{i + 1}.wav", room(note(f, 0.32), wet, 0.35), 0.42)
    for i in range(3):
        # Pencil notes: a very quiet paper touch with a hint of the set's tone.
        paper = brush(20 + i, 0.12, 1500 + 200 * i, 0.008, 0.022)
        tone = note(spec["taps"][i] * 2, 0.12)
        write(out / f"soft_{i + 1}.wav", room([p + 0.25 * s for p, s in zip(paper, tone)], wet, 0.25), 0.2)
    write(out / "erase.wav", room(brush(7, 0.22, 900, 0.02, 0.05), wet, 0.3), 0.22)
    # Mistakes: a low, slow-blooming note, never a buzz.
    low = spec["taps"][0] / 2
    write(out / "error.wav", room(mix([(0, note(low, 0.5)), (0.11, note(low * 0.84, 0.5))], 0.62), wet, 0.4), 0.2)

    for i, ratio in enumerate(PENTATONIC):
        write(out / f"complete_{i}.wav", room(note(spec["root"] * ratio, 1.2), wet + 0.06, 0.8), 0.32)

    # A rising arpeggio, or for chimes and water a looser, wind- or rain-like scatter.
    arpeggio = spec.get("win", [(i * 0.14, r) for i, r in enumerate([1, 5 / 4, 3 / 2, 2, 5 / 2])])
    parts = [(at, note(spec["root"] * r, 2.4 - at)) for at, r in arpeggio]
    write(out / "win.wav", room(mix(parts, 2.6), wet + 0.1, 1.2), 0.4)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("out", nargs="?", default="framework/qt/sounds")
    parser.add_argument("--theme", choices=list(SETS), help="build only this theme")
    args = parser.parse_args()
    for name in [args.theme] if args.theme else SETS:
        build(name, Path(args.out) / name)


if __name__ == "__main__":
    main()
