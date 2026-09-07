"""Generate the small, deterministic Outpost 2D sound set.

This file intentionally uses only Python's standard library.  It is run with
the regular project Python (or Unreal's embedded Python) before
``Tools/import_assets.py`` imports the resulting WAV files.
"""

from __future__ import annotations

import math
import random
import struct
import wave
from pathlib import Path


SAMPLE_RATE = 22_050
AMPLITUDE = 0.78


def _envelope(local_time: float, duration: float, attack: float = 0.006, release: float = 0.06) -> float:
    """Return a soft attack/release envelope for a time inside a voice."""
    if local_time < 0.0 or local_time >= duration:
        return 0.0
    attack_level = 1.0 if attack <= 0.0 else min(1.0, local_time / attack)
    release_start = max(0.0, duration - release)
    release_level = 1.0 if local_time <= release_start else max(0.0, (duration - local_time) / release)
    return attack_level * release_level


def _tone(
    time: float,
    frequency: float,
    duration: float,
    start: float = 0.0,
    amplitude: float = 0.2,
    slide: float = 0.0,
    harmonic: float = 0.0,
) -> float:
    local = time - start
    envelope = _envelope(local, duration)
    if envelope == 0.0:
        return 0.0
    phase = 2.0 * math.pi * (frequency * local + 0.5 * slide * local * local / duration)
    value = math.sin(phase)
    if harmonic:
        value = (1.0 - harmonic) * value + harmonic * math.sin(phase * 2.01) * 0.5
    return amplitude * envelope * value


def _noise(time: float, duration: float, start: float, amplitude: float, rng: random.Random) -> float:
    local = time - start
    envelope = _envelope(local, duration, attack=0.001, release=min(0.08, duration * 0.7))
    if envelope == 0.0:
        return 0.0
    return amplitude * envelope * (rng.random() * 2.0 - 1.0)


def _render(duration: float, voice, seed: int) -> list[float]:
    rng = random.Random(seed)
    count = max(1, int(round(duration * SAMPLE_RATE)))
    samples = [voice(index / SAMPLE_RATE, rng) for index in range(count)]
    peak = max((abs(value) for value in samples), default=1.0)
    if peak > 1.0:
        scale = 1.0 / peak
        samples = [value * scale for value in samples]
    return samples


def _write_wav(path: Path, samples: list[float]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    frames = b"".join(struct.pack("<h", int(max(-1.0, min(1.0, value)) * 32767.0)) for value in samples)
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(SAMPLE_RATE)
        output.writeframes(frames)


def _shot(t: float, rng: random.Random) -> float:
    return _tone(t, 920.0, 0.15, amplitude=0.20, slide=-420.0, harmonic=0.15) + _noise(t, 0.075, 0.0, 0.10, rng)


def _mine(t: float, rng: random.Random) -> float:
    return _tone(t, 92.0, 0.42, amplitude=0.29, slide=-38.0, harmonic=0.30) + _tone(t, 180.0, 0.18, 0.025, 0.09, -70.0) + _noise(t, 0.20, 0.01, 0.075, rng)


def _forge(t: float, rng: random.Random) -> float:
    strikes = sum(_tone(t, frequency, 0.075, start, 0.18, -frequency * 0.35, 0.25) for start, frequency in ((0.0, 840.0), (0.095, 1_040.0), (0.19, 1_280.0)))
    return strikes + _noise(t, 0.045, 0.0, 0.045, rng) + _noise(t, 0.045, 0.095, 0.035, rng) + _noise(t, 0.045, 0.19, 0.028, rng)


def _upgrade(t: float, rng: random.Random) -> float:
    notes = ((0.0, 440.0), (0.105, 554.37), (0.21, 659.25), (0.315, 880.0))
    return sum(_tone(t, frequency, 0.16, start, 0.13, harmonic=0.15) for start, frequency in notes)


def _lift(t: float, rng: random.Random) -> float:
    return _tone(t, 210.0, 0.29, amplitude=0.15, slide=440.0, harmonic=0.2) + _tone(t, 420.0, 0.22, 0.07, 0.05, 160.0)


def _place(t: float, rng: random.Random) -> float:
    return _tone(t, 330.0, 0.13, amplitude=0.16) + _tone(t, 495.0, 0.14, 0.055, amplitude=0.10)


def _hit(t: float, rng: random.Random) -> float:
    return _tone(t, 150.0, 0.13, amplitude=0.18, slide=-80.0, harmonic=0.25) + _noise(t, 0.035, 0.0, 0.085, rng)


def _hurt(t: float, rng: random.Random) -> float:
    return _tone(t, 245.0, 0.25, amplitude=0.14, slide=-135.0, harmonic=0.35) + _noise(t, 0.12, 0.0, 0.035, rng)


def _wave(t: float, rng: random.Random) -> float:
    return _tone(t, 440.0, 0.21, amplitude=0.13) + _tone(t, 660.0, 0.21, 0.13, 0.09, 0.0) + _tone(t, 440.0, 0.21, 0.27, 0.13)


def _win(t: float, rng: random.Random) -> float:
    notes = ((0.0, 523.25), (0.14, 659.25), (0.28, 783.99), (0.42, 1_046.5))
    return sum(_tone(t, frequency, 0.26, start, 0.13, harmonic=0.18) for start, frequency in notes)


def _lose(t: float, rng: random.Random) -> float:
    notes = ((0.0, 392.0), (0.19, 329.63), (0.38, 261.63))
    return sum(_tone(t, frequency, 0.28, start, 0.12, harmonic=0.2) for start, frequency in notes)


SOUNDS = (
    ("S_shot.wav", 0.16, _shot),
    ("S_mine.wav", 0.46, _mine),
    ("S_forge.wav", 0.31, _forge),
    ("S_upgrade.wav", 0.50, _upgrade),
    ("S_lift.wav", 0.34, _lift),
    ("S_place.wav", 0.22, _place),
    ("S_hit.wav", 0.16, _hit),
    ("S_hurt.wav", 0.29, _hurt),
    ("S_wave.wav", 0.56, _wave),
    ("S_win.wav", 0.72, _win),
    ("S_lose.wav", 0.68, _lose),
)


def main() -> None:
    output_dir = Path(__file__).resolve().parents[1] / "SourceArt" / "sounds"
    output_dir.mkdir(parents=True, exist_ok=True)
    for index, (filename, duration, voice) in enumerate(SOUNDS):
        samples = _render(duration, voice, seed=10_000 + index)
        _write_wav(output_dir / filename, [AMPLITUDE * value for value in samples])
        print(f"Generated {output_dir / filename} ({duration:.2f}s, {SAMPLE_RATE} Hz mono PCM16)")


if __name__ == "__main__":
    main()
