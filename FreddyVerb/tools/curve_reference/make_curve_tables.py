#!/usr/bin/env python3
"""Independent reference implementation of the Curve-mode duck shapes.

Writes tests/data/curve_reference.csv (shape, tension, p, value). The C++ unit
test compares dsp::CurveShapes::evaluate against this table, so the shapes are
pinned by two separate implementations of the published formulas.
"""
import math, os, sys

SWELL_PEAK = 10 ** (1.5 / 20)

def smooth(x):
    x = min(1.0, max(0.0, x))
    return 0.5 - 0.5 * math.cos(math.pi * x)

def shape(s, p, T):
    if p <= 0: return 0.0
    if p >= 1: return 1.0
    if s == 0:   # linear ramp
        return 1 - (1 - p) ** (1 + 3 * T) if T >= 0 else p ** (1 - 3 * T)
    if s == 1:   # exponential
        k = 5 * math.exp(T); return (1 - math.exp(-k * p)) / (1 - math.exp(-k))
    if s == 2:   # logarithmic
        k = 5 * math.exp(T); return (math.exp(k * p) - 1) / (math.exp(k) - 1)
    if s == 3:   # S-curve
        a = 2 * math.exp(1.2 * T); return p ** a / (p ** a + (1 - p) ** a)
    if s == 4:   # hold then ramp
        h = 0.5 + 0.4 * T; return 0.0 if p < h else smooth((p - h) / (1 - h))
    if s == 5:   # pump
        x = max(0.0, (p - 0.04) / 0.96); c = 2.6 * math.exp(0.8 * T); return 1 - (1 - x) ** c
    if s == 6:   # swell
        pk = 0.7 + 0.15 * T
        if p < pk: return SWELL_PEAK * math.sin(0.5 * math.pi * p / pk) ** 2
        return 1 + (SWELL_PEAK - 1) * (0.5 + 0.5 * math.cos(math.pi * (p - pk) / (1 - pk)))
    raise ValueError(s)

out = os.path.join(os.path.dirname(__file__), '..', '..', 'tests', 'data', 'curve_reference.csv')
with open(out, 'w') as f:
    f.write('shape,tension,p,value\n')
    for s in range(7):
        for T in (-1.0, -0.5, 0.0, 0.5, 1.0):
            for i in range(0, 201):
                p = i / 200
                f.write(f'{s},{T},{p:.6f},{shape(s, p, T):.15f}\n')
print('wrote', os.path.normpath(out))
