#!/usr/bin/env python3
"""Writes the factory presets (Resources/presets/*.fvpreset).

Each preset pairs a factory IR with duck/gate settings. Only values that differ
from the parameter defaults (= the Init preset) need to be listed; the plugin
fills in defaults for anything missing.
"""
import os, re
from xml.sax.saxutils import quoteattr

OUT = os.path.join(os.path.dirname(__file__), '..', '..', 'Resources', 'presets')

# choice indices
FOLLOW, CURVE = 0, 1
LIN, EXP, LOG, S = 0, 1, 2, 3                        # follow release curves / gate curves (0..2)
C_LIN, C_EXP, C_LOG, C_S, C_HOLD, C_PUMP, C_SWELL = range(7)
PEAK, RMS = 0, 1

P = []
def preset(name, cat, ir, **v): P.append((name, cat, ir, v))

# --- Snare ---------------------------------------------------------------
preset("Snare Bloom Plate", "Snare", "plate_bright_snare", duck_depth_db=-24, duck_release_ms=340, duck_threshold_db=-28,
       gate_hold_ms=180, gate_release_ms=320, gate_threshold_db=-42, mix_pct=38, predelay_ms=12, eq_hpf_freq=180)
preset("Tight Gated '80s Snare", "Snare", "gated_80s", duck_on=0, gate_hold_ms=60, gate_release_ms=40, gate_rel_curve=LIN,
       gate_threshold_db=-36, mix_pct=45, eq_hpf_freq=220, ir_length_pct=85, eq_ls_on=1, eq_ls_freq=300, eq_ls_gain=-3)
preset("Snare Room Snap", "Snare", "room_studio_snare", duck_depth_db=-12, duck_release_ms=120, gate_hold_ms=90,
       gate_release_ms=150, mix_pct=30, eq_hpf_freq=200)
preset("Snare Swell Hall", "Snare", "hall_medium", duck_mode=CURVE, curve_shape=C_SWELL, curve_duration_ms=520,
       duck_depth_db=-30, gate_hold_ms=320, gate_release_ms=600, mix_pct=40, predelay_ms=20)
preset("Snare Pump Chamber", "Snare", "chamber_echo", duck_mode=CURVE, curve_shape=C_PUMP, curve_duration_ms=280,
       duck_depth_db=-36, gate_hold_ms=200, gate_release_ms=260, mix_pct=36, eq_hpf_freq=250)
preset("Snare Spring Slap", "Snare", "spring_tight", duck_depth_db=-15, duck_release_ms=180, gate_hold_ms=120,
       gate_release_ms=220, mix_pct=32, eq_lpf_freq=7000)
preset("Ghost Note Haze", "Snare", "plate_dark", duck_depth_db=-9, duck_release_ms=500, duck_amount=0.5,
       gate_on=0, mix_pct=22, ir_damping_pct=35)
# --- Kick ----------------------------------------------------------------
preset("Kick Room Pump", "Kick", "room_kick_tight", duck_mode=CURVE, curve_shape=C_PUMP, curve_duration_ms=220,
       duck_depth_db=-61, gate_hold_ms=110, gate_release_ms=140, mix_pct=30, eq_hpf_freq=60, eq_lpf_freq=6000,
       mono_below_on=1, mono_below_hz=180)
preset("Kick Booth Thump", "Kick", "room_drum_booth", duck_depth_db=-18, duck_release_ms=150, gate_hold_ms=80,
       gate_release_ms=90, mix_pct=25, eq_hpf_freq=45, eq_lpf_freq=5000, mono_below_on=1, mono_below_hz=250)
preset("Kick Garage Tail", "Kick", "room_garage", duck_depth_db=-30, duck_release_ms=260, key_lpf=800,
       gate_hold_ms=160, gate_release_ms=260, mix_pct=28, eq_hpf_freq=70, mono_below_on=1, mono_below_hz=150)
preset("Kick Hold-Ramp Hall", "Kick", "hall_tom", duck_mode=CURVE, curve_shape=C_HOLD, curve_tension=0.2,
       curve_duration_ms=400, duck_depth_db=-61, gate_hold_ms=250, gate_release_ms=400, mix_pct=26,
       eq_hpf_freq=90, mono_below_on=1, mono_below_hz=200)
# --- Toms ----------------------------------------------------------------
preset("Tom Swell Hall", "Toms", "hall_tom", duck_mode=CURVE, curve_shape=C_SWELL, curve_duration_ms=600,
       duck_depth_db=-27, gate_hold_ms=380, gate_release_ms=700, mix_pct=40, predelay_ms=15, eq_hpf_freq=90)
preset("Tom Big Room", "Toms", "room_live", duck_depth_db=-20, duck_release_ms=300, gate_hold_ms=260,
       gate_release_ms=450, mix_pct=35, eq_hpf_freq=80)
preset("Tom Gated Big", "Toms", "gated_big", duck_on=0, gate_hold_ms=120, gate_release_ms=60, mix_pct=42, eq_hpf_freq=100)
preset("Tom Stone Bloom", "Toms", "room_stone", duck_depth_db=-26, duck_release_ms=450, duck_rel_curve=S,
       gate_hold_ms=420, gate_release_ms=900, mix_pct=34)
# --- Rim / clap / perc --------------------------------------------------
preset("Rim Chamber Flick", "Perc", "chamber_rim", duck_depth_db=-21, duck_release_ms=160, duck_attack_ms=0.5,
       gate_hold_ms=110, gate_release_ms=180, mix_pct=36, key_hpf=400, eq_hpf_freq=300)
