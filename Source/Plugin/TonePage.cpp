#include "Plugin/TonePage.h"

namespace
{
juce::String tone1ParameterId (const TonePage::Entry& entry)
{
    return "tone1." + juce::String (entry.suffix);
}

juce::String tone1ParameterName (juce::AudioProcessorValueTreeState& state, const TonePage::Entry& entry)
{
    if (auto* parameter = state.getParameter (tone1ParameterId (entry)))
        return parameter->getName (100);

    return tone1ParameterId (entry);
}
}

// One labelled control: the widget type follows the layout table, the value is
// bound through an attachment and the range/choices come from the parameter.
// Members are declared widget-first so the attachment members are destroyed
// before the widgets they listen to (M2-09 session-brief trap 1).
class TonePageView::Control final : public juce::Component
{
public:
    Control (juce::AudioProcessorValueTreeState& state, const TonePage::Entry& entry)
    {
        const auto id = tone1ParameterId (entry);

        label.setText (tone1ParameterName (state, entry), juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        label.setFont (juce::Font (juce::FontOptions (12.0f)));
        addAndMakeVisible (label);

        switch (entry.kind)
        {
            case TonePage::ControlKind::Slider:
                slider = std::make_unique<juce::Slider>();
                slider->setSliderStyle (juce::Slider::Rotary);
                slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 16);
                addAndMakeVisible (*slider);
                sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                    state, id, *slider);
                break;

            case TonePage::ControlKind::Combo:
                combo = std::make_unique<juce::ComboBox>();
                addAndMakeVisible (*combo);
                comboAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                    state, id, *combo);
                break;

            case TonePage::ControlKind::Toggle:
                toggle = std::make_unique<juce::ToggleButton>();
                addAndMakeVisible (*toggle);
                buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                    state, id, *toggle);
                break;
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (3);
        label.setBounds (area.removeFromTop (16));

        if (slider != nullptr)
        {
            slider->setBounds (area);
            return;
        }

        auto row = area.removeFromTop (juce::jmin (26, area.getHeight()));

        if (combo != nullptr)
            combo->setBounds (row);
        else if (toggle != nullptr)
            toggle->setBounds (row.withSizeKeepingCentre (juce::jmin (row.getWidth(), 28), 24));
    }

private:
    juce::Label label;
    std::unique_ptr<juce::Slider> slider;
    std::unique_ptr<juce::ComboBox> combo;
    std::unique_ptr<juce::ToggleButton> toggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;
};

// A subpage lays its controls out as a grid that adapts to the available area.
class TonePageView::SubPage final : public juce::Component
{
public:
    SubPage (juce::AudioProcessorValueTreeState& state, TonePage::SubPage page)
    {
        for (const auto& entry : TonePage::kTone1Layout)
            if (entry.subPage == page)
                addAndMakeVisible (controls.add (new Control (state, entry)));
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff1d2025));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (8);
        const auto count = controls.size();

        if (count <= 0 || area.isEmpty())
            return;

        const auto columns = juce::jmax (1, area.getWidth() / kMinCellWidth);
        const auto rows = (count + columns - 1) / columns;
        const auto cellWidth = area.getWidth() / columns;
        const auto cellHeight = juce::jmin (kMaxCellHeight, juce::jmax (1, area.getHeight() / rows));

        for (int index = 0; index < count; ++index)
            controls.getUnchecked (index)->setBounds (area.getX() + (index % columns) * cellWidth,
                                                      area.getY() + (index / columns) * cellHeight,
                                                      cellWidth, cellHeight);
    }

private:
    static constexpr int kMinCellWidth = 110;
    static constexpr int kMaxCellHeight = 112;

    juce::OwnedArray<Control> controls;
};

TonePageView::TonePageView (juce::AudioProcessorValueTreeState& state)
    : apvts (state)
{
    addAndMakeVisible (tabs);
    tabs.setTabBarDepth (26);
    tabs.setOutline (0);

    const auto addSubPage = [this] (const juce::String& name, TonePage::SubPage page)
    {
        auto* pageComponent = subPages.add (new SubPage (apvts, page));
        tabs.addTab (name, juce::Colours::darkgrey, pageComponent, false);
    };

    addSubPage ("WG", TonePage::SubPage::Wg);
    addSubPage ("TVF", TonePage::SubPage::Tvf);
    addSubPage ("TVA", TonePage::SubPage::Tva);
    addSubPage ("LFO", TonePage::SubPage::Lfo);
    addSubPage ("ENV", TonePage::SubPage::Env);
    addSubPage ("CTRL", TonePage::SubPage::Ctrl);
}

TonePageView::~TonePageView() = default;

void TonePageView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff23262b));
}

void TonePageView::resized()
{
    tabs.setBounds (getLocalBounds());
}
