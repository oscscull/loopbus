#include "PluginProcessor.h"
#include "LoopBus.h"
#include <cmath>

LoopBusProcessor::LoopBusProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      params(*this, nullptr, "LoopBusParams", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout LoopBusProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> layout;

    // Mode: 0 = Send, 1 = Return
    layout.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{kModeParamID, 1},
        "Mode",
        juce::StringArray{"Send", "Return"},
        0
    ));

    // Channel: 1-16
    layout.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{kChannelParamID, 1},
        "Channel",
        1, LoopBus::kNumBuses, 1
    ));

    // Feedback attenuation in dB (only used in Return mode)
    // -inf to 0 dB, where more negative = faster decay
    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{kFeedbackParamID, 1},
        "Feedback (dB)",
        juce::NormalisableRange<float>(-60.0f, -0.5f, 0.1f),
        -6.0f,  // default: -6 dB per lap
        juce::String{},
        juce::AudioProcessorParameter::genericParameter,
        [](float v, int) { return juce::String(v, 1) + " dB"; },
        [](const juce::String& s) { return s.trimCharactersAtEnd(" dB").getFloatValue(); }
    ));

    // Limiter ceiling in dB (hard clip threshold)
    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{kLimitParamID, 1},
        "Limit (dB)",
        juce::NormalisableRange<float>(-12.0f, 0.0f, 0.1f),
        0.0f,
        juce::String{},
        juce::AudioProcessorParameter::genericParameter,
        [](float v, int) { return juce::String(v, 1) + " dB"; },
        [](const juce::String& s) { return s.trimCharactersAtEnd(" dB").getFloatValue(); }
    ));

    return {layout.begin(), layout.end()};
}

void LoopBusProcessor::prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/)
{
}

void LoopBusProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    int mode = (int)params.getRawParameterValue(kModeParamID)->load();
    int channel = (int)params.getRawParameterValue(kChannelParamID)->load() - 1;
    auto& bus = LoopBus::getInstance(channel);
    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    if (mode == 0)
    {
        // === SEND MODE ===
        const float* channels[2] = {
            buffer.getReadPointer(0),
            numChannels > 1 ? buffer.getReadPointer(1) : buffer.getReadPointer(0)
        };
        bus.write(channels, numSamples, 2);
    }
    else
    {
        // === RETURN MODE ===
        float feedbackDb = params.getRawParameterValue(kFeedbackParamID)->load();
        float feedbackGain = std::pow(10.0f, feedbackDb / 20.0f);

        float limitDb = params.getRawParameterValue(kLimitParamID)->load();
        float limitLin = std::pow(10.0f, limitDb / 20.0f);

        float tempL[LoopBus::kMaxBlockSize];
        float tempR[LoopBus::kMaxBlockSize];
        float* tempPtrs[2] = {tempL, tempR};

        int readSamples = bus.read(tempPtrs, numSamples, 2);

        if (readSamples > 0)
        {
            for (int c = 0; c < numChannels && c < 2; ++c)
            {
                float* out = buffer.getWritePointer(c);
                for (int i = 0; i < readSamples; ++i)
                {
                    float sample = tempPtrs[c][i] * feedbackGain;
                    sample = std::clamp(sample, -limitLin, limitLin);
                    out[i] += sample;
                }
            }
        }

        // Safety clip on entire output
        for (int c = 0; c < numChannels; ++c)
        {
            float* out = buffer.getWritePointer(c);
            for (int i = 0; i < numSamples; ++i)
                out[i] = std::clamp(out[i], -limitLin, limitLin);
        }
    }
}

//==============================================================================
// Custom editor that hides feedback/limit when in Send mode

class LoopBusEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    LoopBusEditor(LoopBusProcessor& p, juce::AudioProcessorValueTreeState& vts)
        : AudioProcessorEditor(&p), processor(p), valueTreeState(vts)
    {
        // Mode combo box
        modeLabel.setText("Mode", juce::dontSendNotification);
        modeLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(modeLabel);

        modeBox.addItem("Send", 1);
        modeBox.addItem("Return", 2);
        modeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            vts, LoopBusProcessor::kModeParamID, modeBox);
        addAndMakeVisible(modeBox);

        // Channel slider
        channelLabel.setText("Channel", juce::dontSendNotification);
        channelLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(channelLabel);

        channelSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        channelSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 40, 20);
        channelSlider.setNormalisableRange(juce::NormalisableRange<double>(1.0, 16.0, 1.0));
        channelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            vts, LoopBusProcessor::kChannelParamID, channelSlider);
        addAndMakeVisible(channelSlider);

        // Feedback slider
        feedbackLabel.setText("Feedback", juce::dontSendNotification);
        feedbackLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(feedbackLabel);

        feedbackSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        feedbackSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
        feedbackSlider.setTextValueSuffix(" dB");
        feedbackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            vts, LoopBusProcessor::kFeedbackParamID, feedbackSlider);
        addAndMakeVisible(feedbackSlider);

        // Limit slider
        limitLabel.setText("Limit", juce::dontSendNotification);
        limitLabel.setJustificationType(juce::Justification::centredRight);
        addAndMakeVisible(limitLabel);

        limitSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        limitSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 80, 20);
        limitSlider.setTextValueSuffix(" dB");
        limitAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            vts, LoopBusProcessor::kLimitParamID, limitSlider);
        addAndMakeVisible(limitSlider);

        setSize(360, 165);
        startTimerHz(10);
        updateVisibility();
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10);
        int rowH = 30;
        int labelW = 80;

        auto modeRow = area.removeFromTop(rowH);
        modeLabel.setBounds(modeRow.removeFromLeft(labelW));
        modeBox.setBounds(modeRow.reduced(4, 2));

        area.removeFromTop(5);
        auto chRow = area.removeFromTop(rowH);
        channelLabel.setBounds(chRow.removeFromLeft(labelW));
        channelSlider.setBounds(chRow);

        area.removeFromTop(5);
        auto fbRow = area.removeFromTop(rowH);
        feedbackLabel.setBounds(fbRow.removeFromLeft(labelW));
        feedbackSlider.setBounds(fbRow);

        area.removeFromTop(5);
        auto limRow = area.removeFromTop(rowH);
        limitLabel.setBounds(limRow.removeFromLeft(labelW));
        limitSlider.setBounds(limRow);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    }

private:
    void timerCallback() override { updateVisibility(); }

    void updateVisibility()
    {
        bool isReturn = modeBox.getSelectedId() == 2;
        feedbackLabel.setVisible(isReturn);
        feedbackSlider.setVisible(isReturn);
        limitLabel.setVisible(isReturn);
        limitSlider.setVisible(isReturn);
    }

    LoopBusProcessor& processor;
    juce::AudioProcessorValueTreeState& valueTreeState;

    juce::Label modeLabel, channelLabel, feedbackLabel, limitLabel;
    juce::ComboBox modeBox;
    juce::Slider channelSlider, feedbackSlider, limitSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> channelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> feedbackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> limitAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoopBusEditor)
};

//==============================================================================

juce::AudioProcessorEditor* LoopBusProcessor::createEditor()
{
    return new LoopBusEditor(*this, params);
}

void LoopBusProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = params.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void LoopBusProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(params.state.getType()))
        params.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new LoopBusProcessor();
}