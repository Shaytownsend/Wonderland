#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// Tiny fixed-size status UI. Reads processor state via atomics only and refreshes
// at 4 Hz; it only repaints when something actually changed.
class BldsPlayerAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::Timer
{
public:
    explicit BldsPlayerAudioProcessorEditor (BldsPlayerAudioProcessor&);
    ~BldsPlayerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Snapshot
    {
        int hostRate = -1;
        BldsPlayerAudioProcessor::Status status = BldsPlayerAudioProcessor::Status::waitingForHost;
        const blds::Table* active = nullptr;

        bool operator== (const Snapshot& o) const noexcept
        {
            return hostRate == o.hostRate && status == o.status && active == o.active;
        }
    };

    void timerCallback() override;
    void refresh (bool force);
    juce::Rectangle<int> rateIndicatorArea() const;

    BldsPlayerAudioProcessor& bldsProcessor;
    Snapshot shown;

    juce::Label titleLabel, statusLabel, rateLabel, fileLabel, lengthLabel, ratesCaption, channelCaption, hintLabel;
    juce::ComboBox channelBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> channelAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BldsPlayerAudioProcessorEditor)
};
