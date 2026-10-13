#!/usr/bin/env python3
"""Synthesises the framework's soft feedback sounds as small WAV files.

Everything is generated from sine partials and gently filtered noise, so the
sounds are original works released with the project (no third-party samples).

    python3 tools/make_sounds.py framework/qt/sounds
"""

import math
import random
import struct
import sys
import wave
from pathlib import Path

RATE = 44100


def envelope(t, attack, decay):
    """Fast linear attack, exponential decay."""
    if t < attack:
        return t / attack
    return math.exp(-(t - attack) / decay)


def render(duration, voice):
    n = int(duration * RATE)
    samples = [voice(i / RATE) for i in range(n)]
    # Short fade-out so nothing clicks at the end.
    fade = int(0.01 * RATE)
    for i in range(fade):
        samples[n - 1 - i] *= i / fade
    return samples


def lowpass_noise(seed, cutoff):
    rng = random.Random(seed)
    alpha = 1 - math.exp(-2 * math.pi * cutoff / RATE)
    state = 0.0
    cache = {}

    def noise(t):
        nonlocal state
        i = int(t * RATE)
        if i not in cache:
            state += alpha * (rng.uniform(-1, 1) - state)
            cache[i] = state
        return cache[i]

    return noise


def write(path, samples, gain=1.0):
    peak = max(1e-9, max(abs(s) for s in samples))
    scale = gain * 0.9 / peak
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1, min(1, s * scale)) * 32767)) for s in samples))


def tap(freq, seed):
    """A soft wooden knock for placing a digit or card."""
    noise = lowpass_noise(seed, 2500)

    def voice(t):
        body = math.sin(2 * math.pi * freq * t) + 0.35 * math.sin(2 * math.pi * freq * 2.3 * t)
        return envelope(t, 0.002, 0.03) * body * 0.8 + envelope(t, 0.001, 0.006) * noise(t) * 0.6

    return render(0.18, voice)


def soft(freq, seed):
    """A light paper tick for pencil notes."""
    noise = lowpass_noise(seed, 5000)

    def voice(t):
        return envelope(t, 0.001, 0.012) * (0.5 * math.sin(2 * math.pi * freq * t) + noise(t))

    return render(0.1, voice)


def thump(freq, decay):
    """A low, muted thud for erase and gentle mistakes."""

    def voice(t):
        return envelope(t, 0.004, decay) * math.sin(2 * math.pi * freq * t * (1 - 0.15 * t))

    return render(decay * 6, voice)


def bell(freq, duration=1.6):
    """A soft chime with bell-like partials."""
    partials = [(1.0, 1.0, 0.7), (2.0, 0.35, 0.45), (2.76, 0.18, 0.3), (5.4, 0.06, 0.15)]

    def voice(t):
        return sum(
            amp * envelope(t, 0.006, duration * life) * math.sin(2 * math.pi * freq * ratio * t)
            for ratio, amp, life in partials
        )

    return render(duration, voice)


def chord(freqs, spread=0.09, duration=2.8):
    """A resolved arpeggiated chord for winning a game."""
    voices = [(i * spread, bell(f, duration - i * spread)) for i, f in enumerate(freqs)]
    n = int(duration * RATE)
    out = [0.0] * n
    for offset, samples in voices:
        start = int(offset * RATE)
        for i, s in enumerate(samples):
            if start + i < n:
                out[start + i] += s
    return out


def main():
    out = Path(sys.argv[1] if len(sys.argv) > 1 else "framework/qt/sounds")
    out.mkdir(parents=True, exist_ok=True)

    for i, f in enumerate([470, 500, 530]):
        write(out / f"tap_{i + 1}.wav", tap(f, i), 0.55)
    for i, f in enumerate([1500, 1600, 1700]):
        write(out / f"soft_{i + 1}.wav", soft(f, 10 + i), 0.3)
    write(out / "erase.wav", thump(240, 0.035), 0.4)
    write(out / "error.wav", thump(170, 0.05), 0.3)

    # Major pentatonic from C5 upward: each completion chimes one step higher.
    pentatonic = [523.25, 587.33, 659.25, 783.99, 880.00, 1046.50, 1174.66, 1318.51]
    for i, f in enumerate(pentatonic):
        write(out / f"complete_{i}.wav", bell(f), 0.4)

    write(out / "win.wav", chord([523.25, 659.25, 783.99, 1046.50]), 0.5)


if __name__ == "__main__":
    main()
