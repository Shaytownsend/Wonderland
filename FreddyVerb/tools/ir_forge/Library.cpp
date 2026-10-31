// Factory IR library: every shipped IR, its generator and parameters.
//
// Conventions: RT60 targets are given as (low, mid, high, air) = below ~250 Hz, mid band, above ~3.5-4 kHz,
// above ~9-10 kHz. The value written to library.json is *measured* from the final render (see PostProcess).
#include "Library.h"

#include "FxGenerators.h"
#include "NonLinear.h"
#include "PlateReverb.h"
#include "SpaceReverb.h"
#include "SpringReverb.h"
#include "VintageDigital.h"

namespace irf
{
namespace
{
    struct Rt
    {
        double low, mid, high, air;
    };

    void setRt (FdnParams& f, Rt rt)
    {
        f.rtLow = rt.low; f.rtMid = rt.mid; f.rtHigh = rt.high; f.rtAir = rt.air;
    }
    double maxRt (Rt rt) { return std::max ({ rt.low, rt.mid, rt.high, rt.air }); }

    /** An acoustic space from its dimensions: drum kit near one end, spaced room pair facing it. */
    SpaceParams makeSpace (Vec3 dims, Rt rt, uint64_t seed, double micDist, double micSpacing, int lines = 16)
    {
        SpaceParams s;
        auto& e = s.er;
        e.room = dims;
        const double cx = dims.x * 0.47, cy = dims.y * 0.32;
        e.srcL = { cx - 0.45, cy, 0.75 };
        e.srcR = { cx + 0.45, cy + 0.12, 0.85 };
        const double my = std::min (dims.y - 0.6, cy + micDist);
        e.micL = { cx - micSpacing * 0.5, my, std::min (dims.z - 0.4, 1.8) };
        e.micR = { cx + micSpacing * 0.5 + 0.07, my - 0.05, std::min (dims.z - 0.4, 1.75) };
        e.seed = seed;

        const double V = dims.x * dims.y * dims.z, S = 2 * (dims.x * dims.y + dims.x * dims.z + dims.y * dims.z);
        const double mfpMs = 4.0 * V / S / kSpeedOfSound * 1000.0;
        e.maxTimeMs = std::clamp (mfpMs * 5.0, 25.0, 130.0);
        e.maxOrder = dims.x < 8 ? 5 : (dims.x < 15 ? 4 : 3);

        auto& f = s.fdn;
        f.numLines = lines;
        f.minDelayMs = std::max (4.0, mfpMs * 0.55);
        f.maxDelayMs = std::max (11.0, mfpMs * 2.1);
        f.diffMinMs = 0.7;
        f.diffMaxMs = std::clamp (mfpMs * 0.9, 3.0, 12.0);
        f.diffStages = 6;
        setRt (f, rt);
        f.seed = seed * 13 + 5;
        s.lengthS = maxRt (rt) * 1.7 + 0.3;
        return s;
    }

    PlateParams makePlate (Rt rt, uint64_t seed)
    {
        PlateParams p;
        p.rtLow = rt.low; p.rtMid = rt.mid; p.rtHigh = rt.high; p.rtAir = rt.air;
        p.tension = 1500.0;
        p.seed = seed;
        p.lengthS = maxRt (rt) * 1.65 + 0.2;
        p.eq = { { EqBand::HP, 80.0, 0.5412 }, { EqBand::HP, 80.0, 1.3066 } };
        return p;
    }

    FinishParams fin (double endDb = -90.0, double fadeFromDb = -65.0)
    {
        FinishParams f;
        f.endDb = endDb;
        f.fadeFromDb = fadeFromDb;
        return f;
    }

    IrDef def (std::string id, std::string name, std::string cat, std::string gen, uint64_t seed, std::string desc,
               std::function<Ir4 (uint64_t)> fn, FinishParams f = fin())
    {
        IrDef d;
        d.id = std::move (id); d.name = std::move (name); d.category = std::move (cat);
        d.generator = std::move (gen); d.seed = seed; d.description = std::move (desc);
        d.render = std::move (fn); d.finish = f;
        return d;
    }

