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
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    UltimateCompAudioProcessor& audioProcessor;

    // Logo Image
    juce::Image pluginLogo;

    class UltimateLNF;
    class Panel;
    class Knob;

    std::unique_ptr<UltimateLNF> lnf; // Main Dark Theme

    // Panels
    std::unique_ptr<Panel> panelDyn;
    std::unique_ptr<Panel> panelDet;
    std::unique_ptr<Panel> panelCrest;
    std::unique_ptr<Panel> panelTpFlux;
    std::unique_ptr<Panel> panelSat;
    std::unique_ptr<Panel> panelEq;

    // Controls
    std::unique_ptr<Knob> kThresh, kRatio, kKnee, kAttack, kRelease, kMakeup, kMix;
    std::unique_ptr<Knob> kScHpf, kScLpf, kDetRms, kStereoLink, kFbBlend; // Added kScLpf
    std::unique_ptr<Knob> kCrestTarget, kCrestSpeed;
    std::unique_ptr<Knob> kTpAmt, kTpRaise, kFluxAmt;

    std::unique_ptr<Knob> kSatPre, kSatDrive, kSatTrim, kSatMix;
    std::unique_ptr<Knob> kTone, kToneFreq, kBright, kBrightFreq;

    juce::ComboBox cAutoRel, cThrust, cCtrlMode, cTpMode, cFluxMode, cSatMode, cSatAutoGain, cSignalFlow;

    // SIDECHAIN INTEGRATED COMBOS
    juce::ComboBox cScMode;
    juce::ComboBox cMsMode;

    // Buttons
    juce::ToggleButton bTurboAtt, bTurboRel, bMirror, bAutoMakeup;

    // Module Bypasses
    juce::ToggleButton bActiveDyn, bActiveDet, bActiveCrest, bActiveTpFlux, bActiveSat, bActiveEq;

    // Attachments
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> aThresh, aRatio, aKnee, aAttack, aRelease, aMakeup, aMix;
    std::unique_ptr<SliderAttachment> aScHpf, aScLpf, aDetRms, aStereoLink, aFbBlend; // Added aScLpf
    std::unique_ptr<SliderAttachment> aCrestTarget, aCrestSpeed;
    std::unique_ptr<SliderAttachment> aTpAmt, aTpRaise, aFluxAmt;
    std::unique_ptr<SliderAttachment> aSatPre, aSatDrive, aSatTrim, aSatMix;
    std::unique_ptr<SliderAttachment> aTone, aToneFreq, aBright, aBrightFreq;

    // Attachments
    std::unique_ptr<ComboBoxAttachment> aMsMode;
    std::unique_ptr<ComboBoxAttachment> cScModeAtt;

    std::unique_ptr<ComboBoxAttachment> aAutoRel, aThrust, aCtrlMode, aTpMode, aFluxMode, aSatMode, aSatAutoGain, aSignalFlow;
    std::unique_ptr<ButtonAttachment> aTurboAtt, aTurboRel, aMirror, aAutoMakeup;
    std::unique_ptr<ButtonAttachment> aActiveDyn, aActiveDet, aActiveCrest, aActiveTpFlux, aActiveSat, aActiveEq;

    // Helpers
    void initCombo(juce::ComboBox& box, std::unique_ptr<ComboBoxAttachment>& attachment, const juce::String& paramID);
    void bindKnob(Knob& knob, std::unique_ptr<SliderAttachment>& attachment, const juce::String& paramID, const juce::String& suffix);

    float smoothInL = 0.f, smoothInR = 0.f;
    float smoothOutL = 0.f, smoothOutR = 0.f;
    float smoothGR = 0.f, smoothFlux = 0.f, smoothCrest = 0.f;

    juce::Rectangle<int> inMeterArea, outMeterArea, grBarArea, fluxDotArea, crestDotArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UltimateCompAudioProcessorEditor)
};