#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Plugin/SamplerPanel.h"
#include "Plugin/TonePage.h"

class MyJVProcessor;

// Top-level editor: a tabbed host with the Sampler panel (extracted unchanged)
// and the Tone 1 edit page. Patch Common / Tone 2-4 / Keyboard tabs land in
// M3-03, M3-04 and M5.
class MyJVEditor final : public juce::AudioProcessorEditor
{
public:
    explicit MyJVEditor (MyJVProcessor&);
    ~MyJVEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    MyJVProcessor& processor;
    SamplerPanel samplerPanel;
    TonePageView tonePage;

    // Destruction order: the tabs are declared last so they are destroyed first,
    // while the pages they point at are still alive.
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop };
};
