"""Synthesises the NSX's V6 engine loops: engine_low_v6.wav (1800 rpm) and engine_high_v6.wav (5000 rpm).
Run:  python3 Scripts/Audio/make_v6.py     (plain Python, no packages; takes about a minute)
Scripts/setup_materials.py imports them as /Game/Audio/S_EngineLowV6 and S_EngineHighV6.

A four-stroke six fires three times per crank turn, so its note sits 1.5x above a four's at the same rpm
(90 Hz at 1800, 250 Hz at 5000). The sound is a sum of harmonics of the engine cycle (two turns):
  - every 6th harmonic is the firing note and its overtones (strong, smooth),
  - every 3rd adds the per-bank beat of a V engine (the growl),
  - the rest, quiet, are the small differences between cylinders (roughness),
shaped by an exhaust low-pass and intake resonances, plus hiss gated by the firing pulses.
Each loop is a whole number of engine cycles and every part repeats exactly over it, so it loops cleanly.
Level matches the four-cylinder loops (rms about 0.3, peak 0.7).
"""
import math, os, random, struct, wave

SR = 44100
HERE = os.path.dirname(os.path.abspath(__file__))


def envelope(f, rpm):
    """Spectral shape: exhaust low-pass plus intake resonances that open up with revs."""
    high = min(1.0, max(0.0, (rpm - 1500.0) / 4000.0))
    lowpass = 1.0 / (1.0 + (f / (420.0 + 500.0 * high)) ** 2)
    def bump(centre, width, gain):
        return gain * math.exp(-((f - centre) / width) ** 2)
    return lowpass + bump(850.0, 260.0, 0.10 + 0.22 * high) + bump(2300.0, 600.0, 0.02 + 0.07 * high)


def make(rpm, cycles, seed, name):
    rng = random.Random(seed)
    cycle_hz = rpm / 120.0
    n = int(round(cycles * SR / cycle_hz))
    step = 2.0 * math.pi * cycles / n          # phase of the engine cycle per sample
    out = [0.0] * n

    top = 4200.0 if rpm < 3000 else 6500.0
    harmonics = int(top / cycle_hz)
    for h in range(1, harmonics + 1):
        f = h * cycle_hz
        if h % 6 == 0: order = 1.0
        elif h % 3 == 0: order = 0.42
        else: order = 0.10
        amp = order * envelope(f, rpm) * rng.uniform(0.8, 1.2)
        if amp < 0.002: continue
        phase = rng.uniform(0.0, 2.0 * math.pi)
        # Slow wobble (whole number of times per loop) so it doesn't sound like a test tone.
        wob_n, wob_p, wob_a = rng.randint(1, 3), rng.uniform(0.0, 6.28), rng.uniform(0.03, 0.12)
        wob_step = 2.0 * math.pi * wob_n / n
        hs = h * step
        for i in range(n):
            out[i] += amp * (1.0 + wob_a * math.sin(wob_step * i + wob_p)) * math.sin(hs * i + phase)

    # Hiss: sinusoids on exact loop bins (so it repeats), gated by the firing pulses.
    hiss = [0.0] * n
    level = 0.012 if rpm < 3000 else 0.022
    for _ in range(110):
        k = rng.randint(int(1500.0 * n / SR), int(7000.0 * n / SR))
        ph = rng.uniform(0.0, 2.0 * math.pi)
        ks = 2.0 * math.pi * k / n
        a = level * rng.uniform(0.5, 1.0)
        for i in range(n):
            hiss[i] += a * math.sin(ks * i + ph)
    fire = 6.0 * step
    for i in range(n):
        gate = 0.5 + 0.5 * math.sin(fire * i)
        out[i] += hiss[i] * (0.35 + 0.65 * gate * gate)

    # Gentle saturation (an engine is not a clean oscillator), then level to match the four-cylinder loops.
    peak = max(abs(v) for v in out)
    out = [math.tanh(1.6 * v / peak) for v in out]
    rms = math.sqrt(sum(v * v for v in out) / n)
    gain = 0.30 / rms
    peak = max(abs(v) for v in out) * gain
    if peak > 0.72: gain *= 0.72 / peak
    with wave.open(os.path.join(HERE, name), "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
        w.writeframes(struct.pack("<%dh" % n, *[int(max(-1.0, min(1.0, v * gain)) * 32767) for v in out]))
    print("%s: %.3f s, %d cycles, firing note %.0f Hz" % (name, n / SR, cycles, rpm / 20.0))


if __name__ == "__main__":
    make(1800.0, 12, 61, "engine_low_v6.wav")    # 0.8 s
    make(5000.0, 60, 62, "engine_high_v6.wav")   # 1.44 s
