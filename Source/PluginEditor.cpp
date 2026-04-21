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
    setSize(760, 460);

    addAndMakeVisible(transferFunctionComponent);

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

    addAndMakeVisible(resetButton);
    resetButton.onClick = [this]
    {
        audioProcessor.resetTransferCurve();
        transferFunctionComponent.repaint();
    };
}

void DrawableTransferAUAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff111111));
    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    g.drawText("Drawable Transfer Function", 16, 10, 260, 24, juce::Justification::centredLeft);
    g.setFont(13.0f);
    g.setColour(juce::Colours::lightgrey);
    g.drawText("Input", 16, 44, 50, 20, juce::Justification::centredLeft);
    g.drawText("Output", 336, 44, 60, 20, juce::Justification::centredLeft);
    g.drawText("Brush", 16, 74, 50, 20, juce::Justification::centredLeft);
}

void DrawableTransferAUAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(12);
    auto topBar1 = area.removeFromTop(30);
    auto topBar2 = area.removeFromTop(30);
    auto topBar3 = area.removeFromTop(30);
    area.removeFromTop(4);

    bitDepthBox.setBounds(topBar1.removeFromLeft(130).reduced(2));
    interpolationButton.setBounds(topBar1.removeFromLeft(90).reduced(2));
    resetButton.setBounds(topBar1.removeFromLeft(120).reduced(2));

    inputGainSlider.setBounds(topBar2.removeFromLeft(300).reduced(2));
    outputGainSlider.setBounds(topBar2.removeFromLeft(300).reduced(2));
    brushSlider.setBounds(topBar3.removeFromLeft(260).reduced(2));

    transferFunctionComponent.setBounds(area.reduced(2));
}
