#pragma once

#include "Plugin/TonePageLayout.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Pure UI helper, unit-testable without a GUI: the item labels a ComboBox must
// show for a discrete parameter. JUCE's ComboBoxAttachment only maps the
// selected index to the parameter value, so the box has to be filled first.
namespace TonePageUi
{
juce::StringArray comboChoices (const juce::AudioProcessorParameter& parameter);
}

// Tone 1 tab: WG | TVF | TVA | LFO | ENV | CTRL subpages built from the pure
// TonePage::kTone1Layout table (123 tone1.* parameters). Every control is a
// standard juce widget bound to the APVTS with a *Attachment; ranges and choice
// lists come from the parameter objects, never from hard-coded UI data
// (M2-09 D3). Custom skins and display mapping land in M5-02/M5-07.
class TonePageView final : public juce::Component
{
public:
    explicit TonePageView (juce::AudioProcessorValueTreeState& state);
    ~TonePageView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    class SubPage;
    class Control;

    juce::AudioProcessorValueTreeState& apvts;

    // Destruction order: the tabs are declared last so they are destroyed first,
    // while the subpages they point at are still alive.
    juce::OwnedArray<SubPage> subPages;
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
};
