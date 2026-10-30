#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace queen
{
// Sample-accurate Juno-style arpeggiator. No allocation: notes live in fixed arrays.
// Usage per segment: n = samplesUntilNextStep(); render n; advance(n); if due, step(emit).
class Arpeggiator
{
public:
    enum Mode { Up = 0, UpDown = 1, Down = 2 };

    struct Event
    {
        bool on;
        int note;
        float velocity;
    };

    void prepare (double sr);
    void reset();

    void setMode (int m) noexcept { mode = m; }
    void setRange (int octaves) noexcept { range = octaves < 1 ? 1 : (octaves > 3 ? 3 : octaves); }
    void setRate (double stepsPerSecond) noexcept { rate = stepsPerSecond; }
    void setHold (bool shouldHold) noexcept;
    void setKeyTranspose (bool on) noexcept { keyTranspose = on; }

    void noteOn (int note, float velocity) noexcept;
    void noteOff (int note) noexcept;

    bool isRunning() const noexcept { return count > 0; }
    int samplesUntilNextEvent() const noexcept;
    void advance (int n) noexcept;

    // Emits due events (note-off of the sounding note and/or the next note-on) via emit(Event).
    template <typename Emit>
    void processDue (Emit&& emit) noexcept
    {
        if (sounding >= 0 && samplesToGateOff <= 0.0)
        {
            emit (Event { false, sounding, 0.0f });
            sounding = -1;
        }
        if (count == 0)
        {
            if (sounding >= 0)
            {
                emit (Event { false, sounding, 0.0f });
                sounding = -1;
            }
            return;
        }
        if (samplesToStep <= 0.0)
        {
            if (sounding >= 0)
                emit (Event { false, sounding, 0.0f });
            sounding = nextNote();
            emit (Event { true, sounding, velocity });
            const double period = sampleRate / rate;
            samplesToStep += period;
            if (samplesToStep <= 0.0)
                samplesToStep = period;
            samplesToGateOff = period * 0.5;
        }
    }

    // test hook: the sequence produced from the current held notes, without timing
    int nextNote() noexcept;

    int soundingNote() const noexcept { return sounding; }

private:
    int patternLength() const noexcept;
    int patternNote (int index) const noexcept;

    double sampleRate = 44100.0, rate = 8.0;
    double samplesToStep = 0.0, samplesToGateOff = 0.0;
    int mode = Up, range = 1;
    bool hold = false, keyTranspose = false;

    std::array<int, 128> notes {}; // sorted ascending, count valid
    int count = 0;
    std::array<bool, 128> physicallyDown {};
    int physicalCount = 0;
    bool latchedAndReleased = false;
    int transpose = 0;
    int position = 0;
    int sounding = -1;
    float velocity = 0.8f;
};
} // namespace queen
