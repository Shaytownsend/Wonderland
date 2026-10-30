#include <Parameters.h>
#include <catch2/catch_test_macros.hpp>
#include <set>

static juce::var loadRegistry()
{
    juce::File f (juce::String (QUEEN_SOURCE_DIR) + "/queen_blender/tex2/J.json");
    REQUIRE (f.existsAsFile());
    return juce::JSON::parse (f);
}

TEST_CASE ("Parameter table is sane", "[params]")
{
    std::set<std::string_view> ids;
    for (const auto& d : queen::kParams)
    {
        INFO (std::string (d.id));
        CHECK (ids.insert (d.id).second); // no duplicate ids
        CHECK (d.min < d.max);
        CHECK (d.def >= d.min);
        CHECK (d.def <= d.max);
        if (d.kind == queen::Kind::Choice)
        {
            CHECK (queen::choicesOf (d).size() == (int) d.max + 1);
            CHECK (d.def == std::floor (d.def));
        }
    }
    CHECK (queen::indexOf ("op4_feedback") >= 0);
    CHECK (queen::def ("op4_feedback").def == 0.3f);
}

TEST_CASE ("Every registry control has a parameter and vice versa", "[params]")
{
    auto reg = loadRegistry();
    auto* controls = reg["controls"].getArray();
    REQUIRE (controls != nullptr);

    std::set<std::string> referenced;
    int attachable = 0;
    for (auto& c : *controls)
    {
        auto t = c["t"].toString();
        if (! (t == "knob" || t == "slider" || t == "switch" || t == "button"))
            continue;
        ++attachable;
        auto param = c["param"].toString();
        INFO (c["id"].toString().toStdString());
        if (t == "button" && param.isEmpty())
        {
            CHECK (c["action"].toString().isNotEmpty()); // memory buttons are state, not params
            continue;
        }
        REQUIRE (queen::indexOf (param.toStdString()) >= 0);
        referenced.insert (param.toStdString());

        const auto& d = queen::def (param.toStdString());
        if (t == "switch")
            CHECK (d.kind == queen::Kind::Choice);
        if (c.hasProperty ("value"))
            CHECK (d.kind == queen::Kind::Choice);
    }
    CHECK (attachable == 21 + 21 + 7 + 38);

    for (const auto& d : queen::kParams)
    {
        INFO (std::string (d.id));
        CHECK (referenced.contains (std::string (d.id)) == d.onPanel);
    }
}

TEST_CASE ("APVTS layout builds from the table", "[params]")
{
    auto layout = queen::createParameterLayout();
    juce::AudioProcessorGraph dummyHost; // any AudioProcessor works as an owner
    juce::AudioProcessorValueTreeState apvts (dummyHost, nullptr, "QUEEN", std::move (layout));
    for (const auto& d : queen::kParams)
    {
        auto* p = apvts.getParameter (queen::toString (d.id));
        REQUIRE (p != nullptr);
        auto plain = p->convertFrom0to1 (p->getDefaultValue());
        CHECK (std::abs (plain - d.def) < 1e-3f * (d.max - d.min));
    }
}
