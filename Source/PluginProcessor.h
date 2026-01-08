/*
  ==============================================================================
    PluginProcessor.h
  ==============================================================================
*/

#pragma once

#if defined(_MSC_VER)
#pragma warning(push)
// Suppress noisy MSVC Code Analysis/Core Guideline warnings coming from JUCE headers.
#pragma warning(disable: 26495) // uninitialised member variables
#pragma warning(disable: 26451) // arithmetic overflow
#pragma warning(disable: 26439) // function should not throw
#pragma warning(disable: 26440) // function should be noexcept
#pragma warning(disable: 26812) // prefer enum class
#pragma warning(disable: 26819) // unannotated fallthrough
#endif
#include <JuceHeader.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#include "UltimateCompDSP.h"
#include "PresetManager.h" // ADDED

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

    // ADDED: Preset Manager
    std::unique_ptr<PresetManager> presetManager;

    // --- METERING DATA ---
    std::atomic<float> meterInL{ 0.0f };
    std::atomic<float> meterInR{ 0.0f };
    std::atomic<float> meterOutL{ 0.0f };
    std::atomic<float> meterOutR{ 0.0f };
    std::atomic<float> meterGR{ 0.0f };
    std::atomic<float> meterFlux{ 0.0f };
    std::atomic<float> meterCrest{ 0.0f };

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    UltimateCompDSP dsp;
    int lastLatencySamples = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UltimateCompAudioProcessor)
};