#pragma once

#include <array>
#include <optional>

#include <juce_audio_processors/juce_audio_processors.h>

class DrawableTransferAUAudioProcessor final : public juce::AudioProcessor
{
public:
    enum class Preset
    {
        Linear = 0,
        SoftClipTanh,
        HardClip,
        CubicDrive,
        HalfWaveRectifier,
        FullWaveRectifier,
        SineFoldback
    };

    DrawableTransferAUAudioProcessor();
    ~DrawableTransferAUAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    std::array<float, 1024> getTransferTableCopy() const;
    void setTransferPoint(float normalizedX, float normalizedY, int brushRadius = 2);
    void setTransferSegment(float fromX, float fromY, float toX, float toY, int brushRadius = 2);
    void resetTransferCurve();
    void smoothTransferCurve(int passes = 1, float amount = 0.35f);
    void applyPreset(Preset preset);
    static juce::StringArray getPresetNames();
    int getSelectedBitDepth() const;
    bool isInterpolationEnabled() const;
    float getInputGainLinear() const;
    float getOutputGainLinear() const;
    juce::AudioProcessorValueTreeState& getAPVTS();

private:
    using Table = std::array<float, 1024>;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static float quantizeSample(float sample, int bitDepth);
    static float lookupTableValue(const Table& table, float normalized, bool interpolated);

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float>* bitDepthParameter = nullptr;
    std::atomic<float>* interpolationParameter = nullptr;
    std::atomic<float>* inputGainParameter = nullptr;
    std::atomic<float>* outputGainParameter = nullptr;

    mutable juce::SpinLock tableLock;
    Table transferTable {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrawableTransferAUAudioProcessor)
};
