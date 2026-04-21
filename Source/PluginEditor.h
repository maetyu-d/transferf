#pragma once

#include "PluginProcessor.h"

#include <juce_gui_extra/juce_gui_extra.h>

class TransferFunctionComponent final : public juce::Component
{
public:
    explicit TransferFunctionComponent(DrawableTransferAUAudioProcessor& processorToUse);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void setBrushRadius(int newRadius);

private:
    std::optional<juce::Point<float>> applyMouseEvent(const juce::MouseEvent& event);
    static std::optional<juce::Point<float>> toNormalizedPoint(juce::Rectangle<float> area, juce::Point<float> point);

    DrawableTransferAUAudioProcessor& processor;
    std::optional<juce::Point<float>> lastPoint;
    int brushRadius = 4;
};

class DrawableTransferAUAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit DrawableTransferAUAudioProcessorEditor(DrawableTransferAUAudioProcessor&);
    ~DrawableTransferAUAudioProcessorEditor() override = default;

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    void applySelectedPreset();

    DrawableTransferAUAudioProcessor& audioProcessor;
    TransferFunctionComponent transferFunctionComponent;
    juce::ComboBox presetBox;
    juce::ComboBox bitDepthBox;
    juce::ToggleButton interpolationButton { "Interp" };
    juce::Slider inputGainSlider;
    juce::Slider outputGainSlider;
    juce::Slider brushSlider;
    juce::Slider smoothAmountSlider;
    juce::Slider smoothPassesSlider;
    juce::TextButton smoothButton { "Smooth Curve" };
    juce::TextButton applyPresetButton { "Apply Preset" };
    juce::TextButton resetButton { "Reset Curve" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> bitDepthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> interpolationAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> outputGainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DrawableTransferAUAudioProcessorEditor)
};
