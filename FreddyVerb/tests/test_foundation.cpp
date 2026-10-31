#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "PluginProcessor.h"
#include "state/Parameters.h"
#include "TestHelpers.h"

using namespace fv;

TEST_CASE ("Every parameter ID in pid:: exists and has info", "[params]")
{
    FreddyVerbProcessor p;
    auto& s = p.getAPVTS();
    int count = 0;
    for (auto* param : p.getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param);
        REQUIRE (rp != nullptr);
        INFO (rp->getParameterID());
        CHECK (findParamInfo (rp->getParameterID()) != nullptr);
        CHECK (rp->isAutomatable());
        ++count;
    }
    CHECK (count == (int) parameterInfo().size());
    CHECK (s.getParameter (pid::duckDepth) != nullptr);
}

TEST_CASE ("Init defaults match the spec's Init preset", "[params]")
{
    FreddyVerbProcessor p;
    auto v = [&] (const char* id) { return p.getAPVTS().getRawParameterValue (id)->load(); };
    CHECK (v (pid::duckMode) == 0.0f);                     // Follow
    CHECK_THAT (v (pid::duckDepth), Catch::Matchers::WithinAbs (-18.0, 1e-4));
    CHECK_THAT (v (pid::duckRelease), Catch::Matchers::WithinAbs (250.0, 1e-3));
    CHECK_THAT (v (pid::gateHold), Catch::Matchers::WithinAbs (150.0, 1e-3));
    CHECK_THAT (v (pid::gateRelease), Catch::Matchers::WithinAbs (300.0, 1e-3));
    CHECK_THAT (v (pid::mix), Catch::Matchers::WithinAbs (35.0, 1e-4));
    CHECK (v (pid::eqPosition) == 1.0f);                   // Post
    CHECK (v (pid::quality) == 0.0f);                      // Zero latency
}

TEST_CASE ("State round-trip restores every parameter and UI state", "[state]")
{
    FreddyVerbProcessor a;
    juce::Random rng (1234);
    for (auto* param : a.getParameters())
        param->setValueNotifyingHost (rng.nextFloat());
    a.getUIState().setProperty ("theme", "night_session", nullptr);

    juce::MemoryBlock mb;
    a.getStateInformation (mb);

    FreddyVerbProcessor b;
    b.setStateInformation (mb.getData(), (int) mb.getSize());
    for (int i = 0; i < a.getParameters().size(); ++i)
    {
        auto* pa = a.getParameters()[i];
        auto* pb = b.getParameters()[i];
        INFO (pa->getName (64));
        if (dynamic_cast<juce::AudioParameterFloat*> (pa) != nullptr)
            CHECK_THAT (pb->getValue(), Catch::Matchers::WithinAbs (pa->getValue(), 1e-6));
        else
            CHECK (pb->getCurrentValueAsText() == pa->getCurrentValueAsText());
    }
    CHECK (b.getUIState().getProperty ("theme").toString() == "night_session");
}

TEST_CASE ("State with unknown future version still loads parameters", "[state]")
{
    FreddyVerbProcessor a;
    a.getAPVTS().getParameter (pid::mix)->setValueNotifyingHost (0.8f);
    juce::MemoryBlock mb;
    a.getStateInformation (mb);
    auto xml = juce::AudioProcessor::getXmlFromBinary (mb.getData(), (int) mb.getSize());
    REQUIRE (xml != nullptr);
    xml->setAttribute ("version", 99);
    juce::MemoryBlock mb2;
    juce::AudioProcessor::copyXmlToBinary (*xml, mb2);
    FreddyVerbProcessor b;
    b.setStateInformation (mb2.getData(), (int) mb2.getSize());
    CHECK_THAT (b.getAPVTS().getParameter (pid::mix)->getValue(), Catch::Matchers::WithinAbs (0.8, 1e-6));
}

TEST_CASE ("Bus layouts: mono->stereo and stereo->stereo only", "[layout]")
{
    FreddyVerbProcessor p;
    juce::AudioProcessor::BusesLayout l;
    l.inputBuses.add (juce::AudioChannelSet::mono());
    l.outputBuses.add (juce::AudioChannelSet::stereo());
    CHECK (p.checkBusesLayoutSupported (l));
    l.inputBuses.getReference (0) = juce::AudioChannelSet::stereo();
    CHECK (p.checkBusesLayoutSupported (l));
    l.outputBuses.getReference (0) = juce::AudioChannelSet::mono();
    CHECK_FALSE (p.checkBusesLayoutSupported (l));
}

TEST_CASE ("State restore re-syncs bool/choice parameters holding fractional host values", "[state]")
{
    // pluginval's randomised state test: values are set to arbitrary normalised floats
    FreddyVerbProcessor p;
    auto* b = p.getAPVTS().getParameter (pid::gateOn);
    b->setValueNotifyingHost (0.96f);
    juce::MemoryBlock mb;
    p.getStateInformation (mb);
    b->setValueNotifyingHost (0.85f);          // same meaning ("on"), different raw value
    p.setStateInformation (mb.getData(), (int) mb.getSize());
    CHECK (std::abs (b->getValue() - 0.96f) < 0.1f);
}

