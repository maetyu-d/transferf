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
    setSize(820, 520);

    addAndMakeVisible(transferFunctionComponent);

    const auto presetNames = DrawableTransferAUAudioProcessor::getPresetNames();
    for (int i = 0; i < presetNames.size(); ++i)
        presetBox.addItem(presetNames[i], i + 1);
    presetBox.setSelectedId(1);
    addAndMakeVisible(presetBox);

    bitDepthBox.addItem("8-bit", 1);
    bitDepthBox.addItem("12-bit", 2);
    bitDepthBox.addItem("16-bit", 3);
    addAndMakeVisible(bitDepthBox);

    bitDepthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.getAPVTS(),
        "bitDepth",
        bitDepthBox);

    addAndMakeVisible(interpolationButton);
    interpolationAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.getAPVTS(),
        "interpolation",
        interpolationButton);

    inputGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    inputGainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 20);
    inputGainSlider.setTextValueSuffix(" dB");
    addAndMakeVisible(inputGainSlider);
    inputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "inputGain", inputGainSlider);

    outputGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    outputGainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 56, 20);
    outputGainSlider.setTextValueSuffix(" dB");
    addAndMakeVisible(outputGainSlider);
    outputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "outputGain", outputGainSlider);

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
    g.fillAll(juce::Colour(0xff151719));
    g.setColour(juce::Colour(0xff262a2e));
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(8.0f), 10.0f);

    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    g.drawText("Drawable Transfer Function", 20, 12, 300, 24, juce::Justification::centredLeft);

    g.setFont(13.0f);
    g.setColour(juce::Colour(0xffd0d5db));
    g.drawText("Preset", 20, 44, 60, 20, juce::Justification::centredLeft);
    g.drawText("Input", 20, 74, 60, 20, juce::Justification::centredLeft);
    g.drawText("Output", 400, 74, 70, 20, juce::Justification::centredLeft);
    g.drawText("Brush", 20, 104, 60, 20, juce::Justification::centredLeft);
    g.drawText("Smooth", 400, 104, 70, 20, juce::Justification::centredLeft);
}

void DrawableTransferAUAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(18);
    area.removeFromTop(28);

    auto rowPreset = area.removeFromTop(28);
    auto rowGain = area.removeFromTop(28);
    auto rowDraw = area.removeFromTop(28);
    area.removeFromTop(6);

    presetBox.setBounds(rowPreset.removeFromLeft(240).reduced(2));
    applyPresetButton.setBounds(rowPreset.removeFromLeft(110).reduced(2));
    bitDepthBox.setBounds(rowPreset.removeFromLeft(120).reduced(2));
    interpolationButton.setBounds(rowPreset.removeFromLeft(90).reduced(2));
    resetButton.setBounds(rowPreset.removeFromLeft(120).reduced(2));

    rowGain.removeFromLeft(60);
    inputGainSlider.setBounds(rowGain.removeFromLeft(300).reduced(2));
    rowGain.removeFromLeft(20);
    rowGain.removeFromLeft(60);
    outputGainSlider.setBounds(rowGain.removeFromLeft(300).reduced(2));

    rowDraw.removeFromLeft(60);
    brushSlider.setBounds(rowDraw.removeFromLeft(300).reduced(2));
    rowDraw.removeFromLeft(20);
    rowDraw.removeFromLeft(70);
    smoothAmountSlider.setBounds(rowDraw.removeFromLeft(160).reduced(2));
    smoothPassesSlider.setBounds(rowDraw.removeFromLeft(120).reduced(2));
    smoothButton.setBounds(rowDraw.removeFromLeft(120).reduced(2));

    transferFunctionComponent.setBounds(area.reduced(2));
}

void DrawableTransferAUAudioProcessorEditor::applySelectedPreset()
{
    const int selectedIndex = juce::jlimit(0,
                                           static_cast<int>(DrawableTransferAUAudioProcessor::getPresetNames().size()) - 1,
                                           presetBox.getSelectedItemIndex());
    audioProcessor.applyPreset(static_cast<DrawableTransferAUAudioProcessor::Preset>(selectedIndex));
    transferFunctionComponent.repaint();
}
