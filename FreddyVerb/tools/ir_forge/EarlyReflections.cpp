// Randomised image-source early reflections.
//
// Allen & Berkley image method for a shoebox, enumerated up to `maxOrder`, with:
//  * a per-image random time jitter that grows with order (randomised image method), which removes
//    the perfectly periodic flutter between parallel walls that a strict shoebox produces;
//  * per-image gain jitter (surface irregularity);
//  * scattering: part of each reflection's energy is spread into a short random cluster;
//  * frequency-dependent wall absorption: reflections are accumulated per order and the order-k
//    buffer is low-passed k times;
//  * directional (cardioid-family) spaced microphones splayed outward.
#include "EarlyReflections.h"

namespace irf
{
namespace
{
    Vec3 sub (Vec3 a, Vec3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
    double len (Vec3 a) { return std::sqrt (a.x * a.x + a.y * a.y + a.z * a.z); }
    Vec3 norm (Vec3 a) { const double l = std::max (1e-9, len (a)); return { a.x / l, a.y / l, a.z / l }; }
    double dot (Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
}

ErResult renderEarlyReflections (const ErParams& p, double fs, size_t length)
{
    ErResult res;
    res.ir = Ir4 (fs, length);
    Rng rng (p.seed * 7919 + 17);

    const int K = std::max (1, p.maxOrder);
    // per order, per channel accumulation buffers
    std::vector<std::array<std::vector<double>, 4>> orderBuf ((size_t) K + 1);
    for (auto& ob : orderBuf)
        for (auto& c : ob) c.assign (length, 0.0);

    const Vec3 srcs[2] = { p.srcL, p.srcR };
    const Vec3 mics[2] = { p.micL, p.micR };

    // microphone aim vectors (xy-plane), splayed outward from the source axis
    const Vec3 srcC { 0.5 * (p.srcL.x + p.srcR.x), 0.5 * (p.srcL.y + p.srcR.y), 0.5 * (p.srcL.z + p.srcR.z) };
    const Vec3 micC { 0.5 * (p.micL.x + p.micR.x), 0.5 * (p.micL.y + p.micR.y), 0.5 * (p.micL.z + p.micR.z) };
    Vec3 axis = norm ({ srcC.x - micC.x, srcC.y - micC.y, 0.0 });
    Vec3 side = norm ({ p.micL.x - p.micR.x, p.micL.y - p.micR.y, 0.0 });
    const double s = p.splayDeg * kPi / 180.0;
    const Vec3 aim[2] = { norm ({ axis.x * std::cos (s) + side.x * std::sin (s), axis.y * std::cos (s) + side.y * std::sin (s), 0 }),
                          norm ({ axis.x * std::cos (s) - side.x * std::sin (s), axis.y * std::cos (s) - side.y * std::sin (s), 0 }) };

    const double maxDist = p.maxTimeMs * 0.001 * kSpeedOfSound + 1.0;
    const int nMax = (int) std::ceil (maxDist / (2.0 * std::min ({ p.room.x, p.room.y, p.room.z }))) + 1;

    for (int si = 0; si < 2; ++si)
    {
        const Vec3 src = srcs[si];
        for (int mi = 0; mi < 2; ++mi)
            res.directTime[(size_t) (si * 2 + mi)] = len (sub (src, mics[mi])) / kSpeedOfSound;
    }

    for (int nx = -nMax; nx <= nMax; ++nx)
        for (int qx = 0; qx <= 1; ++qx)
            for (int ny = -nMax; ny <= nMax; ++ny)
                for (int qy = 0; qy <= 1; ++qy)
                    for (int nz = -nMax; nz <= nMax; ++nz)
                        for (int qz = 0; qz <= 1; ++qz)
                        {
                            const int ox1 = std::abs (nx - qx), ox2 = std::abs (nx);
                            const int oy1 = std::abs (ny - qy), oy2 = std::abs (ny);
                            const int floorN = std::abs (nz - qz), ceilN = std::abs (nz);
                            const int order = ox1 + ox2 + oy1 + oy2 + floorN + ceilN;
                            if (order == 0 || order > K) continue;

                            const double refl = std::pow (p.betaWall, ox1 + ox2 + oy1 + oy2)
                                              * std::pow (p.betaFloor, floorN) * std::pow (p.betaCeiling, ceilN);

                            // one random draw set per image, shared by all four source/mic pairs so the
                            // image is a coherent physical event; plus per-pair small offsets
                            const double tj = p.jitterMs * 0.001 * (order >= 2 ? std::sqrt ((double) order - 1.0) : 0.3) * rng.bipolar();
                            const double gj = dbToGain (p.gainJitterDb * rng.bipolar());
                            Rng clusterRng = rng.fork ((uint64_t) (order * 131 + nx * 7 + ny * 3 + nz));

                            for (int si = 0; si < 2; ++si)
                            {
                                const Vec3 src = srcs[si];
                                const Vec3 img { (1 - 2 * qx) * src.x + 2 * nx * p.room.x,
                                                 (1 - 2 * qy) * src.y + 2 * ny * p.room.y,
                                                 (1 - 2 * qz) * src.z + 2 * nz * p.room.z };
                                for (int mi = 0; mi < 2; ++mi)
                                {
                                    const Vec3 d = sub (img, mics[mi]);
                                    const double dist = len (d);
                                    if (dist > maxDist) continue;
                                    const double t = dist / kSpeedOfSound + tj;
                                    if (t * 1000.0 > p.maxTimeMs) continue;
                                    const double cosT = dot (norm (d), aim[mi]);
                                    const double dir = (1.0 - p.cardioid) + p.cardioid * cosT;
                                    const double amp = refl * gj * dir / std::max (dist, 0.3);
                                    auto& buf = orderBuf[(size_t) order][(size_t) (si * 2 + mi)];

                                    const double specular = amp * std::sqrt (1.0 - p.scatter);
                                    addSincImpulse (buf, t * fs, specular);

                                    if (p.scatter > 0 && p.scatterTaps > 0)
                                    {
                                        Rng cr = clusterRng.fork ((uint64_t) (si * 2 + mi + 1));
                                        const double spread = p.scatterSpreadMs * 0.001 * std::sqrt ((double) order) * fs;
                                        const double a = amp * std::sqrt (p.scatter / p.scatterTaps);
                                        for (int k = 0; k < p.scatterTaps; ++k)
                                        {
                                            const double off = -std::log (std::max (1e-6, cr.uniform())) * spread * 0.5;
                                            const size_t idx = (size_t) (t * fs + off);
                                            if (idx < length) buf[idx] += a * cr.gauss() * 1.2;
                                        }
                                    }
                                }
                            }
                        }

    // frequency-dependent absorption per reflection order
    for (int k = 1; k <= K; ++k)
        for (int c = 0; c < 4; ++c)
        {
            auto& buf = orderBuf[(size_t) k][(size_t) c];
            for (int pass = 0; pass < k; ++pass)
            {
                auto lp = FirstOrder::lowpass (fs, p.wallLpHz);
                for (auto& v : buf) v = lp.process (v);
            }
            auto& out = res.ir.ch[(size_t) c];
            for (size_t i = 0; i < length; ++i) out[i] += buf[i];
        }

    // fade out at the end of the ER window
    const double tEnd = p.maxTimeMs * 0.001, tFade = std::min (p.fadeMs * 0.001, tEnd * 0.8);
    applyEnvelope (res.ir, [&] (double t) {
        if (t >= tEnd) return 0.0;
        if (t <= tEnd - tFade) return 1.0;
        return 0.5 + 0.5 * std::cos (kPi * (t - (tEnd - tFade)) / tFade);
    });
    return res;
}

} // namespace irf
