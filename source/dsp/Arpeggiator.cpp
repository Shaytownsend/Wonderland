#include "Arpeggiator.h"

namespace queen
{
void Arpeggiator::prepare (double sr)
{
    sampleRate = sr;
    reset();
}

void Arpeggiator::reset()
{
    count = 0;
    physicalCount = 0;
    physicallyDown.fill (false);
    latchedAndReleased = false;
    transpose = 0;
    position = 0;
    sounding = -1;
    samplesToStep = samplesToGateOff = 0.0;
}

void Arpeggiator::setHold (bool shouldHold) noexcept
{
    if (hold && ! shouldHold)
    {
        // releasing HOLD drops notes no longer physically held
        int w = 0;
        for (int i = 0; i < count; ++i)
            if (physicallyDown[(size_t) notes[(size_t) i]])
                notes[(size_t) w++] = notes[(size_t) i];
        count = w;
        transpose = 0;
    }
    hold = shouldHold;
}

void Arpeggiator::noteOn (int note, float vel) noexcept
{
    if (note < 0 || note > 127)
        return;
    velocity = vel;

    if (hold && latchedAndReleased)
    {
        if (keyTranspose && count > 0)
        {
            // latched pattern + KEY TR: a new key transposes relative to the lowest latched note
            transpose = note - notes[0];
            if (! physicallyDown[(size_t) note])
            {
                physicallyDown[(size_t) note] = true;
                ++physicalCount;
            }
            return;
        }
        // first key after releasing everything starts a new latched chord
        count = 0;
        transpose = 0;
        latchedAndReleased = false;
    }

    if (! physicallyDown[(size_t) note])
    {
        physicallyDown[(size_t) note] = true;
        ++physicalCount;
    }

    const bool wasEmpty = count == 0;
    for (int i = 0; i < count; ++i)
        if (notes[(size_t) i] == note)
            return;
    int i = count++;
    while (i > 0 && notes[(size_t) i - 1] > note)
    {
        notes[(size_t) i] = notes[(size_t) i - 1];
        --i;
    }
    notes[(size_t) i] = note;

    if (wasEmpty)
    {
        position = 0;
        samplesToStep = 0.0; // first step immediately
    }
}

void Arpeggiator::noteOff (int note) noexcept
{
    if (note < 0 || note > 127 || ! physicallyDown[(size_t) note])
        return;
    physicallyDown[(size_t) note] = false;
    --physicalCount;

    if (hold)
    {
        if (physicalCount == 0)
            latchedAndReleased = true;
        return;
    }
    for (int i = 0; i < count; ++i)
    {
        if (notes[(size_t) i] == note)
        {
            for (int j = i; j < count - 1; ++j)
                notes[(size_t) j] = notes[(size_t) j + 1];
            --count;
            break;
        }
    }
}

int Arpeggiator::samplesUntilNextEvent() const noexcept
{
    double next = 1.0e9;
    if (count > 0)
        next = samplesToStep;
    if (sounding >= 0 && samplesToGateOff < next)
        next = samplesToGateOff;
    if (sounding >= 0 && count == 0)
        next = 0.0;
    const int n = (int) std::ceil (next);
    return n < 0 ? 0 : n;
}

void Arpeggiator::advance (int n) noexcept
{
    if (count > 0)
        samplesToStep -= n;
    if (sounding >= 0)
        samplesToGateOff -= n;
}

int Arpeggiator::patternLength() const noexcept
{
    const int up = count * range;
    if (mode == UpDown)
        return up > 1 ? 2 * up - 2 : 1;
    return up;
}

int Arpeggiator::patternNote (int index) const noexcept
{
    const int up = count * range;
    int k = index;
    if (mode == Down)
        k = up - 1 - index;
    else if (mode == UpDown && index >= up)
        k = 2 * up - 2 - index;
    return notes[(size_t) (k % count)] + 12 * (k / count) + transpose;
}

int Arpeggiator::nextNote() noexcept
{
    if (count == 0)
        return -1;
    const int len = patternLength();
    if (position >= len)
        position = 0;
    const int n = patternNote (position);
    position = (position + 1) % len;
    return n < 0 ? 0 : (n > 127 ? 127 : n);
}
} // namespace queen
