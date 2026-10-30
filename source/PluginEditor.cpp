#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p) : AudioProcessorEditor (&p), processorRef (p)
{
    setSize (900, 415);
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff3a3a3a));
    g.setColour (juce::Colours::white);
    g.drawFittedText ("The Queen", getLocalBounds(), juce::Justification::centred, 1);
}
