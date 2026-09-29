#pragma once

#include "Plugin/ZoneDraft.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

// Property panel for the selected draft zone (architecture 6.2): root key,
// tuning, gain, pan, loop mode, reverse and the key/velocity ranges.
class ZonePropertiesPanel final : public juce::Component,
                                  private juce::ChangeListener
{
public:
    explicit ZonePropertiesPanel (ZoneDraft&);
    ~ZonePropertiesPanel() override;

    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void pushEdits();
    void refreshFromDraft();
    void configureSlider (juce::Slider& slider, double minimum, double maximum, double interval, double defaultValue);

    ZoneDraft& draft;
    bool updating = false;

    juce::Slider keyLow, keyHigh, velLow, velHigh, rootKey, coarseTune, fineTune, gainDb, pan;
    juce::ComboBox loopMode;
    juce::ToggleButton reverse { "Reverse" };

    std::array<juce::Label, 11> labels;
    std::array<juce::Component*, 11> controls;
};
