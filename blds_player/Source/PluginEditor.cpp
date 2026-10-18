#include "PluginEditor.h"

namespace
{
    constexpr int editorWidth  = 460;
    constexpr int editorHeight = 300;

    const juce::Colour background   { 0xff1c1e22 };
    const juce::Colour textColour   { 0xffe6e6e6 };
    const juce::Colour dimText      { 0xff9aa0a6 };
    const juce::Colour okGreen      { 0xff2ecc71 };
    const juce::Colour errorRed     { 0xffff5a52 };
    const juce::Colour missingGrey  { 0xff4a4e55 };

    juce::String utf8 (const char* s)    { return juce::String (juce::CharPointer_UTF8 (s)); }
    juce::String toJuce (const std::string& s) { return juce::String (juce::CharPointer_UTF8 (s.c_str())); }

    void styleLabel (juce::Label& l, float height, juce::Colour colour, bool bold = false)
    {
        l.setFont (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
        l.setColour (juce::Label::textColourId, colour);
        l.setJustificationType (juce::Justification::centredLeft);
        l.setInterceptsMouseClicks (false, false);
    }
}

BldsPlayerAudioProcessorEditor::BldsPlayerAudioProcessorEditor (BldsPlayerAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    titleLabel.setText (BLDS_EFFECT ? "BLDS Player FX" : "BLDS Player", juce::dontSendNotification);
    styleLabel (titleLabel, 14.0f, dimText, true);

    styleLabel (statusLabel, 24.0f, textColour, true);
    statusLabel.setMinimumHorizontalScale (0.6f);
    styleLabel (rateLabel, 14.0f, textColour);
    styleLabel (fileLabel, 14.0f, textColour);
    fileLabel.setMinimumHorizontalScale (0.5f);
    styleLabel (lengthLabel, 14.0f, textColour);

    ratesCaption.setText ("Available rates:", juce::dontSendNotification);
    styleLabel (ratesCaption, 13.0f, dimText);

    channelCaption.setText ("Output Channel", juce::dontSendNotification);
    styleLabel (channelCaption, 14.0f, textColour);

    hintLabel.setText (utf8 ("Live: track output \xe2\x86\x92 31/32, Right only \xe2\x86\x92 ch 32, 0.0 dB, pan centre"),
                       juce::dontSendNotification);
    styleLabel (hintLabel, 12.0f, dimText);

    for (auto* l : { &titleLabel, &statusLabel, &rateLabel, &fileLabel, &lengthLabel, &ratesCaption, &channelCaption, &hintLabel })
        addAndMakeVisible (*l);

    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (p.getValueTreeState().getParameter (BldsPlayerAudioProcessor::outputChannelParamID)))
        channelBox.addItemList (param->choices, 1);

    addAndMakeVisible (channelBox);
    channelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        p.getValueTreeState(), BldsPlayerAudioProcessor::outputChannelParamID, channelBox);

    setOpaque (true);
    setResizable (false, false);
    setSize (editorWidth, editorHeight);

    refresh (true);
    startTimerHz (4);
}

BldsPlayerAudioProcessorEditor::~BldsPlayerAudioProcessorEditor()
{
    stopTimer();
}

void BldsPlayerAudioProcessorEditor::timerCallback()
{
    refresh (false);
}

