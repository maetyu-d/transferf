#include "PluginEditor.h"

DrawableTransferAUAudioProcessorEditor::MinimalLookAndFeel::MinimalLookAndFeel()
{
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1d232c));
    setColour(juce::ComboBox::textColourId, juce::Colour(0xffedf2f7));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff3a4453));
    setColour(juce::ComboBox::arrowColourId, juce::Colour(0xffa7b4c6));

    setColour(juce::TextButton::buttonColourId, juce::Colour(0xff242c36));
    setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff305377));
    setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe7edf4));
    setColour(juce::TextButton::textColourOnId, juce::Colour(0xfff5f9ff));

    setColour(juce::ToggleButton::textColourId, juce::Colour(0xffd9e1ea));
    setColour(juce::Slider::backgroundColourId, juce::Colour(0xff1c2430));
    setColour(juce::Slider::trackColourId, juce::Colour(0xff5eb8e6));
    setColour(juce::Slider::thumbColourId, juce::Colour(0xff6ecbf8));
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffedf2f7));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff151a22));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff3a4453));
}

TransferFunctionComponent::TransferFunctionComponent(DrawableTransferAUAudioProcessor& processorToUse)
    : processor(processorToUse)
{
}

void TransferFunctionComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0d1014));
    const auto area = getLocalBounds().toFloat().reduced(10.0f);
    g.setColour(juce::Colour(0xff242a33));
    g.drawRoundedRectangle(area.expanded(2.0f), 8.0f, 1.0f);

    g.setColour(juce::Colour(0xff1e2530));
    for (int i = 1; i < 4; ++i)
    {
        const float x = area.getX() + (area.getWidth() * (static_cast<float>(i) / 4.0f));
        const float y = area.getY() + (area.getHeight() * (static_cast<float>(i) / 4.0f));
        g.drawVerticalLine(static_cast<int>(x), area.getY(), area.getBottom());
        g.drawHorizontalLine(static_cast<int>(y), area.getX(), area.getRight());
    }
    g.setColour(juce::Colour(0xff3b4350));
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

    g.setColour(juce::Colour(0xffffb347));
    g.strokePath(curve, juce::PathStrokeType(2.1f, juce::PathStrokeType::JointStyle::curved, juce::PathStrokeType::EndCapStyle::rounded));
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
    setSize(940, 580);
    setLookAndFeel(&lookAndFeel);

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
    inputGainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 22);
    inputGainSlider.setTextValueSuffix(" dB");
    inputGainSlider.setSkewFactorFromMidPoint(0.0);
    addAndMakeVisible(inputGainSlider);
    inputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "inputGain", inputGainSlider);

    outputGainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    outputGainSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 22);
    outputGainSlider.setTextValueSuffix(" dB");
    outputGainSlider.setSkewFactorFromMidPoint(0.0);
    addAndMakeVisible(outputGainSlider);
    outputGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "outputGain", outputGainSlider);

    offsetSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    offsetSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 22);
    offsetSlider.setRange(-1.0, 1.0, 0.001);
    offsetSlider.setTextValueSuffix(" ");
    addAndMakeVisible(offsetSlider);
    offsetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getAPVTS(), "audioOffset", offsetSlider);

    brushSlider.setRange(1.0, 20.0, 1.0);
    brushSlider.setValue(4.0);
    brushSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    brushSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 22);
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
    smoothAmountSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 22);
    addAndMakeVisible(smoothAmountSlider);

    smoothPassesSlider.setRange(1.0, 12.0, 1.0);
    smoothPassesSlider.setValue(1.0);
    smoothPassesSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    smoothPassesSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 22);
    addAndMakeVisible(smoothPassesSlider);

    addAndMakeVisible(applyPresetButton);
    applyPresetButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff35698d));
    applyPresetButton.setClickingTogglesState(false);
    applyPresetButton.onClick = [this]
    {
        applySelectedPreset();
    };

    addAndMakeVisible(storeAButton);
    storeAButton.setClickingTogglesState(false);
    storeAButton.onClick = [this] { audioProcessor.storeCurveToSlot(DrawableTransferAUAudioProcessor::CurveSlot::A); };
    addAndMakeVisible(recallAButton);
    recallAButton.setClickingTogglesState(false);
    recallAButton.onClick = [this]
    {
        audioProcessor.recallCurveFromSlot(DrawableTransferAUAudioProcessor::CurveSlot::A);
        transferFunctionComponent.repaint();
    };
    addAndMakeVisible(storeBButton);
    storeBButton.setClickingTogglesState(false);
    storeBButton.onClick = [this] { audioProcessor.storeCurveToSlot(DrawableTransferAUAudioProcessor::CurveSlot::B); };
    addAndMakeVisible(recallBButton);
    recallBButton.setClickingTogglesState(false);
    recallBButton.onClick = [this]
    {
        audioProcessor.recallCurveFromSlot(DrawableTransferAUAudioProcessor::CurveSlot::B);
        transferFunctionComponent.repaint();
    };

    addAndMakeVisible(smoothButton);
    smoothButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff35698d));
    smoothButton.setClickingTogglesState(false);
    smoothButton.onClick = [this]
    {
        audioProcessor.smoothTransferCurve(static_cast<int>(smoothPassesSlider.getValue()),
                                           static_cast<float>(smoothAmountSlider.getValue()));
        transferFunctionComponent.repaint();
    };

    addAndMakeVisible(resetButton);
    resetButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff6b4e3a));
    resetButton.setClickingTogglesState(false);
    resetButton.onClick = [this]
    {
        audioProcessor.resetTransferCurve();
        transferFunctionComponent.repaint();
    };
}

