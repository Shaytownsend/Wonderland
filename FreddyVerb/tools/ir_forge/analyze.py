#!/usr/bin/env python3
"""ir_forge analysis: objective checks on rendered IRs.

usage: analyze.py [files or directory ...] [--plots DIR] [--library Resources/irs/library.json]

Checks per IR (4-channel true stereo L->L, L->R, R->L, R->R):
  * RT60 (T30, Schroeder backward integration of the 4-channel energy sum) per octave band 125 Hz..8 kHz
    and broadband; compared with library.json's measured value (1 kHz octave)
  * normalised echo density profile (Abel & Huang, 20 ms Hann window): time to reach 0.9 / 1.0
  * DC offset, clipping (|x| >= 0.999), peak dBFS, last-sample silence
  * decay smoothness: RMS deviation (dB) of the Schroeder curve from its linear fit over -5..-35 dB, and the
    max ripple of a 5 ms-smoothed envelope around a 50 ms-smoothed one in the late tail (flutter detector)
  * late-tail inter-channel correlation (|rho| between LL/LR and LL/RR, 200 ms after onset .. -40 dB)
  * spectrum: spectral centroid, energy share below 100 Hz and above 10 kHz
"""
import json
import os
import sys

import numpy as np
from scipy import signal
from scipy.special import erfc

try:
    import soundfile as sf
except ImportError:  # pragma: no cover
    sf = None


def load(path):
    if sf is None:
        raise SystemExit("pip install soundfile")
    x, fs = sf.read(path, dtype="float64", always_2d=True)
    return x.T, fs


def edc_db(x):
    e = np.sum(x ** 2, axis=0)[::-1].cumsum()[::-1]
    e = e / max(e[0], 1e-300)
    return 10 * np.log10(np.maximum(e, 1e-30))


def fit_t(edc, fs, hi, lo):
    idx = np.where((edc <= hi) & (edc >= lo))[0]
    if len(idx) < 16 or edc.min() > lo:
        return float("nan"), float("nan")
    t = idx / fs
    p = np.polyfit(t, edc[idx], 1)
    resid = edc[idx] - np.polyval(p, t)
    return -60.0 / p[0] if p[0] < 0 else float("nan"), float(np.sqrt(np.mean(resid ** 2)))


def octave(x, fs, fc):
    lo, hi = fc / np.sqrt(2), min(fc * np.sqrt(2), fs * 0.45)
    sos = signal.butter(3, [lo, hi], btype="band", fs=fs, output="sos")
    return signal.sosfilt(sos, x, axis=1)


def echo_density(h, fs, win_ms=20.0):
    n = int(win_ms * 1e-3 * fs) | 1
    w = np.hanning(n)
    w /= w.sum()
    half = n // 2
    hp = np.pad(h, (half, half))
    # local weighted std via convolution
    m2 = np.convolve(hp ** 2, w, mode="valid")
    sd = np.sqrt(np.maximum(m2, 1e-30))
    # weighted fraction of samples outside 1 std (approx: per-sample indicator vs local std)
    ind = (np.abs(h) > sd).astype(float)
    frac = np.convolve(np.pad(ind, (half, half)), w, mode="valid")
    return frac / erfc(1 / np.sqrt(2))