    std::vector<IrDef> build()
    {
        std::vector<IrDef> L;

        //==========================================================================================
        // Rooms
        //==========================================================================================
        L.push_back (def ("room_kick_tight", "Tight Kick Room", "Rooms", "space (image-source ER + 16-line FDN)", 1101,
            "Small, punchy room with a short warm bloom that adds body and size to kick drums without smearing the attack.",
            [] (uint64_t seed) {
                auto s = makeSpace ({ 4.3, 3.7, 2.6 }, { 0.42, 0.34, 0.26, 0.17 }, seed, 1.6, 0.9);
                s.er.betaWall = 0.80; s.er.betaFloor = 0.86; s.er.betaCeiling = 0.62; s.er.wallLpHz = 7000;
                s.er.scatter = 0.4;
                s.erToLateDb = 1.5;
                s.fdn.fLow = 180;
                s.eq = { { EqBand::LowShelf, 110, 1.0, 2.0 }, { EqBand::Peak, 3500, 0.9, -1.5 }, { EqBand::HP, 32, 0.707 } };
                return renderSpace (s);
            }));

        L.push_back (def ("room_wood", "Wood Room", "Rooms", "space (image-source ER + 16-line FDN)", 1102,
            "Warm, wood-panelled tracking room with a rich low-mid glow; a natural all-rounder for snare and toms.",
            [] (uint64_t seed) {
                auto s = makeSpace ({ 6.6, 5.3, 3.3 }, { 0.78, 0.68, 0.52, 0.32 }, seed, 2.2, 1.1);
                s.er.betaWall = 0.84; s.er.betaFloor = 0.86; s.er.betaCeiling = 0.78; s.er.wallLpHz = 5500;
                s.er.scatter = 0.5;
                s.erToLateDb = -0.5;
                s.fdn.fHigh = 3000;
                s.eq = { { EqBand::Peak, 420, 0.8, 2.0 }, { EqBand::Peak, 2800, 1.0, -1.0 }, { EqBand::HighShelf, 9000, 0.8, -2.0 } };
                return renderSpace (s);
            }));

        L.push_back (def ("room_live", "Live Room", "Rooms", "space (image-source ER + 16-line FDN)", 1103,
            "Big, bright live room with strong early reflections for an open, exciting drum sound with real size.",
            [] (uint64_t seed) {
                auto s = makeSpace ({ 9.4, 7.2, 4.2 }, { 1.15, 1.1, 0.95, 0.6 }, seed, 3.2, 1.6);
                s.er.betaWall = 0.9; s.er.betaFloor = 0.9; s.er.betaCeiling = 0.86; s.er.wallLpHz = 11000;
                s.er.scatter = 0.35;
                s.erToLateDb = -1.5;
                s.eq = { { EqBand::Peak, 250, 0.8, -1.0 }, { EqBand::HighShelf, 6000, 0.8, 1.5 } };
                return renderSpace (s);
            }));

        L.push_back (def ("room_dead", "Dead Room", "Rooms", "space (image-source ER + 16-line FDN)", 1104,
            "Heavily treated small room: mostly tight early reflections that thicken close mics without an audible tail.",
            [] (uint64_t seed) {
                auto s = makeSpace ({ 4.6, 4.0, 2.7 }, { 0.30, 0.22, 0.15, 0.10 }, seed, 1.5, 0.8);
                s.er.betaWall = 0.55; s.er.betaFloor = 0.78; s.er.betaCeiling = 0.42; s.er.wallLpHz = 4500;
                s.er.scatter = 0.45;
                s.erToLateDb = 4.0;
                s.eq = { { EqBand::Peak, 200, 0.9, 1.5 }, { EqBand::HighShelf, 7000, 0.8, -3.0 } };
                return renderSpace (s);
            }));

        L.push_back (def ("room_air_hat", "Air Room (Hats)", "Rooms", "space (image-source ER + 16-line FDN)", 1105,
            "Airy, bright room with the lows removed so hi-hats, cymbals and shakers gain space without washing up the kit.",
            [] (uint64_t seed) {
                auto s = makeSpace ({ 7.2, 5.6, 3.7 }, { 0.42, 0.6, 0.62, 0.48 }, seed, 2.6, 1.3);
                s.er.betaWall = 0.88; s.er.betaFloor = 0.82; s.er.betaCeiling = 0.88; s.er.wallLpHz = 15000;
                s.er.scatter = 0.45;
                s.erToLateDb = -2.0;
                s.fdn.fAir = 12000;
                s.eq = { { EqBand::HP, 280, 0.5412 }, { EqBand::HP, 280, 1.3066 }, { EqBand::HighShelf, 7500, 0.8, 3.0 },
                         { EqBand::Peak, 900, 1.0, -1.5 } };
                return renderSpace (s);
            }));

        L.push_back (def ("room_studio_snare", "Studio Snare Room", "Rooms", "space (image-source ER + 16-line FDN)", 1106,
            "Medium studio room voiced around the snare: crisp early reflections and a short, dense half-second tail.",
            [] (uint64_t seed) {
                auto s = makeSpace ({ 5.6, 4.7, 3.0 }, { 0.6, 0.55, 0.47, 0.3 }, seed, 2.0, 1.0);
                s.er.betaWall = 0.84; s.er.betaFloor = 0.84; s.er.betaCeiling = 0.7; s.er.wallLpHz = 8500;
                s.erToLateDb = 0.5;
                s.eq = { { EqBand::HP, 60, 0.707 }, { EqBand::Peak, 1800, 0.9, 1.0 } };
                return renderSpace (s);
            }));

        L.push_back (def ("room_drum_booth", "Drum Booth", "Rooms", "space (image-source ER + 16-line FDN)", 1107,
            "Very small iso-booth ambience: a tight slap of close walls that adds depth to dry close mics.",
            [] (uint64_t seed) {
                auto s = makeSpace ({ 3.3, 2.9, 2.5 }, { 0.26, 0.2, 0.16, 0.11 }, seed, 1.1, 0.6);
                s.er.betaWall = 0.72; s.er.betaFloor = 0.82; s.er.betaCeiling = 0.55; s.er.wallLpHz = 6000;
                s.erToLateDb = 3.0;
                s.eq = { { EqBand::Peak, 600, 1.0, 1.0 }, { EqBand::HighShelf, 8000, 0.8, -2.0 } };
                return renderSpace (s);
            }));

        L.push_back (def ("room_stone", "Stone Room", "Rooms", "space (image-source ER + 16-line FDN)", 1108,
            "Hard stone room with a long, bright and dense decay for big, roomy snares and tom fills.",
            [] (uint64_t seed) {
                auto s = makeSpace ({ 8.3, 6.1, 5.2 }, { 1.5, 1.45, 1.2, 0.75 }, seed, 3.0, 1.5);
                s.er.betaWall = 0.93; s.er.betaFloor = 0.92; s.er.betaCeiling = 0.9; s.er.wallLpHz = 10000;
                s.er.scatter = 0.3;
                s.erToLateDb = -3.0;
                s.eq = { { EqBand::Peak, 1500, 0.8, 1.0 } };
                return renderSpace (s);
            }));

        L.push_back (def ("room_garage", "Garage", "Rooms", "space (image-source ER + 16-line FDN)", 1109,
            "Concrete garage with boxy low mids and a hard, reflective character for lo-fi and garage-rock drums.",
            [] (uint64_t seed) {
                auto s = makeSpace ({ 6.1, 3.5, 2.6 }, { 0.95, 0.85, 0.68, 0.42 }, seed, 2.0, 1.0);
                s.er.betaWall = 0.92; s.er.betaFloor = 0.93; s.er.betaCeiling = 0.85; s.er.wallLpHz = 8000;
                s.er.scatter = 0.25;
                s.erToLateDb = 0.0;
                s.eq = { { EqBand::Peak, 330, 1.0, 2.5 }, { EqBand::Peak, 4000, 0.8, -1.5 } };
                return renderSpace (s);
            }));

        //==========================================================================================
        // Plates (modal)
        //==========================================================================================
        L.push_back (def ("plate_medium", "Medium Plate", "Plates", "modal plate", 2101,
            "Classic medium steel plate: dense, smooth and bright - the go-to reverb for snare and toms.",
            [] (uint64_t seed) {
                auto p = makePlate ({ 1.35, 1.6, 1.35, 0.75 }, seed);
                p.eq.push_back ({ EqBand::HighShelf, 6000, 0.7, 1.0 });
                return renderPlate (p);
            }));

        L.push_back (def ("plate_bright_snare", "Bright Snare Plate", "Plates", "modal plate", 2102,
            "Short, sizzling plate with an extended top end that makes snares crack and sit forward in the mix.",
            [] (uint64_t seed) {
                auto p = makePlate ({ 0.9, 1.3, 1.25, 0.9 }, seed);
                p.sizeX = 1.7; p.sizeY = 0.95;
                p.taperHz = 13000; p.fMax = 19000;
                p.eq = { { EqBand::HP, 150, 0.5412 }, { EqBand::HP, 150, 1.3066 }, { EqBand::HighShelf, 4500, 0.7, 3.5 },
                         { EqBand::Peak, 400, 0.9, -1.5 } };
                return renderPlate (p);
            }));

        L.push_back (def ("plate_short", "Short Plate", "Plates", "modal plate", 2103,
            "Tight plate with a sub-second decay for busy grooves and fast fills where a long tail would clutter.",
            [] (uint64_t seed) {
                auto p = makePlate ({ 0.65, 0.8, 0.7, 0.42 }, seed);
                p.sizeX = 1.8; p.sizeY = 0.9;
                p.eq.push_back ({ EqBand::HighShelf, 6000, 0.7, 1.0 });
                return renderPlate (p);
            }));

        L.push_back (def ("plate_long", "Long Plate", "Plates", "modal plate", 2104,
            "Lush, long plate for ballad snares, sparse tom hits and dramatic accents.",
            [] (uint64_t seed) {
                auto p = makePlate ({ 2.3, 2.8, 2.25, 1.2 }, seed);
                p.sizeX = 2.1; p.sizeY = 1.05;
                return renderPlate (p);
            }, fin (-85.0, -62.0)));

        L.push_back (def ("plate_dark", "Dark Plate", "Plates", "modal plate", 2105,
            "Warm, darker plate with heavy damping that thickens toms and kicks without hiss or splash.",
            [] (uint64_t seed) {
                auto p = makePlate ({ 1.6, 1.8, 1.0, 0.45 }, seed);
                p.thickness = 0.0006;
                p.taperHz = 9000; p.fMax = 14000;
                p.eq = { { EqBand::HP, 60, 0.5412 }, { EqBand::HP, 60, 1.3066 }, { EqBand::HighShelf, 4500, 0.7, -3.0 },
                         { EqBand::Peak, 250, 0.8, 1.5 } };
                return renderPlate (p);
            }));

        //==========================================================================================
        // Chambers
        //==========================================================================================
        L.push_back (def ("chamber_rim", "Rim Chamber", "Chambers", "space (image-source ER + 16-line FDN)", 3101,
            "Small, bright tiled chamber that gives rimshots and cross-stick an articulate, woody ring.",
            [] (uint64_t seed) {
                auto s = makeSpace ({ 4.7, 3.3, 2.9 }, { 0.85, 0.9, 0.8, 0.55 }, seed, 2.0, 0.7);
                s.er.betaWall = 0.95; s.er.betaFloor = 0.95; s.er.betaCeiling = 0.94; s.er.wallLpHz = 12000;
                s.er.cardioid = 0.3; s.er.scatter = 0.55;
                s.erToLateDb = -3.0;
                s.fdn.diffG = 0.68;
                s.eq = { { EqBand::HP, 120, 0.707 }, { EqBand::Peak, 2400, 0.9, 2.0 }, { EqBand::Peak, 500, 1.0, -1.5 } };
                return renderSpace (s);
            }));

        L.push_back (def ("chamber_echo", "Echo Chamber", "Chambers", "space (image-source ER + 16-line FDN)", 3102,
            "Classic studio echo chamber: smooth, dense and mid-forward - the sound of '60s and '70s snare records.",
            [] (uint64_t seed) {
                auto s = makeSpace ({ 7.6, 5.1, 4.1 }, { 1.75, 1.8, 1.45, 0.85 }, seed, 4.0, 1.2);
                s.er.betaWall = 0.95; s.er.betaFloor = 0.95; s.er.betaCeiling = 0.94; s.er.wallLpHz = 9000;
                s.er.cardioid = 0.3; s.er.scatter = 0.6;
                s.erToLateDb = -6.0;
                s.fdn.diffG = 0.7;
                s.eq = { { EqBand::HP, 90, 0.707 }, { EqBand::Peak, 1200, 0.7, 1.5 }, { EqBand::HighShelf, 8000, 0.7, -1.5 } };
                return renderSpace (s);
            }));

        L.push_back (def ("chamber_small", "Small Chamber", "Chambers", "space (image-source ER + 16-line FDN)", 3103,
            "Compact concrete chamber with a dense, slightly boxy one-second tail that glues snare and toms together.",
            [] (uint64_t seed) {
                auto s = makeSpace ({ 5.2, 3.9, 3.1 }, { 1.2, 1.15, 0.95, 0.6 }, seed, 2.6, 0.9);
                s.er.betaWall = 0.94; s.er.betaFloor = 0.94; s.er.betaCeiling = 0.93; s.er.wallLpHz = 9500;
                s.er.cardioid = 0.3; s.er.scatter = 0.55;
                s.erToLateDb = -4.0;
                s.fdn.diffG = 0.68;
                s.eq = { { EqBand::HP, 80, 0.707 }, { EqBand::Peak, 700, 0.8, 1.2 } };
                return renderSpace (s);
            }));

        //==========================================================================================
        // Halls
        //==========================================================================================
        L.push_back (def ("hall_tom", "Tom Hall", "Halls", "space (image-source ER + 32-line FDN)", 4101,
            "Warm medium hall with extra low-end sustain that makes toms and floor toms roll and bloom.",
            [] (uint64_t seed) {
                auto s = makeSpace ({ 21.0, 15.0, 9.5 }, { 2.25, 1.9, 1.45, 0.8 }, seed, 7.0, 2.0, 32);
                s.er.betaWall = 0.85; s.er.betaFloor = 0.8; s.er.betaCeiling = 0.85; s.er.wallLpHz = 7000;
                s.erToLateDb = -8.0;
                s.fdn.modDepthMs = 0.12;
                s.fdn.fLow = 300;
                s.eq = { { EqBand::LowShelf, 180, 0.8, 1.5 }, { EqBand::HighShelf, 7000, 0.7, -1.5 } };
                return renderSpace (s);
            }));

        L.push_back (def ("hall_large", "Large Hall", "Halls", "space (image-source ER + 32-line FDN)", 4102,
            "Large concert hall with a long, smooth decay for epic snares, big tom fills and cinematic percussion.",
            [] (uint64_t seed) {
                auto s = makeSpace ({ 34.0, 24.0, 15.0 }, { 3.5, 3.2, 2.5, 1.4 }, seed, 11.0, 2.6, 32);
                s.er.betaWall = 0.87; s.er.betaFloor = 0.8; s.er.betaCeiling = 0.87; s.er.wallLpHz = 8000;
                s.erToLateDb = -10.0;
                s.fdn.modDepthMs = 0.15;
                return renderSpace (s);
            }, fin (-85.0, -62.0)));

        L.push_back (def ("hall_medium", "Medium Hall", "Halls", "space (image-source ER + 32-line FDN)", 4103,
            "Medium hall with a balanced, smooth two-second decay - a versatile snare and percussion reverb.",
            [] (uint64_t seed) {
                auto s = makeSpace ({ 18.0, 13.0, 8.0 }, { 2.35, 2.2, 1.8, 1.0 }, seed, 6.0, 2.0, 32);
                s.er.betaWall = 0.86; s.er.betaFloor = 0.8; s.er.betaCeiling = 0.86; s.er.wallLpHz = 8500;
                s.erToLateDb = -7.0;
                s.fdn.modDepthMs = 0.12;
                return renderSpace (s);
            }));

        L.push_back (def ("hall_bright", "Bright Hall", "Halls", "space (image-source ER + 32-line FDN)", 4104,
            "Bright, modern hall with extended high-frequency decay that adds sparkle and air to snares and cymbals.",
            [] (uint64_t seed) {
                auto s = makeSpace ({ 24.0, 17.0, 11.0 }, { 2.2, 2.5, 2.3, 1.5 }, seed, 8.0, 2.2, 32);
                s.er.betaWall = 0.9; s.er.betaFloor = 0.85; s.er.betaCeiling = 0.9; s.er.wallLpHz = 12000;
                s.erToLateDb = -8.0;
                s.fdn.modDepthMs = 0.12;
                s.fdn.fAir = 11000;
                s.eq = { { EqBand::HP, 70, 0.707 }, { EqBand::HighShelf, 5000, 0.7, 2.0 } };
                return renderSpace (s);
            }));

        //==========================================================================================
        // Springs
        //==========================================================================================
        // Four springs per tank; input/output polarities follow a 4x4 Hadamard pattern so the four
        // true-stereo paths are mutually uncorrelated sums of the same physical springs.
        auto tank = [] (std::vector<SpringUnit> units, double hpHz, double lpHz, double peakHz, double peakDb, double len, uint64_t seed) {
            SpringParams p;
            p.seed = seed;
            const double s[4] = { 1, 1, -1, -1 }, t[4] = { 1, -1, 1, -1 };
            for (size_t k = 0; k < units.size() && k < 4; ++k)
            {
                units[k].inputGain[0] = 1.0; units[k].inputGain[1] = s[k];
                units[k].outputGain[0] = 1.0; units[k].outputGain[1] = t[k];
            }
            p.springs = units;
            p.lengthS = len;
            p.eq = { { EqBand::HP, hpHz, 0.5412 }, { EqBand::HP, hpHz, 1.3066 },
                     { EqBand::LP, lpHz, 0.5098 }, { EqBand::LP, lpHz, 0.6013 }, { EqBand::LP, lpHz, 0.9000 }, { EqBand::LP, lpHz, 2.5629 },
                     { EqBand::Peak, peakHz, 0.9, peakDb } };
            return renderSpring (p);
        };
        auto unit = [] (double loopMs, int stages, double a, double rt, double modRate) {
            SpringUnit u;
            u.loopMs = loopMs; u.stages = stages; u.apCoef = a; u.rt60 = rt; u.modRateHz = modRate;
            return u;
        };

        L.push_back (def ("spring_perc", "Perc Spring", "Springs", "spring tank (stretched-allpass chirp loops)", 5101,
            "Short, twangy spring tank that adds vintage drip and bounce to percussion and snares.",
            [tank, unit] (uint64_t seed) {
                return tank ({ unit (27, 90, 0.60, 1.5, 1.1), unit (31, 96, 0.63, 1.6, 0.8),
                               unit (34, 100, 0.61, 1.7, 1.3), unit (29, 92, 0.65, 1.5, 0.9) },
                             140, 4800, 2200, 2.0, 2.9, seed);
            }));

        L.push_back (def ("spring_twin", "Twin Spring", "Springs", "spring tank (stretched-allpass chirp loops)", 5102,
            "Classic amp-style twin spring tank with a long, boingy decay for surf drums and dub snares.",
            [tank, unit] (uint64_t seed) {
                return tank ({ unit (38, 110, 0.63, 2.5, 1.1), unit (45, 120, 0.60, 2.7, 0.7),
                               unit (52, 100, 0.66, 2.3, 1.4), unit (41, 115, 0.62, 2.6, 0.9) },
                             110, 4500, 1500, 1.5, 4.4, seed);
            }, fin (-85.0, -62.0)));

        L.push_back (def ("spring_tight", "Tight Spring", "Springs", "spring tank (stretched-allpass chirp loops)", 5103,
            "Damped, short spring for subtle drip on rimshots, bongos and lo-fi breaks.",
            [tank, unit] (uint64_t seed) {
                auto a = unit (22, 80, 0.58, 0.95, 1.2), b = unit (25, 85, 0.60, 1.0, 1.6),
                     c = unit (24, 82, 0.59, 0.9, 1.0), d = unit (20, 78, 0.61, 1.0, 1.4);
                for (auto* u : { &a, &b, &c, &d }) u->hfDampHz = 2600;
                return tank ({ a, b, c, d }, 180, 4300, 2000, 1.0, 1.9, seed);
            }));

        //==========================================================================================
        // Vintage digital
        //==========================================================================================
        L.push_back (def ("vintage_digital_room", "Vintage Digital Room", "Vintage", "vintage digital (FDN @ 32 kHz, 12-bit gain-ranging DAC)", 6101,
            "Late-'80s digital room: grainy, chorused and band-limited - instant classic-record drum ambience.",
            [] (uint64_t seed) {
                VintageParams v;
                v.seed = seed;
                v.engine = VintageParams::Fdn;
                v.fdn.numLines = 16; v.fdn.minDelayMs = 7; v.fdn.maxDelayMs = 24;
                setRt (v.fdn, { 0.95, 0.9, 0.7, 0.5 });
                v.fdn.modDepthMs = 0.35; v.fdn.modRateLo = 0.4; v.fdn.modRateHi = 1.1; v.fdn.modRandomAmt = 0.3;
                v.fdn.diffStages = 4; v.fdn.diffMinMs = 1.5; v.fdn.diffMaxMs = 7.0; v.fdn.diffG = 0.65;
                v.fdn.quantBits = 20;
                v.fdn.seed = seed * 3;
                v.taps = { 12, 6.0, 55.0, -10.0, 0.45 };
                v.bandwidthHz = 11000;
                v.lengthS = 1.9;
                v.eq = { { EqBand::HP, 70, 0.707 }, { EqBand::Peak, 1800, 0.8, 1.0 } };
                return renderVintage (v);
            }));

        L.push_back (def ("vintage_digital_plate", "Vintage Digital Plate", "Vintage", "vintage digital (figure-8 allpass tank @ 32 kHz)", 6102,
            "Glassy late-'80s digital plate with chorused shimmer and a 12 kHz ceiling - big, bright '80s snares.",
            [] (uint64_t seed) {
                VintageParams v;
                v.seed = seed;
                v.engine = VintageParams::Dattorro;
                v.dattorro.rt60 = 1.8;
                v.dattorro.size = 0.8;
                v.dattorro.inputDiff1 = 0.78; v.dattorro.inputDiff2 = 0.68;
                v.dattorro.damping = 0.15;
                v.dattorro.excursion = 14;
                v.dattorro.modRateHz = 0.9;
                v.bandwidthHz = 12000;
                v.lengthS = 3.2;
                v.eq = { { EqBand::HP, 110, 0.707 }, { EqBand::HighShelf, 5000, 0.7, 1.5 } };
                return renderVintage (v);
            }));

        L.push_back (def ("vintage_digital_hall", "Vintage Digital Hall", "Vintage", "vintage digital (FDN @ 32 kHz, 12-bit gain-ranging DAC)", 6103,
            "Dark, chorused '80s digital hall with a smooth, wide tail that makes toms and snares huge and dreamy.",
            [] (uint64_t seed) {
                VintageParams v;
                v.seed = seed;
                v.engine = VintageParams::Fdn;
                v.fdn.numLines = 16; v.fdn.minDelayMs = 22; v.fdn.maxDelayMs = 68;
                setRt (v.fdn, { 2.8, 2.5, 1.7, 1.0 });
                v.fdn.modDepthMs = 0.6; v.fdn.modRateLo = 0.3; v.fdn.modRateHi = 0.9; v.fdn.modRandomAmt = 0.4;
                v.fdn.diffStages = 6; v.fdn.diffMinMs = 2; v.fdn.diffMaxMs = 12; v.fdn.diffG = 0.68;
                v.fdn.quantBits = 20;
                v.fdn.seed = seed * 3;
                v.preDelayMs = 18;
                v.taps = { 8, 10.0, 70.0, -8.0, 0.2 };
                v.bandwidthHz = 10000;
                v.lengthS = 4.6;
                v.eq = { { EqBand::HP, 60, 0.707 } };
                return renderVintage (v);
            }, fin (-85.0, -62.0)));

        L.push_back (def ("vintage_ambience", "Vintage Ambience", "Vintage", "vintage digital (FDN @ 32 kHz, 12-bit gain-ranging DAC)", 6104,
            "Short, dense '80s digital ambience program that adds instant size and grit to drum loops and close mics.",
            [] (uint64_t seed) {
                VintageParams v;
                v.seed = seed;
                v.engine = VintageParams::Fdn;
                v.fdn.numLines = 16; v.fdn.minDelayMs = 4; v.fdn.maxDelayMs = 13;
                setRt (v.fdn, { 0.5, 0.45, 0.36, 0.25 });
                v.fdn.modDepthMs = 0.25; v.fdn.modRateLo = 0.5; v.fdn.modRateHi = 1.3;
                v.fdn.diffStages = 5; v.fdn.diffMinMs = 0.8; v.fdn.diffMaxMs = 5; v.fdn.diffG = 0.66;
                v.fdn.quantBits = 20;
                v.fdn.seed = seed * 3;
                v.taps = { 14, 3.0, 35.0, -9.0, 0.6 };
                v.bandwidthHz = 12500;
                v.lengthS = 1.0;
                v.eq = { { EqBand::HP, 80, 0.707 } };
                return renderVintage (v);
            }));

        //==========================================================================================
        // Non-linear
        //==========================================================================================
        L.push_back (def ("gated_80s", "Gated '80s", "Non-Linear", "gated (live-room base, flattened + gate)", 7101,
            "Huge compressed-and-gated room in the '80s style: a wall of ambience that slams shut at around 360 ms.",
            [] (uint64_t seed) {
                auto s = makeSpace ({ 10.0, 8.0, 4.5 }, { 2.4, 2.4, 2.0, 1.2 }, seed, 3.5, 1.8);
                s.er.betaWall = 0.9; s.er.betaFloor = 0.9; s.er.wallLpHz = 10000;
                s.erToLateDb = -2.0;
                s.lengthS = 0.8;
                s.eq = { { EqBand::HP, 70, 0.5412 }, { EqBand::HP, 70, 1.3066 }, { EqBand::Peak, 200, 0.8, 1.5 },
                         { EqBand::Peak, 3000, 0.8, 1.5 }, { EqBand::LP, 15000, 0.707 } };
                auto base = renderSpace (s);
                NonLinParams n;
                n.shape = NonLinParams::Gated;
                n.gateMs = 360; n.fadeMs = 14; n.startDb = 0; n.endDb = -3.5; n.keepNaturalMs = 18;
                return applyNonLinear (base, n);
            }));

        L.push_back (def ("nonlin_reverse", "Reverse Non-Lin", "Non-Linear", "non-linear (dense FDN base, rising envelope + gate)", 7102,
            "Rising, backwards-sounding non-linear program that swells into a hard cut at around 320 ms.",
            [] (uint64_t seed) {
                FdnParams f;
                f.numLines = 32; f.minDelayMs = 4; f.maxDelayMs = 18;
                f.rtLow = 3.0; f.rtMid = 3.0; f.rtHigh = 2.5; f.rtAir = 1.5;
                f.diffStages = 8; f.diffMinMs = 0.5; f.diffMaxMs = 6; f.diffG = 0.7;
                f.loopAllpasses = 1; f.loopApMinMs = 0.4; f.loopApMaxMs = 1.8;
                f.seed = seed;
                auto base = renderFdnImpulse (f, kOutputRate, (size_t) (0.6 * kOutputRate));
                applyEq (base, { { EqBand::HP, 90, 0.5412 }, { EqBand::HP, 90, 1.3066 }, { EqBand::HighShelf, 6000, 0.7, 1.0 },
                                 { EqBand::LP, 13000, 0.707 } });
                NonLinParams n;
                n.shape = NonLinParams::Reverse;
                n.gateMs = 320; n.fadeMs = 8; n.attackMs = 3; n.startDb = -26; n.endDb = 0; n.curve = 0.8;
                return applyNonLinear (base, n);
            }));

        L.push_back (def ("gated_short", "Short Gate", "Non-Linear", "gated (room base, flattened + gate)", 7103,
            "Tight, flat 220 ms gated ambience that fattens snares and claps with a punchy, modern burst.",
            [] (uint64_t seed) {
                auto s = makeSpace ({ 7.0, 5.5, 3.5 }, { 2.0, 2.0, 1.7, 1.0 }, seed, 2.4, 1.2);
                s.er.betaWall = 0.88; s.er.betaFloor = 0.9;
                s.erToLateDb = -1.0;
                s.lengthS = 0.5;
                s.eq = { { EqBand::HP, 80, 0.5412 }, { EqBand::HP, 80, 1.3066 }, { EqBand::LP, 15000, 0.707 } };
                auto base = renderSpace (s);
                NonLinParams n;
                n.shape = NonLinParams::Flat;
                n.gateMs = 220; n.fadeMs = 10; n.startDb = 0; n.endDb = -1.0; n.keepNaturalMs = 10;
                return applyNonLinear (base, n);
            }));

        L.push_back (def ("gated_big", "Big Gated Hall", "Non-Linear", "gated (hall base, flattened + gate)", 7104,
            "Enormous gated hall that holds for half a second then stops dead - stadium-sized '80s toms and snares.",
            [] (uint64_t seed) {
                auto s = makeSpace ({ 20.0, 14.0, 9.0 }, { 3.0, 3.0, 2.4, 1.4 }, seed, 6.0, 2.0, 32);
                s.erToLateDb = -5.0;
                s.lengthS = 0.9;
                s.eq = { { EqBand::HP, 60, 0.5412 }, { EqBand::HP, 60, 1.3066 }, { EqBand::LP, 14000, 0.707 } };
                auto base = renderSpace (s);
                NonLinParams n;
                n.shape = NonLinParams::Gated;
                n.gateMs = 500; n.fadeMs = 18; n.startDb = 0; n.endDb = -6.0; n.keepNaturalMs = 30;
                return applyNonLinear (base, n);
            }));

        L.push_back (def ("nonlin_flat", "Flat Non-Lin", "Non-Linear", "non-linear (dense FDN base, flat envelope + gate)", 7105,
            "Perfectly flat, dense non-linear burst of 280 ms with no early reflections - pure '80s machine gating.",
            [] (uint64_t seed) {
                FdnParams f;
                f.numLines = 32; f.minDelayMs = 3; f.maxDelayMs = 14;
                f.rtLow = 3.0; f.rtMid = 3.0; f.rtHigh = 2.6; f.rtAir = 1.6;
                f.diffStages = 8; f.diffMinMs = 0.4; f.diffMaxMs = 5; f.diffG = 0.7;
                f.loopAllpasses = 1; f.loopApMinMs = 0.3; f.loopApMaxMs = 1.5;
                f.seed = seed;
                auto base = renderFdnImpulse (f, kOutputRate, (size_t) (0.5 * kOutputRate));
                applyEq (base, { { EqBand::HP, 100, 0.5412 }, { EqBand::HP, 100, 1.3066 }, { EqBand::LP, 12500, 0.707 } });
                NonLinParams n;
                n.shape = NonLinParams::Flat;
                n.gateMs = 280; n.fadeMs = 8; n.attackMs = 2; n.startDb = 0; n.endDb = 0;
                return applyNonLinear (base, n);
            }));

        //==========================================================================================
        // FX
        //==========================================================================================
        L.push_back (def ("fx_reverse_clap", "Reverse Clap Swell", "FX", "reverse (modal plate base)", 8101,
            "Reversed plate that swells up after each hit and stops dead - the classic backwards snare and clap effect.",
            [] (uint64_t seed) {
                auto p = makePlate ({ 1.1, 1.3, 1.15, 0.7 }, seed);
                p.lengthS = 1.0;
                p.eq.push_back ({ EqBand::HighShelf, 5000, 0.7, 2.0 });
                auto base = renderPlate (p);
                ReverseParams r;
                r.swellS = 0.85; r.shapePower = 1.6; r.endFadeMs = 5;
                return makeReverse (base, r);
            }));

        L.push_back (def ("fx_bloom", "Bloom Cloud", "FX", "bloom (diffuse modulated FDN + slow-attack envelope)", 8102,
            "Slow-blooming, modulated cloud that swells behind the hit and turns single snares and toms into evolving pads.",
            [] (uint64_t seed) {
                FdnParams f;
                f.numLines = 32; f.minDelayMs = 25; f.maxDelayMs = 90;
                f.rtLow = 3.0; f.rtMid = 3.0; f.rtHigh = 2.3; f.rtAir = 1.3;
                f.diffStages = 8; f.diffMinMs = 3; f.diffMaxMs = 25; f.diffG = 0.72;
                f.loopAllpasses = 2; f.loopApMinMs = 2; f.loopApMaxMs = 9; f.loopApG = 0.6;
                f.modDepthMs = 0.5; f.modRateLo = 0.1; f.modRateHi = 0.4;
                f.seed = seed;
                auto base = renderFdnImpulse (f, kOutputRate, (size_t) (5.2 * kOutputRate));
                applyEq (base, { { EqBand::HP, 70, 0.707 }, { EqBand::HighShelf, 7000, 0.7, -2.0 } });
                BloomParams b;
                b.attackS = 0.32; b.power = 2.2;
                return makeBloom (base, b);
            }, fin (-85.0, -62.0)));

        L.push_back (def ("fx_metallic", "Metal Shimmer", "FX", "metallic (tuned comb resonators + bright FDN wash)", 8103,
            "Tuned metallic resonator wash that turns percussion into ringing, shimmering metal - great on rims and claps.",
            [] (uint64_t seed) {
                MetalParams m;
                m.seed = seed;
                m.pitchesHz = { 196.0, 293.66, 392.0, 466.16, 587.33, 698.46, 932.33, 1174.66, 1567.98 };
                m.rt60 = 1.5; m.hfDampHz = 6500; m.detuneCents = 6; m.modDepthCents = 4;
                m.diffuse.numLines = 16; m.diffuse.minDelayMs = 1.5; m.diffuse.maxDelayMs = 7;
                m.diffuse.rtLow = 0.9; m.diffuse.rtMid = 1.4; m.diffuse.rtHigh = 1.5; m.diffuse.rtAir = 1.0;
                m.diffuse.diffStages = 4; m.diffuse.diffMaxMs = 3;
                m.diffuse.modDepthMs = 0.02;
                m.diffuse.seed = seed + 1;
                m.diffuseLevel = 0.2;
                m.lengthS = 2.6;
                m.eq = { { EqBand::HP, 150, 0.707 }, { EqBand::HighShelf, 6000, 0.7, 2.0 }, { EqBand::LP, 12000, 0.707 } };
                return renderMetallic (m);
            }));

        L.push_back (def ("fx_reverse_hall", "Reverse Hall", "FX", "reverse (hall base)", 8104,
            "Long reversed hall that sucks into a hard stop - big cinematic reverse tom and snare transitions.",
            [] (uint64_t seed) {
                auto s = makeSpace ({ 22.0, 16.0, 10.0 }, { 2.4, 2.4, 1.9, 1.1 }, seed, 7.0, 2.0, 32);
                s.erToLateDb = -8.0;
                s.lengthS = 1.6;
                auto base = renderSpace (s);
                ReverseParams r;
                r.swellS = 1.4; r.shapePower = 1.3; r.endFadeMs = 6;
                return makeReverse (base, r);
            }));

        return L;
    }
} // namespace

const std::vector<IrDef>& factoryLibrary()
{
    static const std::vector<IrDef> lib = build();
    return lib;
}

std::array<int, 3> colourFor (const std::string& category, const std::string& id)
{
    std::array<int, 3> base { 160, 160, 160 };
    if (category == "Rooms") base = { 201, 142, 86 };
    else if (category == "Plates") base = { 196, 204, 218 };
    else if (category == "Chambers") base = { 138, 170, 150 };
    else if (category == "Halls") base = { 176, 58, 74 };
    else if (category == "Springs") base = { 228, 186, 64 };
    else if (category == "Vintage") base = { 84, 200, 188 };
    else if (category == "Non-Linear") base = { 232, 72, 160 };
    else if (category == "FX") base = { 142, 96, 232 };

    uint64_t h = 1469598103934665603ull;
    for (char c : id) { h ^= (uint8_t) c; h *= 1099511628211ull; }
    for (int i = 0; i < 3; ++i)
    {
        const int delta = (int) ((h >> (i * 8)) & 0x1F) - 16; // -16..15
        base[(size_t) i] = std::clamp (base[(size_t) i] + delta, 0, 255);
    }
    return base;
}

} // namespace irf
