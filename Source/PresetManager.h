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
    inline static const juce::String PRESET_EXTENSION = ".xml";

    PresetManager(juce::AudioProcessorValueTreeState& apvts) : valueTreeState(apvts)
    {
        juce::File root = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);

#if JUCE_MAC
        root = root.getChildFile("Audio").getChildFile("Presets").getChildFile("NS_BusComp");
#else
        root = root.getChildFile("NS_BusComp");
#endif

        if (!root.exists())
            root.createDirectory();

        defaultDirectory = root;
    }

    void savePreset(const juce::String& presetName)
    {
        // FIX B: Sanitize name to prevent path traversal
        const juce::String safeName = sanitizePresetPath(presetName);
        if (safeName.isEmpty()) return;

        const auto xml = valueTreeState.copyState().createXml();
        const auto file = defaultDirectory.getChildFile(safeName + PRESET_EXTENSION);

        // Create parent directory explicitly for nested preset saves
        file.getParentDirectory().createDirectory();

        if (!xml->writeTo(file))
        {
            DBG("Could not write preset to file: " + file.getFullPathName());
        }
        else
        {
            currentPresetName = safeName;
        }
    }

    void deletePreset(const juce::String& presetName)
    {
        const auto file = defaultDirectory.getChildFile(presetName + PRESET_EXTENSION);
        if (file.exists())
        {
            file.deleteFile();
            if (currentPresetName == presetName)
                currentPresetName = "<No Preset>";
        }
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
                currentPresetName = presetName; // Track currently loaded
            }
        }
    }

    juce::String getCurrentPresetName() const
    {
        return currentPresetName;
    }

    // Returns a list of all presets (names only, no extension)
    juce::StringArray refreshAndGetAllPresets()
    {
        allPresets.clear();

        // Recursive search for nested folders
        auto results = defaultDirectory.findChildFiles(juce::File::findFiles, true, "*" + PRESET_EXTENSION);

        for (const auto& file : results)
        {
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

    // FIX B: Real state tracking
    juce::String currentPresetName = "<No Preset>";

    // FIX B: Sanitization to prevent illegal chars and directory jumping
    juce::String sanitizePresetPath(juce::String name)
    {
        while (name.contains(".."))
            name = name.replace("..", "");

        return name.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_- ./\\");
    }
};