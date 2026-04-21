#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
constexpr int tableSize = 1024;
constexpr auto curveProperty = "transferCurve";

float presetTransferFunction(DrawableTransferAUAudioProcessor::Preset preset, float input)
{
    const float x = juce::jlimit(-1.0f, 1.0f, input);
    switch (preset)
    {
        case DrawableTransferAUAudioProcessor::Preset::Linear:
            return x;
        case DrawableTransferAUAudioProcessor::Preset::SoftClipTanh:
            return std::tanh(2.5f * x);
        case DrawableTransferAUAudioProcessor::Preset::HardClip:
            return juce::jlimit(-0.6f, 0.6f, x) / 0.6f;
        case DrawableTransferAUAudioProcessor::Preset::CubicDrive:
            return juce::jlimit(-1.0f, 1.0f, (1.5f * x) - (0.5f * x * x * x));
        case DrawableTransferAUAudioProcessor::Preset::HalfWaveRectifier:
            return juce::jmax(0.0f, x);
        case DrawableTransferAUAudioProcessor::Preset::FullWaveRectifier:
            return (std::abs(x) * 2.0f) - 1.0f;
        case DrawableTransferAUAudioProcessor::Preset::SineFoldback:
            return std::sin(juce::MathConstants<float>::pi * x);
        default:
            return x;
    }
}
}

DrawableTransferAUAudioProcessor::DrawableTransferAUAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    bitDepthParameter = apvts.getRawParameterValue("bitDepth");
    interpolationParameter = apvts.getRawParameterValue("interpolation");
    inputGainParameter = apvts.getRawParameterValue("inputGain");
    outputGainParameter = apvts.getRawParameterValue("outputGain");
    resetTransferCurve();
}

juce::AudioProcessorValueTreeState::ParameterLayout DrawableTransferAUAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterChoice>("bitDepth", "Bit Depth",
                                                                  juce::StringArray { "8-bit", "12-bit", "16-bit" }, 1));
    params.push_back(std::make_unique<juce::AudioParameterBool>("interpolation", "Interpolation", true));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "inputGain",
        "Input Gain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f),
        0.0f,
        "dB"));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "outputGain",
        "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f),
        0.0f,
        "dB"));
    return { params.begin(), params.end() };
}

const juce::String DrawableTransferAUAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool DrawableTransferAUAudioProcessor::acceptsMidi() const
{
    return false;
}

bool DrawableTransferAUAudioProcessor::producesMidi() const
{
    return false;
}

bool DrawableTransferAUAudioProcessor::isMidiEffect() const
{
    return false;
}

double DrawableTransferAUAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int DrawableTransferAUAudioProcessor::getNumPrograms()
{
    return 1;
}

int DrawableTransferAUAudioProcessor::getCurrentProgram()
{
    return 0;
}

void DrawableTransferAUAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String DrawableTransferAUAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void DrawableTransferAUAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void DrawableTransferAUAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(sampleRate, samplesPerBlock);
}

void DrawableTransferAUAudioProcessor::releaseResources()
{
}

bool DrawableTransferAUAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != layouts.getMainOutputChannelSet())
        return false;

    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::mono()
           || layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

float DrawableTransferAUAudioProcessor::quantizeSample(float sample, int bitDepth)
{
    const float steps = static_cast<float>((1 << bitDepth) - 1);
    const float normalized = juce::jlimit(0.0f, 1.0f, (sample * 0.5f) + 0.5f);
    const float quantized = std::round(normalized * steps) / steps;
    return (quantized * 2.0f) - 1.0f;
}

float DrawableTransferAUAudioProcessor::lookupTableValue(const Table& table, float normalized, bool interpolated)
{
    const float clamped = juce::jlimit(0.0f, 1.0f, normalized);
    const int tableMaxIndex = static_cast<int>(table.size()) - 1;
    const float indexFloat = clamped * static_cast<float>(tableMaxIndex);

    if (!interpolated)
    {
        const int index = juce::jlimit(0, tableMaxIndex, static_cast<int>(indexFloat));
        return table[static_cast<size_t>(index)];
    }

    const int lowIndex = juce::jlimit(0, tableMaxIndex, static_cast<int>(std::floor(indexFloat)));
    const int highIndex = juce::jlimit(0, tableMaxIndex, lowIndex + 1);
    const float fraction = indexFloat - static_cast<float>(lowIndex);
    return juce::jmap(fraction, table[static_cast<size_t>(lowIndex)], table[static_cast<size_t>(highIndex)]);
}

void DrawableTransferAUAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    Table localTable;
    {
        const juce::SpinLock::ScopedLockType lock(tableLock);
        localTable = transferTable;
    }

    const int bitDepth = getSelectedBitDepth();
    const bool interpolate = isInterpolationEnabled();
    const float inGain = getInputGainLinear();
    const float outGain = getOutputGainLinear();

    for (int channel = 0; channel < getTotalNumInputChannels(); ++channel)
    {
        auto* channelData = buffer.getWritePointer(channel);
        for (int sampleIndex = 0; sampleIndex < buffer.getNumSamples(); ++sampleIndex)
        {
            const float gainedInput = channelData[sampleIndex] * inGain;
            const float input = juce::jlimit(-1.0f, 1.0f, gainedInput);
            const float normalized = (input * 0.5f) + 0.5f;
            const float shaped = lookupTableValue(localTable, normalized, interpolate);
            channelData[sampleIndex] = quantizeSample(juce::jlimit(-1.0f, 1.0f, shaped * outGain), bitDepth);
        }
    }
}

bool DrawableTransferAUAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* DrawableTransferAUAudioProcessor::createEditor()
{
    return new DrawableTransferAUAudioProcessorEditor(*this);
}

void DrawableTransferAUAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    juce::String curveData;

    {
        const juce::SpinLock::ScopedLockType lock(tableLock);
        for (size_t i = 0; i < transferTable.size(); ++i)
        {
            if (i > 0)
                curveData << ",";
            curveData << juce::String(transferTable[i], 6);
        }
    }

    state.setProperty(curveProperty, curveData, nullptr);

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void DrawableTransferAUAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState == nullptr)
        return;

    if (!xmlState->hasTagName(apvts.state.getType()))
        return;

    const auto restoredState = juce::ValueTree::fromXml(*xmlState);
    apvts.replaceState(restoredState);

    if (restoredState.hasProperty(curveProperty))
    {
        auto curveData = restoredState[curveProperty].toString();
        auto tokens = juce::StringArray::fromTokens(curveData, ",", "");
        if (tokens.size() == tableSize)
        {
            const juce::SpinLock::ScopedLockType lock(tableLock);
            for (int i = 0; i < tableSize; ++i)
                transferTable[static_cast<size_t>(i)] = tokens[i].getFloatValue();
            return;
        }
    }

    resetTransferCurve();
}

std::array<float, 1024> DrawableTransferAUAudioProcessor::getTransferTableCopy() const
{
    const juce::SpinLock::ScopedLockType lock(tableLock);
    return transferTable;
}

void DrawableTransferAUAudioProcessor::setTransferPoint(float normalizedX, float normalizedY, int brushRadius)
{
    const int index = juce::jlimit(0, tableSize - 1, static_cast<int>(normalizedX * static_cast<float>(tableSize - 1)));
    const float shapedValue = juce::jlimit(-1.0f, 1.0f, (normalizedY * 2.0f) - 1.0f);

    const int clampedRadius = juce::jmax(0, brushRadius);
    const juce::SpinLock::ScopedLockType lock(tableLock);

    for (int delta = -clampedRadius; delta <= clampedRadius; ++delta)
    {
        const int pointIndex = juce::jlimit(0, tableSize - 1, index + delta);
        transferTable[static_cast<size_t>(pointIndex)] = shapedValue;
    }
}

