/*
  ==============================================================================
    PluginProcessor.h
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "UltimateCompDSP.h"

class UltimateCompAudioProcessor : public juce::AudioProcessor
{
public:
    UltimateCompAudioProcessor();
    ~UltimateCompAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

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

    juce::AudioProcessorValueTreeState apvts;

    // --- METERING DATA (Thread Safe) ---
    std::atomic<float> meterInL{ 0.0f };
    std::atomic<float> meterInR{ 0.0f };
    std::atomic<float> meterOutL{ 0.0f };
    std::atomic<float> meterOutR{ 0.0f };
    std::atomic<float> meterGR{ 0.0f }; // Gain Reduction (dB)
    std::atomic<float> meterFlux{ 0.0f }; // Saturation Heat (0.0 - 1.0+)

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    UltimateCompDSP dsp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UltimateCompAudioProcessor)
};