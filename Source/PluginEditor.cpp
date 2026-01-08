/*
  ==============================================================================
    PluginEditor.cpp
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <functional>

//=============================================================================
static inline juce::Rectangle<float> shrinkToSquare(juce::Rectangle<float> r)
{
    const auto centre = r.getCentre();
    const float s = juce::jmin(r.getWidth(), r.getHeight());
    r.setSize(s, s);
    r.setCentre(centre);
    return r;
}

//=============================================================================
// LookAndFeel
class UltimateCompAudioProcessorEditor::UltimateLNF final : public juce::LookAndFeel_V4
{
public:
    UltimateLNF()
    {
        setColour(juce::Slider::thumbColourId, juce::Colours::transparentBlack);
        setColour(juce::ComboBox::backgroundColourId, c(bgA));
        setColour(juce::ComboBox::textColourId, c(text));
        setColour(juce::ComboBox::outlineColourId, c(edge));
        setColour(juce::ComboBox::arrowColourId, c(accent));
        setColour(juce::ToggleButton::textColourId, c(text));
        setColour(juce::PopupMenu::backgroundColourId, c(bgB));
        setColour(juce::PopupMenu::textColourId, c(text));
        setColour(juce::Label::backgroundColourId, c(bgB));
        setColour(juce::Label::textColourId, c(accent));
        setColour(juce::Label::outlineColourId, c(accent));
        setColour(juce::TextEditor::backgroundColourId, c(bgB));
        setColour(juce::TextEditor::textColourId, c(white));
        setColour(juce::TextEditor::highlightColourId, c(accent).withAlpha(0.4f));

        // Tooltip Colors
        setColour(juce::TooltipWindow::backgroundColourId, c(bgB).darker());
        setColour(juce::TooltipWindow::textColourId, c(text));
        setColour(juce::TooltipWindow::outlineColourId, c(accent));

        // TextButton (For Presets button)
        setColour(juce::TextButton::buttonColourId, c(panel2));
        setColour(juce::TextButton::buttonOnColourId, c(panel2).brighter());
        setColour(juce::TextButton::textColourOffId, c(text2));
        setColour(juce::TextButton::textColourOnId, c(white));
    }

    enum Palette { bgA, bgB, panel, panel2, edge, text, text2, accent, accent2, ok, warn, white, line };

    juce::Colour c(Palette type)
    {
        switch (type) {
        case bgA:     return juce::Colour(0xff0a0910);
        case bgB:     return juce::Colour(0xff14121d);
        case panel:   return juce::Colour(0x00000000);
        case panel2:  return juce::Colour(0xff110f18);
        case edge:    return juce::Colour(0xff382e4d);
        case text:    return juce::Colour(0xffe6e1ff);
        case text2:   return juce::Colour(0xff9085ad);
        case accent:  return juce::Colour(0xffbd00ff);
        case accent2: return juce::Colour(0xffd966ff);
        case ok:      return juce::Colour(0xff00f2ff);
        case warn:    return juce::Colour(0xffff0055);
        case white:   return juce::Colours::white;
        case line:    return juce::Colour(0xff2a2438);
        }
        return juce::Colours::red;
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override
    {
        // Handle Disabled Appearance
        if (!slider.isEnabled())
            g.setOpacity(0.4f);

        auto bounds = shrinkToSquare(juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height)).reduced(2.0f);
        const float radius = bounds.getWidth() * 0.5f;
        const auto centre = bounds.getCentre();
        const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        const float trackWidth = 4.0f;
        juce::Path track;
        track.addCentredArc(centre.x, centre.y, radius - trackWidth, radius - trackWidth, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(c(panel2).darker(0.3f));
        g.strokePath(track, juce::PathStrokeType(trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path val;
        val.addCentredArc(centre.x, centre.y, radius - trackWidth, radius - trackWidth, 0.0f, rotaryStartAngle, angle, true);
        g.setColour(c(accent).withAlpha(0.3f));
        g.strokePath(val, juce::PathStrokeType(trackWidth + 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(c(accent));
        g.strokePath(val, juce::PathStrokeType(trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        float knobR = radius - 10.0f;
        g.setColour(c(bgA).withAlpha(0.8f));
        g.fillEllipse(centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);
        g.setColour(c(edge));
        g.drawEllipse(centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f, 1.5f);

        float dotR = 3.0f;
        float dotDist = knobR - 5.0f;
        float dotX = centre.x + std::cos(angle - juce::MathConstants<float>::halfPi) * dotDist;
        float dotY = centre.y + std::sin(angle - juce::MathConstants<float>::halfPi) * dotDist;
        g.setColour(c(accent2));
        g.fillEllipse(dotX - dotR, dotY - dotR, dotR * 2, dotR * 2);

        // Reset opacity for subsequent drawing
        g.setOpacity(1.0f);
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool, bool) override
    {
        if (b.getButtonText() == "?") {
            // Special "Help" button style
            auto r = b.getLocalBounds().toFloat().reduced(4.0f);

            if (b.getToggleState())
            {
                // ENABLED: Illuminated Purple Fill
                g.setColour(c(accent));
                g.fillEllipse(r);
                g.setColour(c(white)); // Text White
            }
            else
            {
                // DISABLED: Grey Outline / Empty
                g.setColour(c(bgB));
                g.fillEllipse(r);
                g.setColour(c(text2));
                g.drawEllipse(r, 1.5f);
            }

            g.setFont(juce::FontOptions(14.0f).withStyle("bold"));
            g.drawText("?", r, juce::Justification::centred);
            return;
        }

        if (b.getButtonText().isEmpty()) return;
        auto r = b.getLocalBounds().toFloat().reduced(2.0f);

        // ALLOW LONGER TEXT FOR BADGES like "Mirror" or "SC->Comp"
        // UPDATED: Threshold increased to accommodate "Faster/Harder"
        bool isSmall = b.getButtonText().length() <= 16;

        const float boxW = isSmall ? r.getWidth() : 32.0f;
        auto box = r;
        if (!isSmall) {
            // Checkbox style for very long text (fallback)
            const float stdH = 16.0f;
            box = r.withWidth(boxW).withHeight(stdH).withY(r.getCentreY() - stdH / 2);
        }
        bool on = b.getToggleState();
        g.setColour(c(panel2));
        g.fillRoundedRectangle(box, 4.0f);
        g.setColour(c(edge));
        g.drawRoundedRectangle(box, 4.0f, 1.0f);
        if (on) {
            g.setColour(c(accent).withAlpha(0.2f));
            g.fillRoundedRectangle(box, 4.0f);
            g.setColour(c(accent));
            g.drawRoundedRectangle(box, 4.0f, 1.0f);
        }

        // Draw Text inside the box (Badge style)
        g.setColour(on ? c(white) : c(text2));
        g.setFont(juce::FontOptions(11.0f).withStyle("bold"));
        g.drawFittedText(b.getButtonText(), box.toNearestInt(), juce::Justification::centred, 1);
    }

    // Custom Text Button drawing for the "PRESETS" button
    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
        bool isMouseOverButton, bool isButtonDown) override
    {
        // FIXED: Silenced unused warning
        juce::ignoreUnused(isButtonDown);

        auto r = button.getLocalBounds().toFloat();
        g.setColour(backgroundColour);
        g.fillRoundedRectangle(r, 4.0f);

        g.setColour(c(edge));
        g.drawRoundedRectangle(r, 4.0f, 1.0f);

        if (isMouseOverButton)
        {
            g.setColour(c(accent).withAlpha(0.2f));
            g.fillRoundedRectangle(r, 4.0f);
        }
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox&) override
    {
        auto r = juce::Rectangle<float>(0, 0, (float)width, (float)height).reduced(1.0f);
        g.setColour(c(bgA).withAlpha(0.6f));
        g.fillRoundedRectangle(r, 4.0f);
        g.setColour(c(edge));
        g.drawRoundedRectangle(r, 4.0f, 1.0f);
        auto arrow = r.removeFromRight(20.0f);
        juce::Path p;
        p.addTriangle(arrow.getCentreX() - 3, arrow.getCentreY() - 2, arrow.getCentreX() + 3, arrow.getCentreY() - 2, arrow.getCentreX(), arrow.getCentreY() + 3);
        g.setColour(c(accent));
        g.fillPath(p);
    }
};

//=============================================================================
class UltimateCompAudioProcessorEditor::Knob final : public juce::Component
{
public:
    explicit Knob(UltimateLNF& lookAndFeel, juce::String labelText)
        : lnf(lookAndFeel), labelTitle(std::move(labelText))
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setLookAndFeel(&lnf);
        addAndMakeVisible(slider);

        valueLabel.setJustificationType(juce::Justification::centred);
        valueLabel.setEditable(false, true, false);
        valueLabel.setLookAndFeel(&lnf);

        valueLabel.setColour(juce::Label::textColourId, lnf.c(UltimateLNF::accent));
        valueLabel.setColour(juce::TextEditor::textColourId, lnf.c(UltimateLNF::white));
        valueLabel.setColour(juce::TextEditor::backgroundColourId, lnf.c(UltimateLNF::bgB));

        valueLabel.onTextChange = [this]
            {
                const auto t = valueLabel.getText();
                const double val = valueParser ? valueParser(t) : t.getDoubleValue();
                slider.setValue(val, juce::sendNotification);
            };
        addAndMakeVisible(valueLabel);
        slider.onValueChange = [this] { updateLabelText(); if (onValChange) onValChange(); };
    }
    ~Knob() override { slider.setLookAndFeel(nullptr); valueLabel.setLookAndFeel(nullptr); }
    juce::Slider& getSlider() noexcept { return slider; }
    void setUnitSuffix(juce::String s) { suffix = std::move(s); }
    void setTextFromValue(std::function<juce::String(double)> fn) { textFromValue = std::move(fn); }
    void setValueParser(std::function<double(const juce::String&)> fn) { valueParser = std::move(fn); }
    std::function<void()> onValChange;
    void updateLabelText() {
        double v = slider.getValue();
        juce::String s = textFromValue ? textFromValue(v) : slider.getTextFromValue(v);
        if (suffix.isNotEmpty() && !s.contains(suffix)) s << suffix;
        valueLabel.setText(s, juce::dontSendNotification);
    }
    void paint(juce::Graphics& g) override {
        // Handle Disabled Appearance
        if (!isEnabled()) g.setOpacity(0.4f);

        auto b = getLocalBounds();
        const int textH = juce::jlimit(12, 20, (int)std::lround(getHeight() * 0.18f));
        auto top = b.removeFromTop(textH);
        const float fontPx = juce::jlimit(10.0f, 15.0f, (float)textH * 0.80f);
        g.setColour(lnf.c(UltimateLNF::text2));
        g.setFont(juce::FontOptions(fontPx));
        g.drawFittedText(labelTitle, top, juce::Justification::centred, 1);

        // Reset Opacity
        g.setOpacity(1.0f);
    }

    void enablementChanged() override
    {
        // Grey-out the actual rotary control (child slider) when this Knob is disabled.
        const bool en = isEnabled();

        slider.setEnabled(en);
        valueLabel.setEnabled(en);

        // Force a visible dim even if the LookAndFeel doesn't special-case disabled sliders.
        slider.setAlpha(en ? 1.0f : 0.35f);
        // Keep label readable; its disabled colours already grey it out.
        valueLabel.setAlpha(1.0f);

        repaint();
    }

    void resized() override {
        const int textH = juce::jlimit(12, 20, (int)std::lround(getHeight() * 0.18f));
        auto r = getLocalBounds();
        r.removeFromTop(textH);
        auto bot = r.removeFromBottom(textH);
        valueLabel.setBounds(bot);
        valueLabel.setFont(juce::FontOptions(juce::jlimit(10.0f, 15.0f, (float)textH * 0.80f)));
        slider.setBounds(r.reduced(2));
    }
private:
    UltimateLNF& lnf;
    juce::String labelTitle, suffix;
    juce::Slider slider;
    juce::Label valueLabel;
    std::function<juce::String(double)> textFromValue;
    std::function<double(const juce::String&)> valueParser;
};

//=============================================================================
class UltimateCompAudioProcessorEditor::Panel final : public juce::Component
{
public:
    Panel(juce::String titleText, UltimateLNF& lookAndFeel)
        : title(std::move(titleText)), lnf(lookAndFeel) {
    }

    void setHeaderHeight(int h) { headerH = juce::jmax(0, h); repaint(); }
    juce::Rectangle<int> getContentBounds() const { return getLocalBounds().reduced(8).withTrimmedTop(headerH); }
    void paint(juce::Graphics& g) override {
        auto r = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(lnf.c(UltimateLNF::bgA).withAlpha(0.95f));
        g.fillRoundedRectangle(r, 6.0f);
        g.setColour(lnf.c(UltimateLNF::edge));
        g.drawRoundedRectangle(r, 6.0f, 1.5f);
        if (headerH > 0) {
            auto header = r.removeFromTop((float)headerH);
            g.setColour(lnf.c(UltimateLNF::bgB).withAlpha(0.6f));
            g.fillRoundedRectangle(header.getX(), header.getY(), header.getWidth(), header.getHeight(), 6.0f);
            g.setColour(lnf.c(UltimateLNF::text));
            const float headerFontPx = juce::jlimit(11.0f, 18.0f, header.getHeight() * 0.55f);
            g.setFont(juce::FontOptions(headerFontPx).withStyle("bold"));
            g.drawText(title, header.reduced(10, 0), juce::Justification::centred);
            g.setColour(lnf.c(UltimateLNF::edge));
            g.drawHorizontalLine((int)header.getBottom(), header.getX(), header.getRight());
        }
    }
    void resized() override {}
private:
    juce::String title;
    int headerH = 26;
    UltimateLNF& lnf;
};

//=============================================================================
UltimateCompAudioProcessorEditor::UltimateCompAudioProcessorEditor(UltimateCompAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    lnf = std::make_unique<UltimateLNF>();

    // LOAD LOGO
    pluginLogo = juce::ImageCache::getFromMemory(BinaryData::logo_png, BinaryData::logo_pngSize);

    setLookAndFeel(lnf.get());

    auto makeKnob = [&](const juce::String& name, UltimateLNF& theme) {
        auto k = std::make_unique<Knob>(theme, name);
        k->onValChange = [k = k.get()] { k->updateLabelText(); };
        return k;
        };

    // --- 1. Create Knobs ---
    kThresh = makeKnob("Threshold", *lnf); kThresh->setUnitSuffix("dB");
    kRatio = makeKnob("Ratio", *lnf); kRatio->setTextFromValue([](double v) { return juce::String(v, 1) + ":1"; });
    kKnee = makeKnob("Knee", *lnf); kKnee->setUnitSuffix("dB");
    kAttack = makeKnob("Attack", *lnf);
    kAttack->setTextFromValue([this](double v) { return juce::String(bTurboAtt.getToggleState() ? (v * 0.1) : v, 2) + " ms"; });
    kRelease = makeKnob("Release", *lnf);
    kRelease->setTextFromValue([this](double v) { return juce::String(bTurboRel.getToggleState() ? (v * 0.1) : v, 2) + " ms"; });

    kCompInput = makeKnob("Input", *lnf); kCompInput->setUnitSuffix("dB");
    kCompInput->onValChange = [this] {
        kCompInput->updateLabelText();
        float currentVal = (float)kCompInput->getSlider().getValue();
        if (bCompMirror.getToggleState() && !ignoreCallbacks) {
            ignoreCallbacks = true;
            float delta = currentVal - lastCompInputVal;
            double newMakeup = kMakeup->getSlider().getValue() - delta;
            kMakeup->getSlider().setValue(newMakeup, juce::sendNotificationSync);
            ignoreCallbacks = false;
        }
        if (auto* param = audioProcessor.apvts.getParameter("comp_input"))
            lastCompInputVal = param->convertFrom0to1(param->getValue());
        };

    kMakeup = makeKnob("Output", *lnf); kMakeup->setUnitSuffix("dB");
    kMix = makeKnob("Mix", *lnf); kMix->setUnitSuffix("%");

    kScHpf = makeKnob("Low Cut", *lnf); kScHpf->setUnitSuffix("Hz");
    kScLpf = makeKnob("High Cut", *lnf); kScLpf->setUnitSuffix("Hz");
    kDetRms = makeKnob("RMS Window", *lnf); kDetRms->setUnitSuffix("ms");
    kStereoLink = makeKnob("Link", *lnf); kStereoLink->setUnitSuffix("%");
    kMsBalance = makeKnob("M/S Bal", *lnf); kMsBalance->setUnitSuffix("dB");
    kFbBlend = makeKnob("FB Blend", *lnf); kFbBlend->setUnitSuffix("%");
    kScLevel = makeKnob("SC Level", *lnf); kScLevel->setUnitSuffix("dB");


    kScTdAmt = makeKnob("TD Amt", *lnf); kScTdAmt->setUnitSuffix("%");
    kScTdMs = makeKnob("TD M/S", *lnf);
    kScTdMs->setTextFromValue([](double v)
        {
            v = juce::jlimit(0.0, 100.0, v);
            if (v < 0.5)  return juce::String("Mid");
            if (v > 99.5) return juce::String("Side");
            const int s = (int)std::round(v);
            const int m = 100 - s;
            return juce::String("M") + juce::String(m) + juce::String("/S") + juce::String(s);
        });

    kCrestTarget = makeKnob("Crest Target", *lnf); kCrestTarget->setUnitSuffix("dB");
    kCrestSpeed = makeKnob("Crest Speed", *lnf); kCrestSpeed->setUnitSuffix("ms");
    kTpAmt = makeKnob("Focus", *lnf); kTpAmt->setUnitSuffix("%");
    kTpRaise = makeKnob("Punch", *lnf); kTpRaise->setUnitSuffix("dB");
    kFluxAmt = makeKnob("Flux Amt", *lnf); kFluxAmt->setUnitSuffix("%");
    kSatPre = makeKnob("Pre-Gain", *lnf); kSatPre->setUnitSuffix("dB");
    kSatDrive = makeKnob("Drive", *lnf); kSatDrive->setUnitSuffix("dB");
    kSatTrim = makeKnob("Trim", *lnf); kSatTrim->setUnitSuffix("dB");
    kSatMix = makeKnob("Mix", *lnf); kSatMix->setUnitSuffix("%");

    kGirth = makeKnob("Girth", *lnf); kGirth->setUnitSuffix("dB");
    kGirthFreq = makeKnob("Freq", *lnf); kGirthFreq->setUnitSuffix("Hz");
    kGirthFreq->setTextFromValue([](double v) {
        static const int freqs[4] = { 20, 30, 60, 100 };
        int idx = (int)std::lround(v); idx = juce::jlimit(0, 3, idx);
        return juce::String(freqs[idx]);
        });
    kGirthFreq->setValueParser([](const juce::String& s) -> double {
        const auto digits = s.retainCharacters("0123456789");
        const int typed = digits.getIntValue();
        static const int freqs[4] = { 20, 30, 60, 100 };
        if (typed >= 0 && typed <= 3 && digits.length() <= 1) return (double)typed;
        int bestIdx = 0; int bestDist = std::abs(typed - freqs[0]);
        for (int i = 1; i < 4; ++i) { const int d = std::abs(typed - freqs[i]); if (d < bestDist) { bestDist = d; bestIdx = i; } }
        return (double)bestIdx;
        });

    kTone = makeKnob("Tilt", *lnf); kTone->setUnitSuffix("dB");
    kToneFreq = makeKnob("Freq", *lnf); kToneFreq->setUnitSuffix("Hz");
    kBright = makeKnob("Air", *lnf); kBright->setUnitSuffix("dB");
    kBrightFreq = makeKnob("Freq", *lnf); kBrightFreq->setUnitSuffix("Hz");
    // --- 2. Combos & Buttons ---
    auto prepCombo = [&](juce::ComboBox& b, UltimateLNF* theme = nullptr) {
        b.setJustificationType(juce::Justification::centred);
        b.setSize(90, 20);
        if (theme) b.setLookAndFeel(theme);
        };

    prepCombo(cAutoRel); cAutoRel.addItem("Manual", 1); cAutoRel.addItem("Auto", 2);
    prepCombo(cCompAutoGain); cCompAutoGain.addItem("AGC Off", 1); cCompAutoGain.addItem("Partial", 2); cCompAutoGain.addItem("Full", 3);
    prepCombo(cThrust); cThrust.addItem("Normal", 1); cThrust.addItem("Med", 2); cThrust.addItem("Loud", 3);
    prepCombo(cCtrlMode); cCtrlMode.addItem("Manual", 1); cCtrlMode.addItem("Auto", 2);
    prepCombo(cTpMode); cTpMode.addItem("Off", 1); cTpMode.addItem("On", 2);
    prepCombo(cFluxMode); cFluxMode.addItem("Off", 1); cFluxMode.addItem("On", 2);
    prepCombo(cSatMode); cSatMode.addItem("Clean", 1); cSatMode.addItem("Iron", 2); cSatMode.addItem("Steel", 3);
    prepCombo(cSignalFlow); cSignalFlow.addItem("Comp>Sat", 1); cSignalFlow.addItem("Sat>Comp", 2);
    prepCombo(cSatAutoGain); cSatAutoGain.addItem("Off", 1); cSatAutoGain.addItem("Partial", 2); cSatAutoGain.addItem("Full", 3);

    prepCombo(cScMode); cScMode.addItem("In", 1); cScMode.addItem("Ext", 2);
    prepCombo(cMsMode); cMsMode.addItem("Link", 1); cMsMode.addItem("Mid", 2); cMsMode.addItem("Side", 3);
    cMsMode.addItem("M>S", 4); cMsMode.addItem("S>M", 5);

    // CHANGED: "F" -> "Faster/Harder"
    bTurboAtt.setButtonText("Faster/Harder"); bTurboAtt.setClickingTogglesState(true); bTurboAtt.onClick = [this] { kAttack->updateLabelText(); };
    bTurboRel.setButtonText("Faster/Harder"); bTurboRel.setClickingTogglesState(true); bTurboRel.onClick = [this] { kRelease->updateLabelText(); };

    bMirror.setButtonText("Mirror"); bMirror.setClickingTogglesState(true);
    bCompMirror.setButtonText("Mirror"); bCompMirror.setClickingTogglesState(true);

    bScToComp.setButtonText("SC->Comp"); bScToComp.setClickingTogglesState(true);

    bHelp.setButtonText("?");
    bHelp.setClickingTogglesState(true);
    bHelp.setTooltip("Tooltips On/Off\nWhen enabled, hover any control to see detailed help.");
    bHelp.onClick = [this] {
        bool show = bHelp.getToggleState();
        if (show) tooltipWindow = std::make_unique<juce::TooltipWindow>(this, 400);
        else tooltipWindow.reset();
        };
    addAndMakeVisible(bHelp);

    // ADDED: Presets Button
    bPresets.setButtonText("PRESETS");
    bPresets.onClick = [this] {
        if (presetPanel) presetPanel->setVisibility(!presetPanel->isVisible());
        };
    addAndMakeVisible(bPresets);

    // --- 3. Panels ---
    panelDyn = std::make_unique<Panel>("Main Dynamics", *lnf);
    panelDet = std::make_unique<Panel>("Sidechain", *lnf);
    panelCrest = std::make_unique<Panel>("Crest", *lnf);
    panelTpFlux = std::make_unique<Panel>("Transient/Flux", *lnf);
    panelSat = std::make_unique<Panel>("Saturation", *lnf);
    panelEq = std::make_unique<Panel>("Color EQ", *lnf);

    addAndMakeVisible(*panelDyn); addAndMakeVisible(*panelDet); addAndMakeVisible(*panelCrest);
    addAndMakeVisible(*panelTpFlux); addAndMakeVisible(*panelSat); addAndMakeVisible(*panelEq);

    auto setupPowerBtn = [&](juce::ToggleButton& b, juce::String paramId, std::unique_ptr<ButtonAttachment>& att, juce::String tip) {
        b.setButtonText(""); b.setClickingTogglesState(true); b.setTooltip(tip);
        att = std::make_unique<ButtonAttachment>(audioProcessor.apvts, paramId, b);
        addAndMakeVisible(b);
        };
    setupPowerBtn(bActiveDyn, "active_dyn", aActiveDyn, "Main Dynamics on/off\nBypasses the compressor section (no gain reduction when off).");
    setupPowerBtn(bActiveDet, "active_det", aActiveDet, "Sidechain section on/off\nBypasses sidechain filtering/transient-focus controls (detector hears unshaped SC when off).");
    setupPowerBtn(bActiveCrest, "active_crest", aActiveCrest, "Crest control on/off\nDisables Auto-Crest targeting (Crest Target/Speed have no effect when off).");
    setupPowerBtn(bActiveTpFlux, "active_tf", aActiveTpFlux, "Transient/Flux on/off\nDisables Transient Priority and Flux Coupling behaviors.");
    setupPowerBtn(bActiveSat, "active_sat", aActiveSat, "Saturation on/off\nBypasses the transformer/saturation stage (clean pass when off).");
    setupPowerBtn(bActiveEq, "active_eq", aActiveEq, "Color EQ on/off\nBypasses the Girth/Tone/Air EQ stage.");

    // --- 5. Add Children ---
    panelDyn->addAndMakeVisible(*kThresh); panelDyn->addAndMakeVisible(*kRatio); panelDyn->addAndMakeVisible(*kKnee);
    panelDyn->addAndMakeVisible(*kAttack); panelDyn->addAndMakeVisible(bTurboAtt);
    panelDyn->addAndMakeVisible(*kRelease); panelDyn->addAndMakeVisible(bTurboRel);
    panelDyn->addAndMakeVisible(*kCompInput); panelDyn->addAndMakeVisible(*kMakeup);
    panelDyn->addAndMakeVisible(bCompMirror); panelDyn->addAndMakeVisible(cCompAutoGain);
    panelDyn->addAndMakeVisible(*kMix); panelDyn->addAndMakeVisible(cAutoRel);

    panelDet->addAndMakeVisible(*kScHpf); panelDet->addAndMakeVisible(*kScLpf); panelDet->addAndMakeVisible(*kDetRms);
    panelDet->addAndMakeVisible(*kStereoLink); panelDet->addAndMakeVisible(*kMsBalance); panelDet->addAndMakeVisible(*kFbBlend); panelDet->addAndMakeVisible(*kScLevel); panelDet->addAndMakeVisible(*kScTdAmt); panelDet->addAndMakeVisible(*kScTdMs);
    panelDet->addAndMakeVisible(cThrust); panelDet->addAndMakeVisible(cScMode); panelDet->addAndMakeVisible(cMsMode); panelDet->addAndMakeVisible(bScToComp);

    panelCrest->addAndMakeVisible(*kCrestTarget); panelCrest->addAndMakeVisible(*kCrestSpeed); panelCrest->addAndMakeVisible(cCtrlMode);
    panelTpFlux->addAndMakeVisible(*kTpAmt); panelTpFlux->addAndMakeVisible(*kTpRaise); panelTpFlux->addAndMakeVisible(*kFluxAmt);
    panelTpFlux->addAndMakeVisible(cTpMode); panelTpFlux->addAndMakeVisible(cFluxMode);

    panelSat->addAndMakeVisible(*kSatPre); panelSat->addAndMakeVisible(*kSatDrive); panelSat->addAndMakeVisible(*kSatTrim);
    panelSat->addAndMakeVisible(*kSatMix); panelSat->addAndMakeVisible(cSatMode); panelSat->addAndMakeVisible(cSatAutoGain);
    panelSat->addAndMakeVisible(cSignalFlow); panelSat->addAndMakeVisible(bMirror);

    panelEq->addAndMakeVisible(*kGirth); panelEq->addAndMakeVisible(*kGirthFreq);
    panelEq->addAndMakeVisible(*kTone); panelEq->addAndMakeVisible(*kToneFreq);
    panelEq->addAndMakeVisible(*kBright); panelEq->addAndMakeVisible(*kBrightFreq);
    // --- 6. Bindings ---
    bindKnob(*kThresh, aThresh, "thresh", "dB", "Threshold\nSets the level where compression starts. Lower = more gain reduction.");
    bindKnob(*kRatio, aRatio, "ratio", "", "Ratio\nControls how strongly levels above threshold are reduced (higher = harder compression).");
    bindKnob(*kKnee, aKnee, "knee", "dB", "Knee\nSoftens the transition around threshold. Higher = smoother, lower = harder.");
    bindKnob(*kAttack, aAttack, "att_ms", "ms", "Attack\nTime for gain reduction to engage. Lower clamps transients; higher lets punch through. Turbo enables 10x faster range.");
    bindKnob(*kRelease, aRelease, "rel_ms", "ms", "Release\nTime for gain reduction to recover. Lower = snappier/more movement; higher = smoother glue. Turbo enables 10x faster range.");
    bindKnob(*kCompInput, aCompInput, "comp_input", "dB", "Comp Input\nGain into the compressor. Use to drive more GR; Mirror can help keep loudness stable.");

    if (auto* param = audioProcessor.apvts.getParameter("comp_input"))
        lastCompInputVal = param->convertFrom0to1(param->getValue());

    bindKnob(*kMakeup, aMakeup, "makeup", "dB", "Makeup\nPost-compressor output gain. Use to match bypass level, or enable Auto Gain for compensation.");
    bindKnob(*kMix, aMix, "dry_wet", "%", "Mix\nParallel blend: 0% = dry, 100% = fully compressed.");
    bindKnob(*kScHpf, aScHpf, "sc_hp_freq", "Hz", "SC Low Cut\nHigh-pass filter for the detector. Raise to reduce low-end pumping (detection only).");
    bindKnob(*kScLpf, aScLpf, "sc_lp_freq", "Hz", "SC High Cut\nLow-pass filter for the detector. Lower to smooth spiky triggering (detection only).");
    bindKnob(*kDetRms, aDetRms, "det_rms", "ms", "Detector RMS Window\nAveraging time for RMS detection. Higher = smoother; lower = peakier/transient-driven.");
    bindKnob(*kStereoLink, aStereoLink, "stereo_link", "%", "Stereo Link\nLinks L/R detection. Higher keeps the stereo image stable; lower allows more independent action.");
    bindKnob(*kMsBalance, aMsBalance, "ms_balance", "dB", "M/S Balance\nOffsets Mid vs Side emphasis in cross-comp modes (M>S or S>M). Positive biases Mid; negative biases Side.");
    bindKnob(*kFbBlend, aFbBlend, "fb_blend", "%", "FF/FB Blend\nBlends feed-forward (punchy/precise) and feedback (smooth/glue) behavior.");
    bindKnob(*kScLevel, aScLevel, "sc_level_db", "dB", "Sidechain Level\nTrims detector input level. Raise to increase SC-driven GR; lower to reduce (useful for external SC calibration).");

    bindKnob(*kScTdAmt, aScTdAmt, "sc_td_amt", "%", "SC Transient Emphasis\nShapes what the detector hears. + emphasizes attack (reacts to hits); - emphasizes sustain/decay.");
    bindKnob(*kScTdMs, aScTdMs, "sc_td_ms", "", "SC Emphasis Focus (M/S)\nWhere transient emphasis is applied: 0 = Mid, 100 = Side.");

    bindKnob(*kCrestTarget, aCrestTarget, "crest_target", "dB", "Crest Target\nTarget Peak-RMS difference for Auto-Crest. Higher = more punch; lower = denser/flattened dynamics.");
    bindKnob(*kCrestSpeed, aCrestSpeed, "crest_speed", "ms", "Crest Speed\nHow quickly Auto-Crest adapts. Lower = faster tracking; higher = slower, smoother adjustments.");
    bindKnob(*kTpAmt, aTpAmt, "tp_amount", "%", "Transient Priority Amount\nPreserves attacks by easing compression on transients. Higher = more punch retention.");
    bindKnob(*kTpRaise, aTpRaise, "tp_thresh_raise", "dB", "Transient Threshold Raise\nRaises the effective threshold during detected transients so hits pass before the body is compressed.");
    bindKnob(*kFluxAmt, aFluxAmt, "flux_amount", "%", "Flux Coupling Amount\nLinks saturation drive and compressor behavior for a more 'alive' response (use with Flux Mode).");
    bindKnob(*kSatPre, aSatPre, "sat_pre_gain", "dB", "Saturation Pre-Gain\nGain into the saturation block. Drives the transformer harder for more harmonics.");
    bindKnob(*kSatDrive, aSatDrive, "sat_drive", "dB", "Saturation Drive\nAdds drive within the transformer model for more harmonic density and soft clipping/compression.");
    bindKnob(*kSatTrim, aSatTrim, "sat_trim", "dB", "Saturation Trim\nPost-saturation output trim for level matching. Combine with Mirror or AutoGain to keep loudness consistent.");
    bindKnob(*kSatMix, aSatMix, "sat_mix", "%", "Saturation Mix\nBlend between clean and saturated signal (parallel saturation).");
    bindKnob(*kGirth, aGirth, "girth", "dB", "Girth\nPultec-style low-end trick: resonant boost + complementary dip for thicker subs with less mud.");
    bindKnob(*kGirthFreq, aGirthFreq, "girth_freq", "Hz", "Girth Frequency\nSelects the center frequency for the Girth curve (20 / 30 / 60 / 100 Hz).");
    bindKnob(*kTone, aTone, "sat_tone", "dB", "Tone (Tilt)\nTilt EQ around Tone Frequency. + brightens, - darkens without a traditional shelf shape.");
    bindKnob(*kToneFreq, aToneFreq, "sat_tone_freq", "Hz", "Tone Frequency\nPivot/center frequency for the Tilt EQ.");
    bindKnob(*kBright, aBright, "harm_bright", "dB", "Air Shelf\nHigh-shelf boost for sheen/air. Adds top-end sparkle and harmonic brightness.");
    bindKnob(*kBrightFreq, aBrightFreq, "harm_freq", "Hz", "Air Shelf Frequency\nCorner frequency for the Air shelf.");
    initCombo(cAutoRel, aAutoRel, "auto_rel", "Release Mode\nManual uses the Release knob. Auto adapts release to program material for smoother glue and fewer artifacts.");
    initCombo(cCompAutoGain, aCompAutoGain, "comp_autogain", "Compressor Auto Gain\nAttempts to maintain loudness as gain reduction changes. Partial = subtle; Full = stronger makeup.");
    initCombo(cThrust, aThrust, "thrust_mode", "Thrust (Detector Voicing)\nPink-noise style weighting for the detector. Higher settings de-emphasize lows to reduce pumping.");
    initCombo(cCtrlMode, aCtrlMode, "ctrl_mode", "Crest Control Mode\nManual = standard compression. Auto engages crest-factor targeting using Crest Target and Crest Speed.");
    initCombo(cTpMode, aTpMode, "tp_mode", "Transient Priority\nOn preserves attacks by easing compression on transients (use TP Amount / TP Raise).");
    initCombo(cFluxMode, aFluxMode, "flux_mode", "Flux Coupling\nOn links saturation drive and compressor behavior (use Flux Amount).");
    initCombo(cSatMode, aSatMode, "sat_mode", "Transformer Model\nSelects saturation character: Clean (subtle), Iron (warmer/darker), Steel (more aggressive).");
    initCombo(cSatAutoGain, aSatAutoGain, "sat_autogain", "Saturation Auto Gain\nGain compensation through the saturation stage. Partial = conservative; Full = stronger level matching.");
    initCombo(cSignalFlow, aSignalFlow, "signal_flow", "Signal Flow\nComp>Sat = compress then add color. Sat>Comp = saturate first, then compress harmonics.");
    initCombo(cScMode, cScModeAtt, "sc_mode", "Sidechain Source\nIn uses the internal input. Ext uses the host sidechain input (typically channels 3/4).");
    initCombo(cMsMode, aMsMode, "ms_mode", "Mid/Side Mode\nLink = normal stereo. Mid/Side process that component only. M>S / S>M cross-comp one component from the other.");

    bTurboAtt.setTooltip("Turbo Attack Range\nExtends Attack into 10x faster times for tighter, more aggressive transient control.");
    aTurboAtt = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "turbo_att", bTurboAtt);
    bTurboRel.setTooltip("Turbo Release Range\nExtends Release into 10x faster times for snappier recovery and more rhythmic movement.");
    aTurboRel = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "turbo_rel", bTurboRel);
    bMirror.setTooltip("Mirror (Sat Pre/Trim)\nLinks Saturation Pre-Gain and Trim inversely to help keep output level steadier while driving harmonics.");
    aMirror = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "sat_mirror", bMirror);
    bCompMirror.setTooltip("Mirror (Comp Input/Makeup)\nLinks Compressor Input and Makeup inversely to keep loudness roughly constant while you drive the compressor.");
    aCompMirror = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "comp_mirror", bCompMirror);
    bScToComp.setTooltip("SC -> Comp Detector\nUses the sidechain signal as the compressor detector input (filtered/processed SC drives gain reduction).");
    aScToComp = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "sc_to_comp", bScToComp);
    aHelp = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "show_help", bHelp);

    if (bHelp.getToggleState()) tooltipWindow = std::make_unique<juce::TooltipWindow>(this, 400);

    // ADDED: Create Preset Panel (hidden by default)
    if (audioProcessor.presetManager)
    {
        presetPanel = std::make_unique<PresetPanel>(*audioProcessor.presetManager, *lnf);
        addChildComponent(presetPanel.get()); // addChild makes it invisible but valid
    }

    setResizable(true, true);
    setResizeLimits(1000, 600, 2000, 1200);
    setSize(1100, 680);
    startTimerHz(60);
}

UltimateCompAudioProcessorEditor::~UltimateCompAudioProcessorEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void UltimateCompAudioProcessorEditor::bindKnob(Knob& knob, std::unique_ptr<SliderAttachment>& attachment, const juce::String& paramID, const juce::String& suffix, const juce::String& tooltip)
{
    knob.getSlider().setName(paramID);
    knob.getSlider().setTooltip(tooltip);
    attachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, paramID, knob.getSlider());
    knob.setUnitSuffix(suffix);
    knob.updateLabelText();
}

void UltimateCompAudioProcessorEditor::initCombo(juce::ComboBox& box, std::unique_ptr<ComboBoxAttachment>& attachment, const juce::String& paramID, const juce::String& tooltip)
{
    box.setTooltip(tooltip);
    attachment = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, paramID, box);
}

void UltimateCompAudioProcessorEditor::timerCallback()
{
    const float decay = 0.85f;
    const float inL = audioProcessor.meterInL.load(std::memory_order_relaxed);
    const float inR = audioProcessor.meterInR.load(std::memory_order_relaxed);
    const float outL = audioProcessor.meterOutL.load(std::memory_order_relaxed);
    const float outR = audioProcessor.meterOutR.load(std::memory_order_relaxed);

    if (inL > smoothInL)  smoothInL = inL;  else smoothInL *= decay;
    if (inR > smoothInR)  smoothInR = inR;  else smoothInR *= decay;
    if (outL > smoothOutL) smoothOutL = outL; else smoothOutL *= decay;
    if (outR > smoothOutR) smoothOutR = outR; else smoothOutR *= decay;

    const float gr = audioProcessor.meterGR.load(std::memory_order_relaxed);
    smoothGR = (gr < smoothGR) ? gr : (gr * 0.2f + smoothGR * 0.8f);

    const float fl = audioProcessor.meterFlux.load(std::memory_order_relaxed);
    if (fl > smoothFlux) smoothFlux = fl; else smoothFlux *= decay;

    const float cr = audioProcessor.meterCrest.load(std::memory_order_relaxed);
    if (cr > smoothCrest) smoothCrest = cr; else smoothCrest *= decay;

    auto updateEnablement = [&](juce::Component& comp, bool shouldEnable) {
        if (comp.isEnabled() != shouldEnable) comp.setEnabled(shouldEnable);
        };

    int msMode = (int)*audioProcessor.apvts.getRawParameterValue("ms_mode");
    updateEnablement(*kMsBalance, msMode != 0);
    int ctrlMode = (int)*audioProcessor.apvts.getRawParameterValue("ctrl_mode");
    bool autoCrest = (ctrlMode == 1);
    updateEnablement(*kCrestTarget, autoCrest);
    updateEnablement(*kCrestSpeed, autoCrest);
    int tpMode = (int)*audioProcessor.apvts.getRawParameterValue("tp_mode");
    bool tpOn = (tpMode == 1);
    updateEnablement(*kTpAmt, tpOn);
    updateEnablement(*kTpRaise, tpOn);
    int fluxMode = (int)*audioProcessor.apvts.getRawParameterValue("flux_mode");
    bool fluxOn = (fluxMode == 1);
    updateEnablement(*kFluxAmt, fluxOn);

    repaint();
}

void UltimateCompAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(lnf->c(UltimateLNF::bgA));
    g.setGradientFill(juce::ColourGradient(lnf->c(UltimateLNF::bgA), 0, 0, lnf->c(UltimateLNF::bgB), 0, (float)getHeight(), false));
    g.fillAll();
    g.setColour(lnf->c(UltimateLNF::line).withAlpha(0.20f));
    for (int x = 0; x < getWidth(); x += 20) g.drawVerticalLine(x, 0.0f, (float)getHeight());
    for (int y = 0; y < getHeight(); y += 20) g.drawHorizontalLine(y, 0.0f, (float)getWidth());

    // --- DRAW COMPOSITE LOGO (Image + Text) ---
    const int headerX = 15;
    const int headerY = 15;
    const int logoSize = 60;
    const int gap = 10;
    const int textHeight = 32;

    if (pluginLogo.isValid())
    {
        g.setOpacity(1.0f);
        g.drawImageWithin(pluginLogo,
            headerX, headerY, logoSize, logoSize,
            juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize,
            false);

        g.setColour(lnf->c(UltimateLNF::text));
        g.setFont(juce::FontOptions((float)textHeight).withStyle("bold"));

        int textX = headerX + logoSize + gap;
        int textY = headerY + (logoSize - textHeight) / 2;
        int textW = 200;

        // CHANGED: "bussStuff" -> "NS - bussStuff"
        g.drawText("bussStuff", textX, textY, textW, textHeight, juce::Justification::left);
    }
    else
    {
        g.setColour(lnf->c(UltimateLNF::text));
        g.setFont(juce::FontOptions(22.0f).withStyle("bold"));
        // CHANGED: Fallback text updated
        g.drawText("NS - bussStuff", 20, 10, 200, 30, juce::Justification::left);
    }
}

void UltimateCompAudioProcessorEditor::paintOverChildren(juce::Graphics& g)
{
    // FIXED: Don't draw meters/overlay if preset panel is visible (avoids bleed-through)
    if (presetPanel && presetPanel->isVisible())
        return;

    auto drawHugeMeter = [&](juce::Rectangle<int> r, float L, float R, juce::String label) {
        g.setColour(lnf->c(UltimateLNF::panel2).withAlpha(0.9f));
        g.fillRoundedRectangle(r.toFloat(), 6.0f);
        g.setColour(lnf->c(UltimateLNF::edge));
        g.drawRoundedRectangle(r.toFloat(), 6.0f, 1.5f);
        int w = r.getWidth() - 35; int h = r.getHeight() / 2 - 2;
        int barX = r.getX() + 30; int barY = r.getY() + 2;
        g.setColour(lnf->c(UltimateLNF::ok));
        g.fillRect(barX, barY, (int)(w * juce::jlimit(0.0f, 1.0f, L * 1.5f)), h);
        g.fillRect(barX, barY + h + 2, (int)(w * juce::jlimit(0.0f, 1.0f, R * 1.5f)), h);
        g.setColour(lnf->c(UltimateLNF::text));
        g.setFont(juce::FontOptions(11.0f).withStyle("bold"));
        g.drawText(label, r.getX(), r.getY(), 30, r.getHeight(), juce::Justification::centred);
        };
    drawHugeMeter(inMeterArea, smoothInL, smoothInR, "IN");
    drawHugeMeter(outMeterArea, smoothOutL, smoothOutR, "OUT");

    if (!grBarArea.isEmpty()) {
        g.setColour(lnf->c(UltimateLNF::panel2));
        g.fillRoundedRectangle(grBarArea.toFloat(), 6.0f);
        float grInv = std::abs(smoothGR);
        float w = (float)grBarArea.getWidth() * (juce::jlimit(0.0f, 24.0f, grInv) / 24.0f);
        if (w > 1.0f) {
            juce::Graphics::ScopedSaveState ss(g);
            g.reduceClipRegion((int)(grBarArea.getRight() - w), grBarArea.getY(), (int)w, grBarArea.getHeight());
            g.setColour(lnf->c(UltimateLNF::warn));
            g.fillRect(grBarArea);
        }
        g.setColour(lnf->c(UltimateLNF::white));
        g.setFont(juce::FontOptions(16.0f).withStyle("bold"));
        g.drawText(juce::String(smoothGR, 1) + " dB", grBarArea, juce::Justification::centred);
        g.setColour(lnf->c(UltimateLNF::text2));
        g.setFont(juce::FontOptions(12.0f));
        g.drawText("GR", grBarArea.getX() - 30, grBarArea.getY(), 25, grBarArea.getHeight(), juce::Justification::right);
    }

    auto r = fluxDotArea.toFloat();
    g.setColour(lnf->c(UltimateLNF::panel2));
    g.fillEllipse(r);
    if (smoothFlux > 0.01f) {
        g.setColour(lnf->c(UltimateLNF::warn).withAlpha(juce::jlimit(0.2f, 1.0f, smoothFlux)));
        g.fillEllipse(r.reduced(2));
    }

    if (!crestDotArea.isEmpty()) {
        g.setColour(lnf->c(UltimateLNF::panel2));
        g.fillEllipse(crestDotArea.toFloat());
        if (smoothCrest > 0.001f) {
            g.setColour(juce::Colours::red.withAlpha(juce::jlimit(0.2f, 1.0f, smoothCrest * 5.0f)));
            g.fillEllipse(crestDotArea.toFloat().reduced(1.0f));
            g.setColour(juce::Colours::red.withAlpha(0.4f));
            g.drawEllipse(crestDotArea.toFloat(), 2.0f);
        }
    }

    auto drawPower = [&](juce::ToggleButton& b) {
        auto bR = b.getBounds().toFloat();
        bool on = b.getToggleState();
        g.setColour(on ? lnf->c(UltimateLNF::ok) : lnf->c(UltimateLNF::text2).withAlpha(0.3f));
        float cx = bR.getCentreX(); float cy = bR.getCentreY(); float rad = 5.0f;
        juce::Path path; path.addArc(cx - rad, cy - rad, rad * 2, rad * 2, 0.5f, 5.8f, true);
        g.strokePath(path, juce::PathStrokeType(1.5f));
        g.drawLine(cx, cy - rad, cx, cy - rad + 4.0f, 1.5f);
        if (on) { g.setColour(lnf->c(UltimateLNF::ok).withAlpha(0.4f)); g.fillEllipse(cx - rad, cy - rad, rad * 2, rad * 2); }
        };
    drawPower(bActiveDyn); drawPower(bActiveDet); drawPower(bActiveCrest);
    drawPower(bActiveTpFlux); drawPower(bActiveSat); drawPower(bActiveEq);

    auto drawLabel = [&](juce::Component& c, juce::String text) {
        if (auto* p = c.getParentComponent()) {
            if (!c.isVisible()) return;
            auto r = c.getBoundsInParent();
            auto rEditor = r + p->getPosition();

            g.setColour(lnf->c(UltimateLNF::text2));
            g.setFont(juce::FontOptions(10.0f).withStyle("bold"));
            juce::Rectangle<int> labelRect(rEditor.getX(), rEditor.getY() - 12, rEditor.getWidth(), 12);
            g.drawText(text, labelRect, juce::Justification::centred);
        }
        };
    drawLabel(cAutoRel, "RELEASE"); drawLabel(cThrust, "THRUST"); drawLabel(cCtrlMode, "CONTROL");
    drawLabel(cFluxMode, "FLUX"); drawLabel(cTpMode, "Priority"); drawLabel(cSatMode, "TRANSFORMER");
    // CHANGED: Label text updated
    drawLabel(cSatAutoGain, "AGC"); drawLabel(cSignalFlow, "FLOW");
    drawLabel(cCompAutoGain, "AGC");
    drawLabel(cScMode, "INPUT");
    drawLabel(cMsMode, "M/S ROUTING");

    // --- DRAW CYAN CONNECTION LINES FOR MIRROR ---
    if (bCompMirror.getToggleState())
    {
        // Coordinates relative to Editor
        auto btnC = bCompMirror.getBounds().getCentre() + panelDyn->getPosition();
        auto inC = kCompInput->getBounds().getCentre() + panelDyn->getPosition();
        auto outC = kMakeup->getBounds().getCentre() + panelDyn->getPosition();

        g.setColour(lnf->c(UltimateLNF::ok)); // Cyan

        juce::Path p;
        p.startNewSubPath(btnC.toFloat());
        p.lineTo(inC.toFloat());
        p.startNewSubPath(btnC.toFloat());
        p.lineTo(outC.toFloat());

        g.strokePath(p, juce::PathStrokeType(2.0f));
    }


// --- DRAW CYAN "SISTER" CONNECTION LINES (always-on) ---
// NOTE: Drawn in paintOverChildren so they are guaranteed visible above the UI.
auto centerInEditor = [this](juce::Component* c) -> juce::Point<float>
{
    if (c == nullptr) return {};
    // Convert the component's local centre to this editor's coordinate space (works even if nested in panels).
    const auto localCentre = c->getLocalBounds().getCentre();
    const auto global      = c->localPointToGlobal(localCentre);
    const auto editorLocal = this->getLocalPoint(nullptr, global);
    return editorLocal.toFloat();
};

auto approxRadius = [](juce::Component* c) -> float
{
    if (c == nullptr) return 0.0f;
    const auto lb = c->getLocalBounds().toFloat();
    return 0.5f * std::min(lb.getWidth(), lb.getHeight());
};

auto drawSister = [&](juce::Component* a, juce::Component* b)
{
    if (a == nullptr || b == nullptr) return;
    if (!a->isShowing() || !b->isShowing()) return;

    auto pa = centerInEditor(a);
    auto pb = centerInEditor(b);

    auto ra = approxRadius(a);
    auto rb = approxRadius(b);

    auto v = pb - pa;
    const float len = v.getDistanceFromOrigin();
    if (len < 1.0f) return;

    const auto dir = v / len;

    // Start/end slightly outside the controls so the line is not hidden under knob faces.
    pa += dir * (ra * 0.75f);
    pb -= dir * (rb * 0.75f);

    juce::Path path;
    path.startNewSubPath(pa);
    path.lineTo(pb);

    // Subtle glow + crisp core so it is unmissable.
    g.setColour(juce::Colours::cyan.withAlpha(0.25f));
    g.strokePath(path, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour(juce::Colours::cyan.withAlpha(0.90f));
    g.strokePath(path, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // End-caps
    g.setColour(juce::Colours::cyan.withAlpha(0.95f));
    g.fillEllipse(pa.x - 2.5f, pa.y - 2.5f, 5.0f, 5.0f);
    g.fillEllipse(pb.x - 2.5f, pb.y - 2.5f, 5.0f, 5.0f);
};

// Amount <-> Frequency pairs (your primary "sister" pattern)
drawSister(kGirth.get(),  kGirthFreq.get());
drawSister(kTone.get(),   kToneFreq.get());
drawSister(kBright.get(), kBrightFreq.get());

// Other obvious "paired" controls
}

void UltimateCompAudioProcessorEditor::resized()
{
    constexpr float baseW = 1100.0f; constexpr float baseH = 680.0f;
    const float s = juce::jlimit(0.75f, 2.0f, juce::jmin(getWidth() / baseW, getHeight() / baseH));
    auto si = [s](float v) -> int { return juce::jmax(1, (int)std::lround(v * s)); };
    const int outerPad = si(15.0f);
    auto r = getLocalBounds().reduced(outerPad);

    const int headerH = si(26.0f);
    panelDyn->setHeaderHeight(headerH); panelDet->setHeaderHeight(headerH);
    panelCrest->setHeaderHeight(headerH); panelTpFlux->setHeaderHeight(headerH);
    panelSat->setHeaderHeight(headerH); panelEq->setHeaderHeight(headerH);

    // ADDED: Resize the preset panel to cover the main area
    // Just slightly smaller than the window, centered
    if (presetPanel)
        presetPanel->setBounds(getLocalBounds().reduced(si(50)));

    auto topBar = r.removeFromTop(si(60.0f));

    const int helpS = si(24.0f);
    bHelp.setBounds(topBar.getRight() - helpS, topBar.getY() + si(8.0f), helpS, helpS);

    // ADDED: Resize Preset Button
    // To the left of the Help button
    const int presetBtnW = si(80.0f);
    bPresets.setBounds(bHelp.getX() - presetBtnW - si(10), bHelp.getY(), presetBtnW, helpS);

    const int meterH = si(24.0f); const int meterGap = si(6.0f);
    const int metersTotalWidth = juce::jmin(topBar.getWidth(), si(300.0f));
    auto centerMeters = topBar.withWidth(metersTotalWidth).withX((getWidth() - metersTotalWidth) / 2);
    inMeterArea = centerMeters.removeFromTop(meterH); centerMeters.removeFromTop(meterGap); outMeterArea = centerMeters.removeFromTop(meterH);

    const int rowH = r.getHeight() / 3;
    auto row1 = r.removeFromTop(rowH); auto row2 = r.removeFromTop(rowH); auto row3 = r;

    // CHANGED: Increased combo width to ensure text fits
    const int comboH = si(20.0f);
    const int comboW = si(110.0f); // Was 90

    const int panelPadX = si(2.0f); const int panelPadY = si(5.0f);
    const int fixedKnobW = si(64.0f); const int fixedKnobH = si(74.0f);

    auto placeKnob = [&](Knob* k, juce::Rectangle<int> slot) { if (k == nullptr) return; k->setBounds(slot.withSizeKeepingCentre(fixedKnobW, fixedKnobH)); };

    auto placePowerBtn = [&](juce::ToggleButton& b, juce::Component* panel) {
        if (panel == nullptr) return;
        auto pr = panel->getBounds();
        int btnS = si(18.0f); int mR = si(10.0f); int mY = (headerH - btnS) / 2;
        b.setBounds(pr.getRight() - btnS - mR, pr.getY() + mY, btnS, btnS);
        };

    panelDyn->setBounds(row1.reduced(0, panelPadY));
    placePowerBtn(bActiveDyn, panelDyn.get());
    {
        auto cLocal = panelDyn->getContentBounds();
        auto bot = cLocal.removeFromBottom(si(44.0f));
        const int barW = juce::jmin(bot.getWidth(), si(300.0f));
        const int barH = juce::jmin(bot.getHeight(), si(24.0f));
        auto bar = bot.withWidth(barW).withHeight(barH);
        bar.setX(bot.getCentreX() - barW / 2); bar.setY(bot.getY() + (bot.getHeight() - barH) / 2);
        grBarArea = bar.translated(panelDyn->getX(), panelDyn->getY());

        auto botRight = bot.removeFromRight(comboW + si(10.0f));
        cAutoRel.setBounds(botRight.withSizeKeepingCentre(comboW, comboH));

        auto botLeft = bot.removeFromLeft(comboW + si(10.0f));
        cCompAutoGain.setBounds(botLeft.withSizeKeepingCentre(comboW, comboH));

        auto topStrip = cLocal.removeFromTop(si(24.0f));
        const int colW = cLocal.getWidth() / 8;
        placeKnob(kThresh.get(), cLocal.removeFromLeft(colW)); placeKnob(kRatio.get(), cLocal.removeFromLeft(colW));
        placeKnob(kKnee.get(), cLocal.removeFromLeft(colW));
        auto rAttackSlot = cLocal.removeFromLeft(colW); placeKnob(kAttack.get(), rAttackSlot);
        int btnW = si(120.0f); int btnH = si(20.0f);
        bTurboAtt.setBounds(rAttackSlot.getX() + (rAttackSlot.getWidth() - btnW) / 2, kAttack->getY() - btnH - si(4), btnW, btnH);
        auto rReleaseSlot = cLocal.removeFromLeft(colW); placeKnob(kRelease.get(), rReleaseSlot);
        bTurboRel.setBounds(rReleaseSlot.getX() + (rReleaseSlot.getWidth() - btnW) / 2, kRelease->getY() - btnH - si(4), btnW, btnH);

        auto rCompInputSlot = cLocal.removeFromLeft(colW); placeKnob(kCompInput.get(), rCompInputSlot);
        auto rMake = cLocal.removeFromLeft(colW); placeKnob(kMakeup.get(), rMake);

        // CHANGED: Mirror button placement
        // "in between input and output" horizontally.
        // We place it centered on the boundary line between the two slots (rMake.getX()).
        int autoW = si(40.0f);
        bCompMirror.setBounds(rMake.getX() - autoW / 2, kMakeup->getY() - btnH - si(4), autoW, btnH);

        placeKnob(kMix.get(), cLocal.removeFromLeft(colW));
    }

    {
        const int detW = (int)std::lround(row2.getWidth() * 0.6f);
        panelDet->setBounds(row2.removeFromLeft(detW).reduced(panelPadX, panelPadY));
        placePowerBtn(bActiveDet, panelDet.get());
        panelCrest->setBounds(row2.reduced(panelPadX, panelPadY));
        placePowerBtn(bActiveCrest, panelCrest.get());
    }

    {
        auto c = panelDet->getContentBounds().reduced(si(2.0f));
        auto bot = c.removeFromBottom(si(44.0f));
        cThrust.setBounds(bot.removeFromRight(comboW).withSizeKeepingCentre(comboW, comboH));

        // CHANGED: Widths for Sidechain bottom row controls
        int smallComboW = si(90.0f); // Was 70
        const int badgeW = si(70.0f); // Was 38, now fits "SC->Comp"
        const int badgeH = comboH;
        const int gap = si(6.0f);

        const int leftNeeded = smallComboW + gap + badgeW + gap + smallComboW + si(12.0f);
        auto botLeft = bot.removeFromLeft(leftNeeded);

        cScMode.setBounds(botLeft.removeFromLeft(smallComboW).withSizeKeepingCentre(smallComboW, comboH));
        botLeft.removeFromLeft(gap);
        bScToComp.setBounds(botLeft.removeFromLeft(badgeW).withSizeKeepingCentre(badgeW, badgeH));
        botLeft.removeFromLeft(gap);
        cMsMode.setBounds(botLeft.removeFromLeft(smallComboW).withSizeKeepingCentre(smallComboW, comboH));

        const int w = c.getWidth() / 9;
        placeKnob(kScHpf.get(), c.removeFromLeft(w));
        placeKnob(kScLpf.get(), c.removeFromLeft(w));
        placeKnob(kDetRms.get(), c.removeFromLeft(w));
        placeKnob(kStereoLink.get(), c.removeFromLeft(w));
        placeKnob(kMsBalance.get(), c.removeFromLeft(w));
        placeKnob(kFbBlend.get(), c.removeFromLeft(w));
        placeKnob(kScLevel.get(), c.removeFromLeft(w));
        placeKnob(kScTdAmt.get(), c.removeFromLeft(w));
        placeKnob(kScTdMs.get(), c);
    }

    {
        auto c = panelCrest->getContentBounds().reduced(si(2.0f));
        auto bot = c.removeFromBottom(si(44.0f));
        cCtrlMode.setBounds(bot.removeFromRight(comboW).withSizeKeepingCentre(comboW, comboH));
        const int w = c.getWidth() / 2;
        placeKnob(kCrestTarget.get(), c.removeFromLeft(w)); placeKnob(kCrestSpeed.get(), c);
        auto pr = panelCrest->getBounds(); int dotS = si(8.0f);
        crestDotArea = juce::Rectangle<int>(pr.getX() + si(10.0f), pr.getY() + headerH + si(10.0f), dotS, dotS);
    }

    const int w3 = row3.getWidth() / 3;
    panelTpFlux->setBounds(row3.removeFromLeft(w3).reduced(panelPadX, panelPadY));
    placePowerBtn(bActiveTpFlux, panelTpFlux.get());
    panelSat->setBounds(row3.removeFromLeft(w3).reduced(panelPadX, panelPadY));
    placePowerBtn(bActiveSat, panelSat.get());
    panelEq->setBounds(row3.reduced(panelPadX, panelPadY));
    placePowerBtn(bActiveEq, panelEq.get());

    {
        auto c = panelTpFlux->getContentBounds().reduced(si(2.0f));
        auto bot = c.removeFromBottom(si(44.0f));
        // CHANGED: Increased slot size for Flux/TP combos
        const int miniSlot = si(90.0f); const int miniW = si(85.0f);
        cFluxMode.setBounds(bot.removeFromRight(miniSlot).withSizeKeepingCentre(miniW, comboH));
        cTpMode.setBounds(bot.removeFromRight(miniSlot).withSizeKeepingCentre(miniW, comboH));
        const int w = c.getWidth() / 3;
        placeKnob(kTpAmt.get(), c.removeFromLeft(w)); placeKnob(kTpRaise.get(), c.removeFromLeft(w)); placeKnob(kFluxAmt.get(), c);
    }

    {
        auto c = panelSat->getContentBounds().reduced(si(4.0f));
        auto satRect = panelSat->getBounds();
        const int dotPadL = si(10.0f); const int dotY = si(8.0f); const int dotS = si(12.0f);
        fluxDotArea = juce::Rectangle<int>(satRect.getX() + dotPadL, satRect.getY() + dotY, dotS, dotS);
        auto bot = c.removeFromBottom(si(44.0f));

        // CHANGED: Centered Combos
        const int miniSlot = si(90.0f); const int miniW = si(85.0f);
        int totalComboW = 3 * miniSlot;
        int startX = bot.getX() + (bot.getWidth() - totalComboW) / 2;

        cSatMode.setBounds(startX, bot.getY() + (bot.getHeight() - comboH) / 2, miniW, comboH);
        startX += miniSlot;
        cSatAutoGain.setBounds(startX, bot.getY() + (bot.getHeight() - comboH) / 2, miniW, comboH);
        startX += miniSlot;
        cSignalFlow.setBounds(startX, bot.getY() + (bot.getHeight() - comboH) / 2, miniW, comboH);

        const int w = c.getWidth() / 4;
        // Place knobs
        auto rPre = c.removeFromLeft(w);
        placeKnob(kSatPre.get(), rPre);

        // CHANGED: Place Mirror button above Pre-Gain Knob
        // UPDATED: moved Y position to -si(4) to clear the "Pre-Gain" label
        int btnW = si(40.0f); int btnH = si(18.0f);
        bMirror.setBounds(rPre.getCentreX() - btnW / 2, rPre.getY() - si(6), btnW, btnH);

        placeKnob(kSatDrive.get(), c.removeFromLeft(w));
        placeKnob(kSatTrim.get(), c.removeFromLeft(w)); placeKnob(kSatMix.get(), c);
    }
    {
        auto c = panelEq->getContentBounds().reduced(si(4.0f));
        const int w = c.getWidth() / 6;
        placeKnob(kGirth.get(), c.removeFromLeft(w)); placeKnob(kGirthFreq.get(), c.removeFromLeft(w));
        placeKnob(kTone.get(), c.removeFromLeft(w)); placeKnob(kToneFreq.get(), c.removeFromLeft(w));
        placeKnob(kBright.get(), c.removeFromLeft(w)); placeKnob(kBrightFreq.get(), c);
    }
}