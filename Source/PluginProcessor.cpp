/*
  ==============================================================================
    PluginProcessor.cpp
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
UltimateCompAudioProcessor::UltimateCompAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    )
#endif
    , apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
}

UltimateCompAudioProcessor::~UltimateCompAudioProcessor() {}

//==============================================================================
const juce::String UltimateCompAudioProcessor::getName() const { return JucePlugin_Name; }
bool UltimateCompAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}
bool UltimateCompAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}
bool UltimateCompAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}
double UltimateCompAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int UltimateCompAudioProcessor::getNumPrograms() { return 1; }
int UltimateCompAudioProcessor::getCurrentProgram() { return 0; }
void UltimateCompAudioProcessor::setCurrentProgram(int) {}
const juce::String UltimateCompAudioProcessor::getProgramName(int) { return {}; }
void UltimateCompAudioProcessor::changeProgramName(int, const juce::String&) {}

//==============================================================================
void UltimateCompAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    dsp.prepare(sampleRate, samplesPerBlock);
}

void UltimateCompAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool UltimateCompAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    const auto& mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != juce::AudioChannelSet::mono()
        && mainOut != juce::AudioChannelSet::stereo())
        return false;

#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif

void UltimateCompAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    // 1. UPDATE PARAMETERS
    dsp.p_thresh = *apvts.getRawParameterValue("thresh");
    dsp.p_ratio = *apvts.getRawParameterValue("ratio");
    dsp.p_knee = *apvts.getRawParameterValue("knee");
    dsp.p_att_ms = *apvts.getRawParameterValue("att_ms");
    dsp.p_rel_ms = *apvts.getRawParameterValue("rel_ms");
    dsp.p_auto_rel = (int)*apvts.getRawParameterValue("auto_rel");
    dsp.p_turbo = (int)*apvts.getRawParameterValue("turbo");

    dsp.p_signal_flow = (int)*apvts.getRawParameterValue("signal_flow");

    dsp.p_ctrl_mode = (int)*apvts.getRawParameterValue("ctrl_mode");
    dsp.p_crest_target = *apvts.getRawParameterValue("crest_target");
    dsp.p_crest_speed = *apvts.getRawParameterValue("crest_speed");

    dsp.p_thrust_mode = (int)*apvts.getRawParameterValue("thrust_mode");
    dsp.p_det_rms = *apvts.getRawParameterValue("det_rms");
    dsp.p_stereo_link = *apvts.getRawParameterValue("stereo_link");
    dsp.p_sc_hp_freq = *apvts.getRawParameterValue("sc_hp_freq");
    dsp.p_fb_blend = *apvts.getRawParameterValue("fb_blend");

    dsp.p_tp_mode = (int)*apvts.getRawParameterValue("tp_mode");
    dsp.p_tp_amount = *apvts.getRawParameterValue("tp_amount");
    dsp.p_tp_thresh_raise = *apvts.getRawParameterValue("tp_thresh_raise");

    dsp.p_flux_mode = (int)*apvts.getRawParameterValue("flux_mode");
    dsp.p_flux_amount = *apvts.getRawParameterValue("flux_amount");

    dsp.p_sat_mode = (int)*apvts.getRawParameterValue("sat_mode");
    dsp.p_sat_drive = *apvts.getRawParameterValue("sat_drive");
    dsp.p_sat_autogain = (int)*apvts.getRawParameterValue("sat_autogain"); // NEW
    dsp.p_sat_trim = *apvts.getRawParameterValue("sat_trim");
    dsp.p_sat_tone = *apvts.getRawParameterValue("sat_tone");
    dsp.p_sat_tone_freq = *apvts.getRawParameterValue("sat_tone_freq");
    dsp.p_sat_mix = *apvts.getRawParameterValue("sat_mix");

    dsp.p_harm_bright = *apvts.getRawParameterValue("harm_bright");
    dsp.p_harm_freq = *apvts.getRawParameterValue("harm_freq");

    dsp.p_makeup = *apvts.getRawParameterValue("makeup");
    dsp.p_dry_wet = *apvts.getRawParameterValue("dry_wet");
    dsp.p_out_trim = *apvts.getRawParameterValue("out_trim");

    dsp.updateParameters();

    // 2. MEASURE INPUT
    float inL = (numCh > 0) ? buffer.getMagnitude(0, 0, numSamples) : 0.0f;
    float inR = (numCh > 1) ? buffer.getMagnitude(1, 0, numSamples) : inL;

    // 3. PROCESS
    dsp.process(buffer);

    // 4. MEASURE OUTPUT
    float outL = (numCh > 0) ? buffer.getMagnitude(0, 0, numSamples) : 0.0f;
    float outR = (numCh > 1) ? buffer.getMagnitude(1, 0, numSamples) : outL;

    // 5. UPDATE METERS
    meterInL.store(inL);
    meterInR.store(inR);
    meterOutL.store(outL);
    meterOutR.store(outR);
    meterGR.store(dsp.getGainReductiondB());
    meterFlux.store(dsp.getFluxSaturation());
}

//==============================================================================
bool UltimateCompAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* UltimateCompAudioProcessor::createEditor()
{
    return new UltimateCompAudioProcessorEditor(*this);
}

//==============================================================================
void UltimateCompAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void UltimateCompAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout UltimateCompAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Main
    layout.add(std::make_unique<juce::AudioParameterFloat>("thresh", "Threshold", -60.0f, 0.0f, -20.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("ratio", "Ratio", 1.0f, 20.0f, 4.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("knee", "Knee", 0.0f, 24.0f, 6.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("att_ms", "Attack", 0.1f, 200.0f, 10.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("rel_ms", "Release", 10.0f, 2000.0f, 100.0f));
    layout.add(std::make_unique<juce::AudioParameterChoice>("auto_rel", "Auto Release", juce::StringArray{ "Manual", "Auto" }, 0));

    layout.add(std::make_unique<juce::AudioParameterBool>("turbo", "Turbo Timing", false));

    layout.add(std::make_unique<juce::AudioParameterChoice>("signal_flow", "Signal Flow",
        juce::StringArray{ "Comp > Sat", "Sat > Comp" }, 0));

    layout.add(std::make_unique<juce::AudioParameterChoice>("ctrl_mode", "Control Mode",
        juce::StringArray{ "Manual", "Auto Crest" }, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>("crest_target", "Crest Target", 6.0f, 20.0f, 12.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("crest_speed", "Crest Speed", 50.0f, 4000.0f, 400.0f));

    layout.add(std::make_unique<juce::AudioParameterChoice>("thrust_mode", "Thrust",
        juce::StringArray{ "Normal", "Med (Shelf)", "Loud (Pink)" }, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>("det_rms", "RMS Window", 0.0f, 300.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("stereo_link", "Stereo Link %", 0.0f, 100.0f, 100.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("sc_hp_freq", "SC HPF", 20.0f, 500.0f, 20.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("fb_blend", "Feedback Blend %", 0.0f, 100.0f, 0.0f));

    layout.add(std::make_unique<juce::AudioParameterChoice>("tp_mode", "Transient Priority", juce::StringArray{ "Off", "On" }, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>("tp_amount", "TP Amount %", 0.0f, 100.0f, 50.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("tp_thresh_raise", "TP Raise (dB)", 0.0f, 24.0f, 12.0f));

    layout.add(std::make_unique<juce::AudioParameterChoice>("flux_mode", "Flux-Coupled", juce::StringArray{ "Off", "On" }, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>("flux_amount", "Flux Amount %", 0.0f, 100.0f, 30.0f));

    layout.add(std::make_unique<juce::AudioParameterChoice>("sat_mode", "Transformer", juce::StringArray{ "Clean", "Iron", "Steel" }, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>("sat_drive", "Sat Drive", 0.0f, 24.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>("sat_autogain", "Sat Auto Gain", false)); // NEW
    layout.add(std::make_unique<juce::AudioParameterFloat>("sat_trim", "Sat Trim", -24.0f, 0.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("sat_tone", "Sat Tone", -12.0f, 12.0f, 3.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("sat_tone_freq", "Sat Tone Freq", 1000.0f, 12000.0f, 5500.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("sat_mix", "Sat Mix %", 0.0f, 100.0f, 100.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("harm_bright", "Harm Bright", -12.0f, 12.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("harm_freq", "Harm Freq", 1000.0f, 12000.0f, 4500.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>("makeup", "Makeup Gain", 0.0f, 24.0f, 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("dry_wet", "Dry/Wet %", 0.0f, 100.0f, 100.0f));

    // UPDATED: Output Trim -24 to +24
    layout.add(std::make_unique<juce::AudioParameterFloat>("out_trim", "Output", -24.0f, 24.0f, 0.0f));

    return layout;
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UltimateCompAudioProcessor();
}