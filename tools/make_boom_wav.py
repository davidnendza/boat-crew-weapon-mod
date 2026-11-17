"""Writes sounds/boom.wav: the Longer Tom's firing boom, played by the mod on top of the stock gun sound for guns with
fx kick. Synthesised (no samples): a sharp noise crack, a deep thump falling from 70 to 28 Hz, a low rumble tail and a
soft-clipped peak. 44.1 kHz mono 16-bit, about 3 s.

Usage: python make_boom_wav.py <out.wav>"""
import sys
import wave

import numpy as np

SR, DUR = 44100, 3.0
t = np.arange(int(SR * DUR)) / SR
rng = np.random.default_rng(155)


def lowpass(x, cutoff):
    a = np.exp(-2 * np.pi * cutoff / SR)
    y = np.empty_like(x); acc = 0.0
    for i, v in enumerate(x):
        acc = (1 - a) * v + a * acc; y[i] = acc
    return y


crack = rng.standard_normal(t.size) * np.exp(-t / 0.012)
crack -= lowpass(crack, 900)                                          # keep the crack's top end
freq = 28 + 42 * np.exp(-t / 0.18)                                    # 70 Hz falling to 28 Hz
thump = np.sin(2 * np.pi * np.cumsum(freq) / SR) * np.exp(-t / 0.55)
body = lowpass(rng.standard_normal(t.size), 220) * np.exp(-t / 0.35) * 6
rumble = lowpass(lowpass(rng.standard_normal(t.size), 70), 70) * np.exp(-t / 1.1) * 25
attack = np.clip(t / 0.003, 0, 1)
mix = attack * (0.9 * crack + 1.0 * thump + 0.7 * body + 0.8 * rumble)
mix = np.tanh(2.2 * mix / np.abs(mix).max())                           # soft clip: loud and dense
mix *= np.clip((DUR - t) / 0.3, 0, 1)                                 # fade the last 0.3 s
pcm = (mix / np.abs(mix).max() * 0.97 * 32767).astype('<i2')
with wave.open(sys.argv[1], 'wb') as w:
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes(pcm.tobytes())
print('wrote', sys.argv[1], pcm.size / SR, 's')