void DrawableTransferAUAudioProcessor::setTransferSegment(float fromX, float fromY, float toX, float toY, int brushRadius)
{
    const int fromIndex = juce::jlimit(0, tableSize - 1, static_cast<int>(fromX * static_cast<float>(tableSize - 1)));
    const int toIndex = juce::jlimit(0, tableSize - 1, static_cast<int>(toX * static_cast<float>(tableSize - 1)));

    const float fromValue = juce::jlimit(-1.0f, 1.0f, (fromY * 2.0f) - 1.0f);
    const float toValue = juce::jlimit(-1.0f, 1.0f, (toY * 2.0f) - 1.0f);

    const int start = juce::jmin(fromIndex, toIndex);
    const int end = juce::jmax(fromIndex, toIndex);
    const int clampedRadius = juce::jmax(0, brushRadius);
    const int range = juce::jmax(1, end - start);

    const juce::SpinLock::ScopedLockType lock(tableLock);
    for (int i = start; i <= end; ++i)
    {
        const float t = static_cast<float>(i - start) / static_cast<float>(range);
        const float value = juce::jmap(t, fromValue, toValue);

        for (int delta = -clampedRadius; delta <= clampedRadius; ++delta)
        {
            const int pointIndex = juce::jlimit(0, tableSize - 1, i + delta);
            transferTable[static_cast<size_t>(pointIndex)] = value;
        }
    }
}

void DrawableTransferAUAudioProcessor::resetTransferCurve()
{
    const juce::SpinLock::ScopedLockType lock(tableLock);
    for (int i = 0; i < tableSize; ++i)
    {
        const float x = static_cast<float>(i) / static_cast<float>(tableSize - 1);
        transferTable[static_cast<size_t>(i)] = (x * 2.0f) - 1.0f;
    }
}

void DrawableTransferAUAudioProcessor::smoothTransferCurve(int passes, float amount)
{
    const int clampedPasses = juce::jlimit(1, 32, passes);
    const float clampedAmount = juce::jlimit(0.0f, 1.0f, amount);
    const juce::SpinLock::ScopedLockType lock(tableLock);

    Table smoothed = transferTable;
    for (int pass = 0; pass < clampedPasses; ++pass)
    {
        for (int i = 1; i < tableSize - 1; ++i)
        {
            const float neighborAverage = 0.5f * (transferTable[static_cast<size_t>(i - 1)]
                                                  + transferTable[static_cast<size_t>(i + 1)]);
            smoothed[static_cast<size_t>(i)] = juce::jmap(clampedAmount, transferTable[static_cast<size_t>(i)], neighborAverage);
        }

        smoothed[0] = transferTable[0];
        smoothed[static_cast<size_t>(tableSize - 1)] = transferTable[static_cast<size_t>(tableSize - 1)];
        transferTable = smoothed;
    }
}

void DrawableTransferAUAudioProcessor::applyPreset(Preset preset)
{
    const juce::SpinLock::ScopedLockType lock(tableLock);
    for (int i = 0; i < tableSize; ++i)
    {
        const float xNorm = static_cast<float>(i) / static_cast<float>(tableSize - 1);
        const float x = (xNorm * 2.0f) - 1.0f;
        transferTable[static_cast<size_t>(i)] = presetTransferFunction(preset, x);
    }
}

juce::StringArray DrawableTransferAUAudioProcessor::getPresetNames()
{
    return {
        "Linear",
        "Soft Clip (tanh)",
        "Hard Clip",
        "Cubic Drive",
        "Half-Wave Rectifier",
        "Full-Wave Rectifier",
        "Sine Foldback"
    };
}

int DrawableTransferAUAudioProcessor::getSelectedBitDepth() const
{
    const int choice = static_cast<int>(bitDepthParameter->load());
    switch (choice)
    {
        case 0: return 8;
        case 1: return 12;
        case 2: return 16;
        default: return 12;
    }
}

bool DrawableTransferAUAudioProcessor::isInterpolationEnabled() const
{
    return interpolationParameter->load() >= 0.5f;
}

float DrawableTransferAUAudioProcessor::getInputGainLinear() const
{
    return juce::Decibels::decibelsToGain(inputGainParameter->load());
}

float DrawableTransferAUAudioProcessor::getOutputGainLinear() const
{
    return juce::Decibels::decibelsToGain(outputGainParameter->load());
}

juce::AudioProcessorValueTreeState& DrawableTransferAUAudioProcessor::getAPVTS()
{
    return apvts;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrawableTransferAUAudioProcessor();
}