def analyze(path, plots=None):
    x, fs = load(path)
    r = {"file": os.path.basename(path), "len": x.shape[1] / fs}
    r["peak_db"] = 20 * np.log10(np.max(np.abs(x)) + 1e-30)
    r["clip"] = bool(np.max(np.abs(x)) >= 0.999)
    r["dc_db"] = 20 * np.log10(np.max(np.abs(x.mean(axis=1))) / (np.max(np.abs(x)) + 1e-30) + 1e-30)
    r["last_zero"] = bool(np.all(x[:, -1] == 0))

    edc = edc_db(x)
    r["t30_bb"], r["edc_rms"] = fit_t(edc, fs, -5, -35)
    bands = [125, 250, 500, 1000, 2000, 4000, 8000]
    r["t30"] = {}
    for fc in bands:
        r["t30"][fc] = fit_t(edc_db(octave(x, fs, fc)), fs, -5, -35)[0]

    # onset
    env_all = np.sqrt(np.sum(x ** 2, axis=0))
    onset = int(np.argmax(env_all > env_all.max() * 10 ** (-30 / 20)))
    r["onset_ms"] = onset / fs * 1000

    # echo density on LL (and averaged over channels)
    ned = np.mean([echo_density(x[c], fs) for c in range(4)], axis=0)
    seg = ned[onset:]
    def first_cross(th):
        i = np.where(seg >= th)[0]
        return i[0] / fs * 1000 if len(i) else float("nan")
    r["ned90_ms"] = first_cross(0.9)
    r["ned_late"] = float(np.median(ned[onset + int(0.1 * fs): onset + int(0.4 * fs)])) if x.shape[1] > onset + int(0.4 * fs) else float(np.median(seg))

    # flutter / envelope ripple in the late tail: 5 ms vs 50 ms smoothed envelope (dB)
    p = env_all ** 2
    def smooth(v, ms):
        n = max(1, int(ms * 1e-3 * fs))
        return np.convolve(v, np.ones(n) / n, mode="same")
    e5 = 10 * np.log10(smooth(p, 5) + 1e-30)
    e50 = 10 * np.log10(smooth(p, 50) + 1e-30)
    i_start = onset + int(0.08 * fs)
    below = np.where(edc < -40)[0]
    i_end = below[0] if len(below) else len(edc)
    if i_end - i_start > int(0.05 * fs):
        d = (e5 - e50)[i_start:i_end]
        r["ripple_db"] = float(np.std(d))
    else:
        r["ripple_db"] = float("nan")

    # late-tail correlation
    b = i_end
    a = onset + min(int(0.2 * fs), (b - onset) // 3)   # late part: 200 ms in, or the last 2/3 for short IRs
    if b - a > int(0.03 * fs):
        def corr(u, v):
            return float(np.dot(u, v) / np.sqrt(np.dot(u, u) * np.dot(v, v) + 1e-30))
        r["rho_LL_LR"] = corr(x[0, a:b], x[1, a:b])
        r["rho_LL_RR"] = corr(x[0, a:b], x[3, a:b])
    else:
        r["rho_LL_LR"] = r["rho_LL_RR"] = float("nan")

    # spectrum
    f, P = signal.welch(x, fs=fs, nperseg=8192, axis=1)
    Ps = P.sum(axis=0)
    r["centroid"] = float(np.sum(f * Ps) / np.sum(Ps))
    r["lo_share"] = float(Ps[f < 100].sum() / Ps.sum())
    r["hi_share"] = float(Ps[f > 10000].sum() / Ps.sum())

    if plots:
        try:
            import matplotlib
            matplotlib.use("Agg")
            import matplotlib.pyplot as plt
            fig, ax = plt.subplots(3, 1, figsize=(10, 10))
            t = np.arange(x.shape[1]) / fs
            ax[0].plot(t, e5 - e5.max(), lw=0.5, label="env 5ms")
            ax[0].plot(t, edc, label="EDC")
            ax[0].set_ylim(-100, 3); ax[0].legend(); ax[0].set_title(r["file"])
            ax[1].specgram(x[0], NFFT=2048, Fs=fs, noverlap=1536, cmap="magma", vmin=-160)
            ax[1].set_ylim(0, 20000)
            ax[2].plot(t, ned, lw=0.5); ax[2].set_xlim(0, min(t[-1], 0.5)); ax[2].set_ylabel("NED")
            fig.tight_layout()
            fig.savefig(os.path.join(plots, r["file"] + ".png"), dpi=80)
            plt.close(fig)
        except ImportError:
            pass
    return r


def main():
    args = sys.argv[1:]
    plots = None
    lib = None
    if "--plots" in args:
        i = args.index("--plots"); plots = args[i + 1]; del args[i:i + 2]
        os.makedirs(plots, exist_ok=True)
    if "--library" in args:
        i = args.index("--library"); lib = args[i + 1]; del args[i:i + 2]
    if not args:
        args = [os.path.join(os.path.dirname(__file__), "..", "..", "Resources", "irs")]
    files = []
    for a in args:
        if os.path.isdir(a):
            files += sorted(os.path.join(a, f) for f in os.listdir(a) if f.endswith((".flac", ".wav")))
            if lib is None and os.path.exists(os.path.join(a, "library.json")):
                lib = os.path.join(a, "library.json")
        else:
            files.append(a)
    meta = {}
    if lib:
        for e in json.load(open(lib)):
            meta[e["file"]] = e

    hdr = ("file", "len", "rt60js", "T30bb", "125", "250", "500", "1k", "2k", "4k", "8k", "NED90", "NEDlt",
           "edcRMS", "rip", "rLR", "rRR", "cent", "<100", ">10k", "pk", "dc", "0end")
    print(("{:<26}" + "{:>6}" * (len(hdr) - 1)).format(*hdr))
    bad = 0
    for p in files:
        r = analyze(p, plots)
        js = meta.get(r["file"], {}).get("rt60", float("nan"))
        row = [r["file"][:26], f"{r['len']:.2f}", f"{js:.2f}", f"{r['t30_bb']:.2f}"]
        row += [f"{r['t30'][b]:.2f}" for b in [125, 250, 500, 1000, 2000, 4000, 8000]]
        row += [f"{r['ned90_ms']:.0f}", f"{r['ned_late']:.2f}", f"{r['edc_rms']:.2f}", f"{r['ripple_db']:.2f}",
                f"{r['rho_LL_LR']:+.2f}", f"{r['rho_LL_RR']:+.2f}", f"{r['centroid']:.0f}",
                f"{r['lo_share']*100:.1f}", f"{r['hi_share']*100:.1f}", f"{r['peak_db']:.1f}", f"{r['dc_db']:.0f}",
                "y" if r["last_zero"] else "N"]
        print(("{:<26}" + "{:>6}" * (len(row) - 1)).format(*row))
        fails = verdict(r, js)
        if fails:
            bad += 1
            print(f"    FAIL {r['file']}: " + "; ".join(fails))
    print(f"\n{len(files)} files checked, {bad} with failures")
    return 1 if bad else 0


def verdict(r, rt_json):
    """Pass/fail thresholds. Springs are deliberately sparse/fluttery; non-linear and reverse IRs are not
    exponential, so decay-shape checks only apply to the exponential (space-like) IRs."""
    name = r["file"]
    spring = name.startswith("spring_")
    nonexp = name.startswith(("gated_", "nonlin_", "fx_reverse"))
    vintage = name.startswith("vintage_")
    f = []
    if r["clip"]: f.append("clipping")
    if not r["last_zero"]: f.append("last sample not zero")
    if r["dc_db"] > -60: f.append(f"DC {r['dc_db']:.0f} dB")
    if r["hi_share"] > 0.5 and not name.startswith("room_air"): f.append("excess >10 kHz energy")
    if not spring:
        if r["ned_late"] < 0.9: f.append(f"late echo density {r['ned_late']:.2f}")
        if not nonexp and r["ned90_ms"] > (150 if vintage else 100): f.append(f"slow density build-up {r['ned90_ms']:.0f} ms")
    if not spring and not nonexp:
        if r["edc_rms"] > 0.6: f.append(f"non-exponential decay (EDC rms {r['edc_rms']:.2f} dB)")
        if r["ripple_db"] == r["ripple_db"] and r["ripple_db"] > 1.0: f.append(f"envelope ripple/flutter {r['ripple_db']:.2f} dB")
        if rt_json == rt_json and abs(rt_json - r["t30"][1000]) > 0.05 * rt_json + 0.01:
            f.append(f"library rt60 {rt_json:.2f} != measured {r['t30'][1000]:.2f}")
    lim = 0.35 if spring else 0.2
    for k in ("rho_LL_LR", "rho_LL_RR"):
        if r[k] == r[k] and abs(r[k]) > lim: f.append(f"{k} {r[k]:+.2f}")
    return f


if __name__ == "__main__":
    sys.exit(main())
