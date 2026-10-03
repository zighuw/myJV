#include "Plugin/MyJVEditor.h"

#include "Plugin/MyJVProcessor.h"

MyJVEditor::MyJVEditor (MyJVProcessor& ownerProcessor)
    : juce::AudioProcessorEditor (&ownerProcessor),
      processor (ownerProcessor),
      samplerPanel (ownerProcessor),
      tonePage (ownerProcessor.getApvts())
{
    setSize (900, 700);

    addAndMakeVisible (tabs);
    tabs.setTabBarDepth (26);
    tabs.setOutline (0);
    tabs.addTab ("Sampler", juce::Colours::darkgrey, &samplerPanel, false);
    tabs.addTab ("Tone 1", juce::Colours::darkgrey, &tonePage, false);
}

MyJVEditor::~MyJVEditor() = default;

void MyJVEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff23262b));
}

void MyJVEditor::resized()
{
    tabs.setBounds (getLocalBounds());
}
