#include "PluginEditor.h"

TransferFunctionComponent::TransferFunctionComponent(DrawableTransferAUAudioProcessor& processorToUse)
    : processor(processorToUse)
{
}

void TransferFunctionComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::dimgrey);
    g.drawRect(getLocalBounds());

    const auto area = getLocalBounds().toFloat().reduced(8.0f);
    g.setColour(juce::Colours::darkgrey);
    g.drawHorizontalLine(static_cast<int>(area.getCentreY()), area.getX(), area.getRight());
    g.drawVerticalLine(static_cast<int>(area.getCentreX()), area.getY(), area.getBottom());

    auto table = processor.getTransferTableCopy();

    juce::Path curve;
    for (size_t i = 0; i < table.size(); ++i)
    {
        const float xNorm = static_cast<float>(i) / static_cast<float>(table.size() - 1);
        const float yNorm = (table[i] + 1.0f) * 0.5f;
        const float x = area.getX() + xNorm * area.getWidth();
        const float y = area.getBottom() - yNorm * area.getHeight();

        if (i == 0)
            curve.startNewSubPath(x, y);
        else
            curve.lineTo(x, y);
    }

    g.setColour(juce::Colours::orange);
    g.strokePath(curve, juce::PathStrokeType(2.0f));
}

void TransferFunctionComponent::mouseDown(const juce::MouseEvent& event)
{
    lastPoint = applyMouseEvent(event);
}

void TransferFunctionComponent::mouseDrag(const juce::MouseEvent& event)
{
    const auto current = applyMouseEvent(event);
    if (!current.has_value() || !lastPoint.has_value())
        return;

    processor.setTransferSegment(lastPoint->x, lastPoint->y, current->x, current->y, brushRadius);
    lastPoint = current;
    repaint();
}

void TransferFunctionComponent::mouseUp(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    lastPoint.reset();
}

void TransferFunctionComponent::setBrushRadius(int newRadius)
{
    brushRadius = juce::jlimit(1, 20, newRadius);
}

std::optional<juce::Point<float>> TransferFunctionComponent::applyMouseEvent(const juce::MouseEvent& event)
{
    const auto area = getLocalBounds().toFloat().reduced(8.0f);
    const auto normalized = toNormalizedPoint(area, event.position);
    if (!normalized.has_value())
        return std::nullopt;

    processor.setTransferPoint(normalized->x, normalized->y, brushRadius);
    repaint();
    return normalized;
}

std::optional<juce::Point<float>> TransferFunctionComponent::toNormalizedPoint(juce::Rectangle<float> area, juce::Point<float> point)
{
    if (!area.contains(point))
        return std::nullopt;

    const float xNorm = juce::jlimit(0.0f, 1.0f, (point.x - area.getX()) / area.getWidth());
    const float yNorm = juce::jlimit(0.0f, 1.0f, 1.0f - ((point.y - area.getY()) / area.getHeight()));
    return juce::Point<float>(xNorm, yNorm);
}