preset("Clap Reverse Bloom", "Perc", "fx_reverse_clap", duck_mode=CURVE, curve_shape=C_LOG, curve_duration_ms=350,
       duck_depth_db=-40, gate_hold_ms=300, gate_release_ms=200, mix_pct=40, eq_hpf_freq=250)
preset("Perc Spring Pop", "Perc", "spring_perc", duck_depth_db=-18, duck_release_ms=200, gate_hold_ms=160,
       gate_release_ms=260, mix_pct=34, eq_hpf_freq=200, eq_lpf_freq=6500)
preset("Hat Air Room", "Perc", "room_air_hat", duck_depth_db=-12, duck_release_ms=90, duck_threshold_db=-34,
       gate_threshold_db=-46, gate_hold_ms=60, gate_release_ms=120, mix_pct=24, key_hpf=2000, eq_hpf_freq=600,
       width_pct=150)
preset("Shaker Shimmer", "Perc", "fx_metallic", duck_depth_db=-15, duck_release_ms=140, gate_hold_ms=90,
       gate_release_ms=220, mix_pct=18, key_hpf=1500, eq_hpf_freq=900, width_pct=170)
preset("Conga Wood Room", "Perc", "room_wood", duck_depth_db=-14, duck_release_ms=180, gate_hold_ms=140,
       gate_release_ms=220, mix_pct=30)
preset("Cowbell Spring Twin", "Perc", "spring_twin", duck_mode=CURVE, curve_shape=C_EXP, curve_duration_ms=240,
       duck_depth_db=-24, gate_hold_ms=200, gate_release_ms=300, mix_pct=28)
# --- Vintage / non-linear / FX -------------------------------------------
preset("Digital '87 Plate", "Vintage", "vintage_digital_plate", duck_depth_db=-20, duck_release_ms=280,
       gate_hold_ms=220, gate_release_ms=380, mix_pct=36, eq_lpf_freq=11000)
preset("Digital Room Snap", "Vintage", "vintage_digital_room", duck_depth_db=-14, duck_release_ms=150,
       gate_hold_ms=110, gate_release_ms=160, mix_pct=30)
preset("Ambience Glue", "Vintage", "vintage_ambience", duck_depth_db=-8, duck_release_ms=120, gate_on=0,
       mix_pct=22, width_pct=120)
preset("Reverse Non-Lin Suck", "Non-Linear", "nonlin_reverse", duck_on=0, gate_hold_ms=40, gate_release_ms=30,
       mix_pct=40, eq_hpf_freq=200)
preset("Flat Gate Wall", "Non-Linear", "nonlin_flat", duck_on=0, gate_hold_ms=200, gate_release_ms=25,
       gate_rel_curve=LIN, mix_pct=38)
preset("Bloom Cloud Swell", "FX", "fx_bloom", duck_mode=CURVE, curve_shape=C_SWELL, curve_duration_ms=900,
       duck_depth_db=-61, gate_hold_ms=600, gate_release_ms=1500, mix_pct=35, width_pct=160)
preset("Reverse Hall Riser", "FX", "fx_reverse_hall", duck_depth_db=-30, duck_release_ms=600, gate_hold_ms=500,
       gate_release_ms=300, mix_pct=38)
preset("Metal Shimmer Hit", "FX", "fx_metallic", duck_mode=CURVE, curve_shape=C_S, curve_duration_ms=500,
       duck_depth_db=-36, gate_hold_ms=350, gate_release_ms=800, mix_pct=30, eq_hpf_freq=400)
# --- Utility / mix -------------------------------------------------------
preset("Long Plate Tail (No Gate)", "Utility", "plate_long", duck_depth_db=-18, duck_release_ms=400, gate_on=0, mix_pct=30)
preset("Big Hall Ducked", "Utility", "hall_large", duck_depth_db=-24, duck_release_ms=500, gate_hold_ms=600,
       gate_release_ms=1200, mix_pct=32, predelay_ms=25)
preset("Bright Hall Snare Throw", "Utility", "hall_bright", duck_mode=CURVE, curve_shape=C_LIN, curve_duration_ms=700,
       curve_sync=1, curve_note=13, duck_depth_db=-40, gate_hold_ms=400, gate_hold_sync=1, gate_hold_note=13,
       gate_release_ms=500, mix_pct=36)
preset("Short Plate Lift", "Utility", "plate_short", duck_depth_db=-12, duck_release_ms=150, gate_hold_ms=150,
       gate_release_ms=200, mix_pct=28)
preset("Small Chamber Lookahead", "Utility", "chamber_small", lookahead_ms=3, duck_depth_db=-30, duck_attack_ms=0.3,
       duck_release_ms=220, gate_hold_ms=180, gate_release_ms=250, mix_pct=34)

os.makedirs(OUT, exist_ok=True)
for f in os.listdir(OUT):
    if f.endswith('.fvpreset'): os.remove(os.path.join(OUT, f))
for name, cat, ir, vals in P:
    fn = re.sub(r'[^A-Za-z0-9]+', '_', name).strip('_') + '.fvpreset'
    params = ''.join(f'  <PARAM id="{k}" value="{v}"/>\n' for k, v in sorted(vals.items()))
    xml = (f'<?xml version="1.0" encoding="UTF-8"?>\n'
           f'<FreddyPreset version="1" name={quoteattr(name)} category={quoteattr(cat)} author="Freddy Verb Factory">\n'
           f'{params}  <IR kind="factory" id="{ir}"/>\n</FreddyPreset>\n')
    open(os.path.join(OUT, fn), 'w').write(xml)
print(f'wrote {len(P)} presets to {os.path.normpath(OUT)}')