void BldsPlayerAudioProcessorEditor::refresh (bool force)
{
    Snapshot now;
    now.hostRate = processor.getHostRate();
    now.status   = processor.getStatus();
    now.active   = processor.getActiveTable();

    if (! force && now == shown)
        return;

    shown = now;

    using Status = BldsPlayerAudioProcessor::Status;
    const auto rateText = juce::String (now.hostRate) + " Hz";
    const auto* slotTable = processor.getLibrary().tableForRate (now.hostRate);

    juce::String statusText;
    juce::Colour statusColour = dimText;

    switch (now.status)
    {
        case Status::ok:
            statusText = utf8 ("BLDS OK \xe2\x80\x94 ") + rateText;
            statusColour = okGreen;
            break;

        case Status::noFile:
            statusText = "No BLDS file for " + rateText;
            statusColour = errorRed;
            break;

        case Status::invalidFile:
            statusText = "Invalid file: " + (slotTable != nullptr ? toJuce (slotTable->reason) : juce::String ("unknown"));
            statusColour = errorRed;
            break;

        case Status::waitingForHost:
        default:
            statusText = utf8 ("Waiting for host sample rate\xe2\x80\xa6");
            break;
    }

    statusLabel.setText (statusText, juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, statusColour);

    rateLabel.setText ("Detected sample rate: " + (now.hostRate > 0 ? rateText : juce::String (utf8 ("\xe2\x80\x94"))),
                       juce::dontSendNotification);

    if (now.active != nullptr)
    {
        fileLabel.setText ("Loaded file: " + toJuce (now.active->fileName), juce::dontSendNotification);
        lengthLabel.setText ("File length: " + juce::String ((juce::int64) now.active->length) + " samples",
                             juce::dontSendNotification);
    }
    else if (slotTable != nullptr && slotTable->state == blds::Table::State::invalid)
    {
        fileLabel.setText ("Rejected file: " + toJuce (slotTable->fileName) + " (" + toJuce (slotTable->reason) + ")",
                           juce::dontSendNotification);
        lengthLabel.setText (utf8 ("File length: \xe2\x80\x94 (outputting silence)"), juce::dontSendNotification);
    }
    else
    {
        fileLabel.setText (utf8 ("Loaded file: \xe2\x80\x94 (outputting silence)"), juce::dontSendNotification);
        lengthLabel.setText (utf8 ("File length: \xe2\x80\x94"), juce::dontSendNotification);
    }

    repaint (rateIndicatorArea());
}

juce::Rectangle<int> BldsPlayerAudioProcessorEditor::rateIndicatorArea() const
{
    return { 16, 168, editorWidth - 32, 34 };
}

void BldsPlayerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);

    const auto area = rateIndicatorArea().toFloat();
    const auto& lib = processor.getLibrary();
    const float gap = 6.0f;
    const float w = (area.getWidth() - gap * (float) (blds::numRates - 1)) / (float) blds::numRates;

    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));

    for (int i = 0; i < blds::numRates; ++i)
    {
        const auto& t = lib.slot (i);
        const auto r = juce::Rectangle<float> (area.getX() + (float) i * (w + gap), area.getY(), w, area.getHeight());

        const auto fill = t.state == blds::Table::State::valid   ? okGreen
                        : t.state == blds::Table::State::invalid ? errorRed
                                                                 : missingGrey;

        g.setColour (fill.withAlpha (0.85f));
        g.fillRoundedRectangle (r, 5.0f);

        if (t.rate == shown.hostRate)
        {
            g.setColour (juce::Colours::white);
            g.drawRoundedRectangle (r.reduced (1.0f), 5.0f, 2.0f);
        }

        g.setColour (t.state == blds::Table::State::missing ? dimText : juce::Colours::black);
        g.drawFittedText (juce::String (t.rate / 1000.0, t.rate % 1000 == 0 ? 0 : 1) + "k",
                          r.toNearestInt(), juce::Justification::centred, 1);
    }
}

void BldsPlayerAudioProcessorEditor::resized()
{
    titleLabel.setBounds     (16, 8, editorWidth - 32, 18);
    statusLabel.setBounds    (16, 28, editorWidth - 32, 34);
    rateLabel.setBounds      (16, 70, editorWidth - 32, 20);
    fileLabel.setBounds      (16, 92, editorWidth - 32, 20);
    lengthLabel.setBounds    (16, 114, editorWidth - 32, 20);
    ratesCaption.setBounds   (16, 146, editorWidth - 32, 18);
    channelCaption.setBounds (16, 218, 130, 26);
    channelBox.setBounds     (150, 218, editorWidth - 166, 26);
    hintLabel.setBounds      (16, 260, editorWidth - 32, 24);
}
