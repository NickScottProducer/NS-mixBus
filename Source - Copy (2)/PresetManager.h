/*
  ==============================================================================
    PresetManager.h
    Handles file I/O for the Internal Preset System.
  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>

class PresetManager
{
public:
    // FIXED: Added 'inline' to prevent multiple definition linker errors
    inline static const juce::String PRESET_EXTENSION = ".xml";

    PresetManager(juce::AudioProcessorValueTreeState& apvts) : valueTreeState(apvts)
    {
        // Windows: %APPDATA%\NS_bussStuff
        // Mac: ~/Library/Application Support/NS_bussStuff
        juce::File root = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);

#if JUCE_MAC
        root = root.getChildFile("Audio").getChildFile("Presets").getChildFile("NS_bussStuff");
#else
        root = root.getChildFile("NS_bussStuff");
#endif

        if (!root.exists())
            root.createDirectory();

        defaultDirectory = root;
    }

    void savePreset(const juce::String& presetName)
    {
        const auto xml = valueTreeState.copyState().createXml();
        const auto file = defaultDirectory.getChildFile(presetName + PRESET_EXTENSION);
        if (!xml->writeTo(file))
        {
            DBG("Could not write preset to file: " + file.getFullPathName());
        }
    }

    void deletePreset(const juce::String& presetName)
    {
        const auto file = defaultDirectory.getChildFile(presetName + PRESET_EXTENSION);
        if (file.exists())
            file.deleteFile();
    }

    void loadPreset(const juce::String& presetName)
    {
        const auto file = defaultDirectory.getChildFile(presetName + PRESET_EXTENSION);
        if (file.existsAsFile())
        {
            const auto xml = juce::parseXML(file);
            if (xml != nullptr && xml->hasTagName(valueTreeState.state.getType()))
            {
                valueTreeState.replaceState(juce::ValueTree::fromXml(*xml));
            }
        }
    }

    int getLoadPresetIndex() const { return currentPresetIndex; }

    juce::String getCurrentPresetName() const
    {
        if (currentPresetIndex < 0 || currentPresetIndex >= allPresets.size())
            return "<No Preset>";
        return allPresets[currentPresetIndex];
    }

    // Returns a list of all presets (names only, no extension)
    juce::StringArray getAllPresets()
    {
        allPresets.clear();

        // Recursive search for nested folders
        auto results = defaultDirectory.findChildFiles(juce::File::findFiles, true, "*" + PRESET_EXTENSION);

        for (const auto& file : results)
        {
            // Store relative path so "Bass/Smash" shows up as such
            auto relativePath = file.getRelativePathFrom(defaultDirectory);
            relativePath = relativePath.dropLastCharacters(PRESET_EXTENSION.length());
            allPresets.add(relativePath);
        }

        allPresets.sort(true);
        return allPresets;
    }

private:
    juce::AudioProcessorValueTreeState& valueTreeState;
    juce::File defaultDirectory;
    juce::StringArray allPresets;
    int currentPresetIndex = -1;
};