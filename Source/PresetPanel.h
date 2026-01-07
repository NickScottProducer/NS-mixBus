/*
  ==============================================================================
    PresetPanel.h
    GUI for the Preset Browser (No Dropdowns, Search enabled).
  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "PresetManager.h"

class PresetPanel : public juce::Component,
    public juce::TextEditor::Listener,
    public juce::ListBoxModel
{
public:
    PresetPanel(PresetManager& pm, juce::LookAndFeel& lnf)
        : manager(pm), customLnf(lnf)
    {
        // --- Search Bar ---
        searchBar.addListener(this);
        searchBar.setTextToShowWhenEmpty("Search Presets...", juce::Colours::grey);
        searchBar.setJustification(juce::Justification::centredLeft);
        addAndMakeVisible(searchBar);

        // --- List Box ---
        presetList.setModel(this);
        presetList.setRowHeight(24);
        presetList.setMultipleSelectionEnabled(false);
        addAndMakeVisible(presetList);

        // --- Name Editor (For saving) ---
        nameEditor.setTextToShowWhenEmpty("New Preset Name...", juce::Colours::grey);
        nameEditor.setJustification(juce::Justification::centred);
        addAndMakeVisible(nameEditor);

        // --- Buttons ---
        auto configBtn = [&](juce::TextButton& b, juce::String txt) {
            b.setButtonText(txt);
            b.setClickingTogglesState(false);
            addAndMakeVisible(b);
            };
        configBtn(loadBtn, "LOAD");
        configBtn(saveBtn, "SAVE");
        configBtn(deleteBtn, "DELETE");
        configBtn(cancelBtn, "X");

        // Logic
        loadBtn.onClick = [this] { loadPreset(); };
        saveBtn.onClick = [this] { savePreset(); };
        deleteBtn.onClick = [this] { deletePreset(); };
        cancelBtn.onClick = [this] { setVisible(false); };

        loadData();
    }

    ~PresetPanel() override {}

    void paint(juce::Graphics& g) override
    {
        // --- 1. Background Gradient (Matches Main Plugin) ---
        // Hex codes copied from your UltimateLNF palette
        juce::Colour bgA(0xff0a0910);
        juce::Colour bgB(0xff14121d);

        // We make it fully opaque (or very slight transparency) so it covers the knobs below clearly
        g.setGradientFill(juce::ColourGradient(bgA, 0, 0, bgB, 0, (float)getHeight(), false));
        g.fillAll();

        // --- 2. Blueprint Grid ---
        g.setColour(juce::Colour(0xff2a2438).withAlpha(0.30f)); // Grid line color

        // Draw vertical lines
        for (int x = 0; x < getWidth(); x += 20)
            g.drawVerticalLine(x, 0.0f, (float)getHeight());

        // Draw horizontal lines
        for (int y = 0; y < getHeight(); y += 20)
            g.drawHorizontalLine(y, 0.0f, (float)getWidth());

        // --- 3. Border & Title ---
        g.setColour(juce::Colour(0xff382e4d)); // Edge color
        g.drawRect(getLocalBounds(), 1);

        g.setColour(juce::Colour(0xffe6e1ff)); // Text color
        g.setFont(juce::FontOptions(18.0f).withStyle("bold"));
        g.drawText("PRESET BROWSER", 20, 15, 200, 25, juce::Justification::left);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(20);

        // Header
        auto header = r.removeFromTop(30);
        cancelBtn.setBounds(header.removeFromRight(30));

        r.removeFromTop(10);

        // Search
        searchBar.setBounds(r.removeFromTop(24));
        r.removeFromTop(10);

        // Footer (Save/Load)
        auto footer = r.removeFromBottom(30);
        int btnW = 80;
        loadBtn.setBounds(footer.removeFromRight(btnW));
        footer.removeFromRight(10);
        deleteBtn.setBounds(footer.removeFromLeft(btnW));

        // Save Row (Above Footer)
        r.removeFromBottom(10);
        auto saveRow = r.removeFromBottom(24);
        saveBtn.setBounds(saveRow.removeFromRight(btnW));
        saveRow.removeFromRight(10);
        nameEditor.setBounds(saveRow);

        r.removeFromBottom(10);

        // List
        presetList.setBounds(r);
    }

    // --- ListBoxModel Methods ---
    int getNumRows() override { return filteredPresets.size(); }

    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (rowIsSelected)
            g.fillAll(juce::Colour(0xffbd00ff).withAlpha(0.2f)); // Accent color

        g.setColour(juce::Colour(0xffe6e1ff)); // Text color
        g.setFont(14.0f);
        g.drawText(filteredPresets[row], 5, 0, width - 10, height, juce::Justification::centredLeft);
    }

    void listBoxItemClicked(int row, const juce::MouseEvent&) override
    {
        presetList.selectRow(row);
    }

    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override
    {
        presetList.selectRow(row);
        loadPreset();
    }

    // --- TextEditor Listener ---
    void textEditorTextChanged(juce::TextEditor& editor) override
    {
        if (&editor == &searchBar)
            filterList(searchBar.getText());
    }

    void setVisibility(bool shouldShow)
    {
        setVisible(shouldShow);
        if (shouldShow)
        {
            loadData();
            toFront(true);
        }
    }

private:
    PresetManager& manager;
    juce::LookAndFeel& customLnf;

    juce::TextEditor searchBar;
    juce::ListBox presetList;

    juce::TextEditor nameEditor;
    juce::TextButton loadBtn, saveBtn, deleteBtn, cancelBtn;

    juce::StringArray allPresets;
    juce::StringArray filteredPresets;

    void loadData()
    {
        allPresets = manager.getAllPresets();
        filterList(searchBar.getText());
    }

    void filterList(const juce::String& text)
    {
        filteredPresets.clear();
        if (text.isEmpty())
        {
            filteredPresets = allPresets;
        }
        else
        {
            for (const auto& p : allPresets)
                if (p.containsIgnoreCase(text))
                    filteredPresets.add(p);
        }
        presetList.updateContent();
        repaint();
    }

    void loadPreset()
    {
        int row = presetList.getSelectedRow();
        if (row >= 0 && row < filteredPresets.size())
        {
            manager.loadPreset(filteredPresets[row]);
            // Optional: Close on load
            // setVisible(false); 
        }
    }

    void savePreset()
    {
        juce::String name = nameEditor.getText();
        if (name.isNotEmpty())
        {
            manager.savePreset(name);
            loadData();
            nameEditor.clear();
        }
    }

    void deletePreset()
    {
        int row = presetList.getSelectedRow();
        if (row >= 0 && row < filteredPresets.size())
        {
            manager.deletePreset(filteredPresets[row]);
            loadData();
        }
    }
};