DrawableTransferAUAudioProcessorEditor::DrawableTransferAUAudioProcessorEditor(DrawableTransferAUAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), transferFunctionComponent(p)
{
    setSize(920, 560);

    addAndMakeVisible(transferFunctionComponent);

    const auto presetNames = DrawableTransferAUAudioProcessor::getPresetNames();
    for (int i = 0; i < presetNames.size(); ++i)
        presetBox.addItem(presetNames[i], i + 1);
    presetBox.setSelectedId(1);
    addAndMakeVisible(presetBox);

    bitDepthBox.addItem("4-bit", 1);
    bitDepthBox.addItem("6-bit", 2);
    bitDepthBox.addItem("8-bit", 3);
    bitDepthBox.addItem("12-bit", 4);
    bitDepthBox.addItem("16-bit", 5);
    bitDepthBox.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(bitDepthBox);

    bitDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getAPVTS(),
        "bitDepth",
        bitDepthBox);

    addAndMakeVisible(interpolationButton);
    interpolationButton.setButtonText("Interpolation");
    interpolationAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getAPVTS(),
        "interpolation",
        interpolationButton);

    addAndMakeVisible(offsetEnabledButton);
    offsetEnabledButton.setButtonText("Audio Offset");
    offsetEnabledAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getAPVTS(),
        "offsetEnabled",
        offsetEnabledButton);

    inputGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    inputGainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 20);
    inputGainSlider.setTextValueSuffix(" dB");
    inputGainSlider.setSkewFactorFromMidPoint(0.0);
    addAndMakeVisible(inputGainSlider);
    inputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "inputGain", inputGainSlider);

    outputGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    outputGainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 20);
    outputGainSlider.setTextValueSuffix(" dB");
    outputGainSlider.setSkewFactorFromMidPoint(0.0);
    addAndMakeVisible(outputGainSlider);
    outputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "outputGain", outputGainSlider);

    offsetSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    offsetSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 20);
    offsetSlider.setRange(-1.0, 1.0, 0.001);
    offsetSlider.setTextValueSuffix(" ");
    addAndMakeVisible(offsetSlider);
    offsetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "audioOffset", offsetSlider);

    brushSlider.setRange(1.0, 20.0, 1.0);
    brushSlider.setValue(4.0);
    brushSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    brushSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 46, 20);
    brushSlider.setTextValueSuffix(" px");
    brushSlider.onValueChange = [this]
    {
        transferFunctionComponent.setBrushRadius(static_cast<int>(brushSlider.getValue()));
    };
    addAndMakeVisible(brushSlider);
    transferFunctionComponent.setBrushRadius(static_cast<int>(brushSlider.getValue()));

    smoothAmountSlider.setRange(0.05, 1.0, 0.01);
    smoothAmountSlider.setValue(0.35);
    smoothAmountSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    smoothAmountSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 46, 20);
    addAndMakeVisible(smoothAmountSlider);

    smoothPassesSlider.setRange(1.0, 12.0, 1.0);
    smoothPassesSlider.setValue(1.0);
    smoothPassesSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    smoothPassesSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 46, 20);
    addAndMakeVisible(smoothPassesSlider);

    addAndMakeVisible(applyPresetButton);
    applyPresetButton.onClick = [this]
    {
        applySelectedPreset();
    };

    addAndMakeVisible(storeAButton);
    storeAButton.onClick = [this] { audioProcessor.storeCurveToSlot(DrawableTransferAUAudioProcessor::CurveSlot::A); };
    addAndMakeVisible(recallAButton);
    recallAButton.onClick = [this]
    {
        audioProcessor.recallCurveFromSlot(DrawableTransferAUAudioProcessor::CurveSlot::A);
        transferFunctionComponent.repaint();
    };
    addAndMakeVisible(storeBButton);
    storeBButton.onClick = [this] { audioProcessor.storeCurveToSlot(DrawableTransferAUAudioProcessor::CurveSlot::B); };
    addAndMakeVisible(recallBButton);
    recallBButton.onClick = [this]
    {
        audioProcessor.recallCurveFromSlot(DrawableTransferAUAudioProcessor::CurveSlot::B);
        transferFunctionComponent.repaint();
    };

    addAndMakeVisible(smoothButton);
    smoothButton.onClick = [this]
    {
        audioProcessor.smoothTransferCurve(static_cast<int>(smoothPassesSlider.getValue()),
                                           static_cast<float>(smoothAmountSlider.getValue()));
        transferFunctionComponent.repaint();
    };

    addAndMakeVisible(resetButton);
    resetButton.onClick = [this]
    {
        audioProcessor.resetTransferCurve();
        transferFunctionComponent.repaint();
    };
}

void DrawableTransferAUAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff111315));
    auto panel = getLocalBounds().toFloat().reduced(10.0f);
    g.setColour(juce::Colour(0xff1b1f23));
    g.fillRoundedRectangle(panel, 10.0f);
    g.setColour(juce::Colour(0xff2d333b));
    g.drawRoundedRectangle(panel, 10.0f, 1.0f);

    g.setColour(juce::Colours::white);
    g.setFont(17.0f);
    g.drawText("Drawable Transfer Function", 24, 14, 320, 24, juce::Justification::centredLeft);

    g.setFont(13.0f);
    g.setColour(juce::Colour(0xffd0d5db));
    g.drawText("Presets", 28, 48, 80, 20, juce::Justification::centredLeft);
    g.drawText("A/B Compare", 560, 48, 100, 20, juce::Justification::centredLeft);
    g.drawText("Input Gain", 28, 84, 90, 20, juce::Justification::centredLeft);
    g.drawText("Output Gain", 480, 84, 100, 20, juce::Justification::centredLeft);
    g.drawText("Offset Amount", 28, 120, 110, 20, juce::Justification::centredLeft);
    g.drawText("Drawing", 28, 156, 90, 20, juce::Justification::centredLeft);
    g.drawText("Smoothing", 480, 156, 90, 20, juce::Justification::centredLeft);
}

void DrawableTransferAUAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    area.removeFromTop(30);

    auto rowPreset = area.removeFromTop(32);
    auto rowAB = area.removeFromTop(32);
    auto rowGain = area.removeFromTop(32);
    auto rowOffset = area.removeFromTop(32);
    auto rowDraw = area.removeFromTop(32);
    area.removeFromTop(8);

    rowPreset.removeFromLeft(110);
    presetBox.setBounds(rowPreset.removeFromLeft(280).reduced(3));
    rowPreset.removeFromLeft(8);
    applyPresetButton.setBounds(rowPreset.removeFromLeft(120).reduced(3));
    rowPreset.removeFromLeft(8);
    bitDepthBox.setBounds(rowPreset.removeFromLeft(120).reduced(3));
    rowPreset.removeFromLeft(8);
    interpolationButton.setBounds(rowPreset.removeFromLeft(130).reduced(3));
    rowPreset.removeFromLeft(8);
    offsetEnabledButton.setBounds(rowPreset.removeFromLeft(120).reduced(3));
    rowPreset.removeFromLeft(8);
    resetButton.setBounds(rowPreset.removeFromLeft(110).reduced(3));

    rowAB.removeFromLeft(540);
    storeAButton.setBounds(rowAB.removeFromLeft(90).reduced(3));
    rowAB.removeFromLeft(8);
    recallAButton.setBounds(rowAB.removeFromLeft(90).reduced(3));
    rowAB.removeFromLeft(8);
    storeBButton.setBounds(rowAB.removeFromLeft(90).reduced(3));
    rowAB.removeFromLeft(8);
    recallBButton.setBounds(rowAB.removeFromLeft(90).reduced(3));

    rowGain.removeFromLeft(110);
    inputGainSlider.setBounds(rowGain.removeFromLeft(330).reduced(3));
    rowGain.removeFromLeft(20);
    outputGainSlider.setBounds(rowGain.removeFromLeft(330).reduced(3));

    rowOffset.removeFromLeft(110);
    offsetSlider.setBounds(rowOffset.removeFromLeft(330).reduced(3));

    rowDraw.removeFromLeft(110);
    brushSlider.setBounds(rowDraw.removeFromLeft(330).reduced(3));
    rowDraw.removeFromLeft(20);
    smoothAmountSlider.setBounds(rowDraw.removeFromLeft(170).reduced(3));
    rowDraw.removeFromLeft(8);
    smoothPassesSlider.setBounds(rowDraw.removeFromLeft(120).reduced(3));
    rowDraw.removeFromLeft(8);
    smoothButton.setBounds(rowDraw.removeFromLeft(130).reduced(3));

    transferFunctionComponent.setBounds(area.reduced(4));
}

void DrawableTransferAUAudioProcessorEditor::applySelectedPreset()
{
    const int selectedIndex = juce::jlimit(0,
                                           static_cast<int>(DrawableTransferAUAudioProcessor::getPresetNames().size()) - 1,
                                           presetBox.getSelectedItemIndex());
    audioProcessor.applyPreset(static_cast<DrawableTransferAUAudioProcessor::Preset>(selectedIndex));
    transferFunctionComponent.repaint();
}
