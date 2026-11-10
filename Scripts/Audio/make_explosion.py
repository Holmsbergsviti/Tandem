"""Synthesises explosion.wav: the engine blowing up (also played quietly, pitched up, when a part is torn off).
Run:  python3 Scripts/Audio/make_explosion.py     (plain Python, no packages)
Scripts/setup_materials.py imports it as /Game/Audio/S_Explosion (not looping).

A sharp crack (wide-band noise, a few ms), a body of noise whose brightness falls quickly (the fireball),
and a low thump sliding from 90 to 35 Hz (the pressure wave), all dying away over about 1.6 s.
"""
import math, os, random, struct, wave

SR = 44100
N = int(SR * 1.8)
rng = random.Random(77)
out = []
low = 0.0      # one-pole low-pass state
phase = 0.0
for i in range(N):
    t = i / SR
    white = rng.uniform(-1.0, 1.0)
    cutoff = 300.0 + 9000.0 * math.exp(-t / 0.12)            # bright at first, then a rumble
    a = 1.0 - math.exp(-2.0 * math.pi * cutoff / SR)
    low += a * (white - low)
    body = low * math.exp(-t / 0.45) * 1.6
    crack = white * math.exp(-t / 0.006) * 0.9
    phase += 2.0 * math.pi * (35.0 + 55.0 * math.exp(-t / 0.15)) / SR
    thump = math.sin(phase) * math.exp(-t / 0.35) * 0.9
    fade = min(1.0, (N - i) / (0.1 * SR))                    # no click at the end
    out.append(math.tanh(1.4 * (body + crack + thump)) * fade)
peak = max(abs(v) for v in out)
with wave.open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "explosion.wav"), "wb") as w:
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
    w.writeframes(struct.pack("<%dh" % N, *[int(v / peak * 0.9 * 32767) for v in out]))
print("explosion.wav: %.2f s" % (N / SR))
