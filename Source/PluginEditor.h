/*
  ==============================================================================
    PluginEditor.h
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

class UltimateCompAudioProcessorEditor final : public juce::AudioProcessorEditor,
    private juce::Timer
{
public:
    explicit UltimateCompAudioProcessorEditor(UltimateCompAudioProcessor&);
    ~UltimateCompAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override; // <--- Critical addition
    void resized() override;

private:
    void timerCallback() override;

    UltimateCompAudioProcessor& audioProcessor;

    // Forward declarations
    class UltimateLNF;
    class Panel;
    class Knob;

    std::unique_ptr<UltimateLNF> lnf;

    // Panels
    std::unique_ptr<Panel> panelDyn;
    std::unique_ptr<Panel> panelDet;
    std::unique_ptr<Panel> panelCrest;
    std::unique_ptr<Panel> panelTpFlux;
    std::unique_ptr<Panel> panelSat;
    std::unique_ptr<Panel> panelEq;

    // Controls
    std::unique_ptr<Knob> kThresh, kRatio, kKnee, kAttack, kRelease, kMakeup, kMix;
    std::unique_ptr<Knob> kScHpf, kDetRms, kStereoLink, kFbBlend;
    std::unique_ptr<Knob> kCrestTarget, kCrestSpeed;
    std::unique_ptr<Knob> kTpAmt, kTpRaise, kFluxAmt;
    std::unique_ptr<Knob> kSatDrive, kSatTrim, kSatMix;
    std::unique_ptr<Knob> kTone, kToneFreq, kBright, kBrightFreq;

    juce::ComboBox cAutoRel;
    juce::ToggleButton bTurbo;

    juce::ComboBox cThrust;
    juce::ComboBox cCtrlMode;
    juce::ComboBox cTpMode;
    juce::ComboBox cFluxMode;
    juce::ComboBox cSatMode;
    juce::ComboBox cSatAutoGain;
    juce::ComboBox cSignalFlow;

    // Attachments
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> aThresh, aRatio, aKnee, aAttack, aRelease, aMakeup, aMix;
    std::unique_ptr<SliderAttachment> aScHpf, aDetRms, aStereoLink, aFbBlend;
    std::unique_ptr<SliderAttachment> aCrestTarget, aCrestSpeed;
    std::unique_ptr<SliderAttachment> aTpAmt, aTpRaise, aFluxAmt;
    std::unique_ptr<SliderAttachment> aSatDrive, aSatTrim, aSatMix;
    std::unique_ptr<SliderAttachment> aTone, aToneFreq, aBright, aBrightFreq;

    std::unique_ptr<ComboBoxAttachment> aAutoRel, aThrust, aCtrlMode, aTpMode, aFluxMode, aSatMode, aSatAutoGain, aSignalFlow;
    std::unique_ptr<ButtonAttachment> aTurbo;

    // Helpers
    void initCombo(juce::ComboBox& box, std::unique_ptr<ComboBoxAttachment>& attachment, const juce::String& paramID);
    void bindKnob(Knob& knob, std::unique_ptr<SliderAttachment>& attachment, const juce::String& paramID, const juce::String& suffix);

    // Meter data
    float smoothInL = 0.f, smoothInR = 0.f;
    float smoothOutL = 0.f, smoothOutR = 0.f;
    float smoothGR = 0.f;
    float smoothFlux = 0.f;

    // Paint geometry (Global Coordinates)
    juce::Rectangle<int> inMeterArea;
    juce::Rectangle<int> outMeterArea;
    juce::Rectangle<int> grBarArea;
    juce::Rectangle<int> fluxDotArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UltimateCompAudioProcessorEditor)
};