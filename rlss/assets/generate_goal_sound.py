#!/usr/bin/env python3
"""Synthesizes the goal explosion sound, rlss/assets/sounds/goal_explosion.wav (NumPy, deterministic).

A sharp crack, a low boom sweeping down and a rumble of filtered noise that dies out:

    python3 rlss/assets/generate_goal_sound.py
"""

from __future__ import annotations

import wave
from pathlib import Path

import numpy as np

RATE = 44100
DURATION = 2.2
OUT = Path(__file__).resolve().parent / "sounds" / "goal_explosion.wav"


def low_pass(signal: np.ndarray, cutoff: np.ndarray) -> np.ndarray:
    """One-pole low-pass filter with a cutoff (Hz) that may change over time."""
    alpha = 1.0 - np.exp(-2.0 * np.pi * cutoff / RATE)
    out = np.empty_like(signal)
    state = 0.0
    for i, (sample, a) in enumerate(zip(signal, alpha)):
        state += a * (sample - state)
        out[i] = state
    return out


def main() -> None:
    rng = np.random.default_rng(5)
    t = np.arange(int(RATE * DURATION)) / RATE
    # Boom: a sine sweeping from 110 Hz down to 32 Hz.
    frequency = 32.0 + 78.0 * np.exp(-t * 6.0)
    phase = 2.0 * np.pi * np.cumsum(frequency) / RATE
    boom = np.sin(phase) * np.exp(-t * 2.6) * (1.0 - np.exp(-t * 400.0))
    # Rumble: noise, darker and quieter as it fades.
    noise = rng.uniform(-1.0, 1.0, t.size)
    rumble = low_pass(noise, 120.0 + 3500.0 * np.exp(-t * 7.0)) * np.exp(-t * 2.0) * 2.4
    # Crack: a bright burst in the first few milliseconds, and crackles after it.
    crack = noise * np.exp(-t * 60.0) * 0.9
    crackles = noise * (rng.random(t.size) > 0.9985) * np.exp(-t * 3.5) * 1.5
    signal = 0.9 * boom + rumble + crack + crackles
    signal = np.tanh(signal * 1.4)  # soft saturation, punchier
    signal = signal / np.max(np.abs(signal)) * 0.92
    signal[-2000:] *= np.linspace(1.0, 0.0, 2000)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(OUT), "wb") as file:
        file.setnchannels(1)
        file.setsampwidth(2)
        file.setframerate(RATE)
        file.writeframes((signal * 32767).astype("<i2").tobytes())
    print(f"wrote {OUT}")


if __name__ == "__main__":
    main()
