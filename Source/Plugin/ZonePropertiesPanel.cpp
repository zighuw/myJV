#include "Plugin/ZonePropertiesPanel.h"

#include <algorithm>

ZonePropertiesPanel::ZonePropertiesPanel (ZoneDraft& ownerDraft)
    : draft (ownerDraft),
      controls { &keyLow, &keyHigh, &velLow, &velHigh, &rootKey, &coarseTune, &fineTune, &gainDb, &pan,
                 &loopMode, &reverse }
{
    configureSlider (keyLow, kMidiNoteMin, kMidiNoteMax, 1.0, 0.0);
    configureSlider (keyHigh, kMidiNoteMin, kMidiNoteMax, 1.0, 127.0);
    configureSlider (velLow, kMidiVelocityMin, kMidiVelocityMax, 1.0, 1.0);
    configureSlider (velHigh, kMidiVelocityMin, kMidiVelocityMax, 1.0, 127.0);
    configureSlider (rootKey, -1.0, kMidiNoteMax, 1.0, -1.0);
    configureSlider (coarseTune, -24.0, 24.0, 1.0, 0.0);
    configureSlider (fineTune, -100.0, 100.0, 1.0, 0.0);
    configureSlider (gainDb, -24.0, 24.0, 0.1, 0.0);
    configureSlider (pan, 0.0, 127.0, 1.0, 64.0);

    loopMode.addItemList ({ "Off", "Forward", "Sustain" }, 1);

    const char* const labelTexts[] { "Key Lo", "Key Hi", "Vel Lo", "Vel Hi", "Root Key",
                                     "Coarse", "Fine", "Gain dB", "Pan", "Loop", "" };

    for (std::size_t i = 0; i < controls.size(); ++i)
    {
        labels[i].setText (labelTexts[i], juce::dontSendNotification);
        labels[i].setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (labels[i]);
        addAndMakeVisible (*controls[i]);
    }

    for (auto* slider : { &keyLow, &keyHigh, &velLow, &velHigh, &rootKey, &coarseTune,
                          &fineTune, &gainDb, &pan })
        slider->onValueChange = [this] { pushEdits(); };

    loopMode.onChange = [this] { pushEdits(); };
    reverse.onClick = [this] { pushEdits(); };

    draft.addChangeListener (this);
    refreshFromDraft();
}

ZonePropertiesPanel::~ZonePropertiesPanel()
{
    draft.removeChangeListener (this);
}

void ZonePropertiesPanel::configureSlider (juce::Slider& slider, double minimum, double maximum,
                                           double interval, double defaultValue)
{
    slider.setSliderStyle (juce::Slider::LinearBar);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 18);
    slider.setRange (minimum, maximum, interval);
    slider.setDoubleClickReturnValue (true, defaultValue);
}

void ZonePropertiesPanel::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshFromDraft();
}

void ZonePropertiesPanel::resized()
{
    auto area = getLocalBounds().reduced (4);
    const auto rows = (int) (controls.size() + 1) / 2;
    const auto rowHeight = rows > 0 ? juce::jmax (18, area.getHeight() / rows) : area.getHeight();
    const auto columnWidth = area.getWidth() / 2;

    for (std::size_t i = 0; i < controls.size(); ++i)
    {
        const auto column = (int) i / rows;
        const auto row = (int) i % rows;
        auto cell = juce::Rectangle<int> (area.getX() + column * columnWidth,
                                          area.getY() + row * rowHeight,
                                          columnWidth, rowHeight - 2);

        labels[i].setBounds (cell.removeFromLeft (64));
        controls[i]->setBounds (cell.reduced (2, 1));
    }
}

void ZonePropertiesPanel::pushEdits()
{
    if (updating)
        return;

    const auto index = draft.getSelectedIndex();
    const auto* selected = draft.getSelectedZone();

    if (selected == nullptr || index < 0)
        return;

    auto zone = *selected;
    zone.keyLow = (int) keyLow.getValue();
    zone.keyHigh = (int) keyHigh.getValue();
    zone.velLow = (int) velLow.getValue();
    zone.velHigh = (int) velHigh.getValue();
    zone.rootKeyOverride = (int) rootKey.getValue();
    zone.coarseTune = (float) coarseTune.getValue();
    zone.fineTune = (float) fineTune.getValue();
    zone.gainDb = (float) gainDb.getValue();
    zone.pan = (int) pan.getValue();
    zone.loopMode = (LoopMode) juce::jmax (0, loopMode.getSelectedId() - 1);
    zone.reverse = reverse.getToggleState();

    if (zone.keyLow > zone.keyHigh)
        std::swap (zone.keyLow, zone.keyHigh);

    if (zone.velLow > zone.velHigh)
        std::swap (zone.velLow, zone.velHigh);

    draft.updateZone (index, zone);
}

void ZonePropertiesPanel::refreshFromDraft()
{
    updating = true;

    const auto* selected = draft.getSelectedZone();
    const auto enabled = selected != nullptr;

    for (auto* control : controls)
        control->setEnabled (enabled);

    if (selected != nullptr)
    {
        keyLow.setValue (selected->keyLow, juce::dontSendNotification);
        keyHigh.setValue (selected->keyHigh, juce::dontSendNotification);
        velLow.setValue (selected->velLow, juce::dontSendNotification);
        velHigh.setValue (selected->velHigh, juce::dontSendNotification);
        rootKey.setValue (selected->rootKeyOverride, juce::dontSendNotification);
        coarseTune.setValue (selected->coarseTune, juce::dontSendNotification);
        fineTune.setValue (selected->fineTune, juce::dontSendNotification);
        gainDb.setValue (selected->gainDb, juce::dontSendNotification);
        pan.setValue (selected->pan, juce::dontSendNotification);
        loopMode.setSelectedId ((int) selected->loopMode + 1, juce::dontSendNotification);
        reverse.setToggleState (selected->reverse, juce::dontSendNotification);
    }

    updating = false;
}