DrawableTransferAUAudioProcessorEditor::~DrawableTransferAUAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void DrawableTransferAUAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0e1013));
    auto panel = getLocalBounds().toFloat().reduced(12.0f);
    g.setColour(juce::Colour(0xff171b21));
    g.fillRoundedRectangle(panel, 10.0f);
    g.setColour(juce::Colour(0xff2a303a));
    g.drawRoundedRectangle(panel, 10.0f, 1.0f);

    g.setColour(juce::Colour(0xfff3f5f7));
    g.setFont(16.0f);
    g.drawText("Drawable Transfer Function", 28, 18, 300, 20, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xff97a3b3));
    g.setFont(12.0f);
    g.drawText("Tone", 28, 46, 80, 16, juce::Justification::centredLeft);
    g.drawText("Signal", 28, 82, 80, 16, juce::Justification::centredLeft);
    g.drawText("Curve Tools", 28, 118, 100, 16, juce::Justification::centredLeft);
    g.drawText("A/B", 730, 118, 40, 16, juce::Justification::centredLeft);
}

void DrawableTransferAUAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(22);
    area.removeFromTop(34);

    auto rowTone = area.removeFromTop(32);
    auto rowSignal = area.removeFromTop(32);
    auto rowTools = area.removeFromTop(32);
    area.removeFromTop(14);

    rowTone.removeFromLeft(90);
    presetBox.setBounds(rowTone.removeFromLeft(286).reduced(4));
    rowTone.removeFromLeft(8);
    applyPresetButton.setBounds(rowTone.removeFromLeft(114).reduced(4));
    rowTone.removeFromLeft(8);
    bitDepthBox.setBounds(rowTone.removeFromLeft(96).reduced(4));
    rowTone.removeFromLeft(8);
    interpolationButton.setBounds(rowTone.removeFromLeft(126).reduced(4));
    rowTone.removeFromLeft(8);
    resetButton.setBounds(rowTone.removeFromLeft(92).reduced(4));

    rowSignal.removeFromLeft(90);
    inputGainSlider.setBounds(rowSignal.removeFromLeft(252).reduced(4));
    rowSignal.removeFromLeft(10);
    outputGainSlider.setBounds(rowSignal.removeFromLeft(252).reduced(4));
    rowSignal.removeFromLeft(10);
    offsetEnabledButton.setBounds(rowSignal.removeFromLeft(112).reduced(4));
    rowSignal.removeFromLeft(10);
    offsetSlider.setBounds(rowSignal.removeFromLeft(202).reduced(4));

    rowTools.removeFromLeft(90);
    brushSlider.setBounds(rowTools.removeFromLeft(188).reduced(4));
    rowTools.removeFromLeft(8);
    smoothAmountSlider.setBounds(rowTools.removeFromLeft(154).reduced(4));
    rowTools.removeFromLeft(8);
    smoothPassesSlider.setBounds(rowTools.removeFromLeft(90).reduced(4));
    rowTools.removeFromLeft(8);
    smoothButton.setBounds(rowTools.removeFromLeft(124).reduced(4));
    rowTools.removeFromLeft(24);
    storeAButton.setBounds(rowTools.removeFromLeft(72).reduced(4));
    rowTools.removeFromLeft(6);
    recallAButton.setBounds(rowTools.removeFromLeft(72).reduced(4));
    rowTools.removeFromLeft(6);
    storeBButton.setBounds(rowTools.removeFromLeft(72).reduced(4));
    rowTools.removeFromLeft(6);
    recallBButton.setBounds(rowTools.removeFromLeft(72).reduced(4));

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
