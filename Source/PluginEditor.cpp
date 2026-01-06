/*
  ==============================================================================
    PluginEditor.cpp
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

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

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override
    {
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
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool, bool) override
    {
        if (b.getButtonText().isEmpty()) return;
        auto r = b.getLocalBounds().toFloat().reduced(2.0f);

        // ALLOW LONGER TEXT FOR BADGES like "Mirror" or "SC->Comp"
        bool isSmall = b.getButtonText().length() <= 10;

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

        valueLabel.onTextChange = [this] { float val = valueLabel.getText().getFloatValue(); slider.setValue(val, juce::sendNotification); };
        addAndMakeVisible(valueLabel);
        slider.onValueChange = [this] { updateLabelText(); if (onValChange) onValChange(); };
    }
    ~Knob() override { slider.setLookAndFeel(nullptr); valueLabel.setLookAndFeel(nullptr); }
    juce::Slider& getSlider() noexcept { return slider; }
    void setUnitSuffix(juce::String s) { suffix = std::move(s); }
    void setTextFromValue(std::function<juce::String(double)> fn) { textFromValue = std::move(fn); }
    std::function<void()> onValChange;
    void updateLabelText() {
        double v = slider.getValue();
        juce::String s = textFromValue ? textFromValue(v) : slider.getTextFromValue(v);
        if (suffix.isNotEmpty() && !s.contains(suffix)) s << suffix;
        valueLabel.setText(s, juce::dontSendNotification);
    }
    void paint(juce::Graphics& g) override {
        auto b = getLocalBounds();
        const int textH = juce::jlimit(12, 20, (int)std::lround(getHeight() * 0.18f));
        auto top = b.removeFromTop(textH);
        const float fontPx = juce::jlimit(10.0f, 15.0f, (float)textH * 0.80f);
        g.setColour(lnf.c(UltimateLNF::text2));
        g.setFont(juce::FontOptions(fontPx));
        g.drawFittedText(labelTitle, top, juce::Justification::centred, 1);
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

    // LOAD LOGO: Ensure 'logo.png' is added to Projucer BinaryData!
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

    // Compressor Input & Makeup
    kCompInput = makeKnob("Input", *lnf); kCompInput->setUnitSuffix("dB");

    // PHYSICAL MIRROR LOGIC FOR INPUT KNOB
    kCompInput->onValChange = [this] {
        kCompInput->updateLabelText();

        // This physically moves the output knob if mirror is ON
        float currentVal = (float)kCompInput->getSlider().getValue();
        if (bCompMirror.getToggleState() && !ignoreCallbacks) {
            ignoreCallbacks = true; // prevent recursion loop
            float delta = currentVal - lastCompInputVal;
            // Move Output inversely
            double newMakeup = kMakeup->getSlider().getValue() - delta;
            kMakeup->getSlider().setValue(newMakeup, juce::sendNotificationSync);
            ignoreCallbacks = false;
        }
        lastCompInputVal = currentVal;
        };

    kMakeup = makeKnob("Output", *lnf); kMakeup->setUnitSuffix("dB");
    kMix = makeKnob("Mix", *lnf); kMix->setUnitSuffix("%");

    // Sidechain Knobs
    kScHpf = makeKnob("Low Cut", *lnf); kScHpf->setUnitSuffix("Hz");
    kScLpf = makeKnob("High Cut", *lnf); kScLpf->setUnitSuffix("Hz");
    kDetRms = makeKnob("RMS Window", *lnf); kDetRms->setUnitSuffix("ms");
    kStereoLink = makeKnob("Link", *lnf); kStereoLink->setUnitSuffix("%");
    kMsBalance = makeKnob("M/S Bal", *lnf); kMsBalance->setUnitSuffix("dB"); // NEW
    kFbBlend = makeKnob("FB Blend", *lnf); kFbBlend->setUnitSuffix("%");
    kScLevel = makeKnob("SC Level", *lnf); kScLevel->setUnitSuffix("dB");

    kCrestTarget = makeKnob("Crest Target", *lnf); kCrestTarget->setUnitSuffix("dB");
    kCrestSpeed = makeKnob("Crest Speed", *lnf); kCrestSpeed->setUnitSuffix("ms");
    kTpAmt = makeKnob("Focus", *lnf); kTpAmt->setUnitSuffix("%");
    kTpRaise = makeKnob("Punch", *lnf); kTpRaise->setUnitSuffix("dB");
    kFluxAmt = makeKnob("Flux Amt", *lnf); kFluxAmt->setUnitSuffix("%");
    kSatPre = makeKnob("Pre-Gain", *lnf); kSatPre->setUnitSuffix("dB");
    kSatDrive = makeKnob("Drive", *lnf); kSatDrive->setUnitSuffix("dB");
    kSatTrim = makeKnob("Trim", *lnf); kSatTrim->setUnitSuffix("dB");
    kSatMix = makeKnob("Mix", *lnf); kSatMix->setUnitSuffix("%");
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

    // NEW: Compressor Auto-Gain Control
    prepCombo(cCompAutoGain); cCompAutoGain.addItem("AGC Off", 1); cCompAutoGain.addItem("Partial", 2); cCompAutoGain.addItem("Full", 3);

    prepCombo(cThrust); cThrust.addItem("Normal", 1); cThrust.addItem("Med", 2); cThrust.addItem("Loud", 3);
    prepCombo(cCtrlMode); cCtrlMode.addItem("Manual", 1); cCtrlMode.addItem("Auto", 2);
    prepCombo(cTpMode); cTpMode.addItem("Off", 1); cTpMode.addItem("On", 2);
    prepCombo(cFluxMode); cFluxMode.addItem("Off", 1); cFluxMode.addItem("On", 2);
    prepCombo(cSatMode); cSatMode.addItem("Clean", 1); cSatMode.addItem("Iron", 2); cSatMode.addItem("Steel", 3);
    prepCombo(cSignalFlow); cSignalFlow.addItem("Comp>Sat", 1); cSignalFlow.addItem("Sat>Comp", 2);
    prepCombo(cSatAutoGain); cSatAutoGain.addItem("Off", 1); cSatAutoGain.addItem("Partial", 2); cSatAutoGain.addItem("Full", 3);

    // SIDECHAIN INTEGRATED COMBOS
    prepCombo(cScMode); cScMode.addItem("In", 1); cScMode.addItem("Ext", 2);
    prepCombo(cMsMode); cMsMode.addItem("Link", 1); cMsMode.addItem("Mid", 2); cMsMode.addItem("Side", 3);
    cMsMode.addItem("M>S", 4); cMsMode.addItem("S>M", 5);

    bTurboAtt.setButtonText("F"); bTurboAtt.setClickingTogglesState(true); bTurboAtt.onClick = [this] { kAttack->updateLabelText(); };
    bTurboRel.setButtonText("F"); bTurboRel.setClickingTogglesState(true); bTurboRel.onClick = [this] { kRelease->updateLabelText(); };

    bMirror.setButtonText("Mirror"); bMirror.setClickingTogglesState(true);
    bCompMirror.setButtonText("Mirror"); bCompMirror.setClickingTogglesState(true);

    // Routing Toggles
    bScToComp.setButtonText("SC->Comp"); bScToComp.setClickingTogglesState(true);
    bScToSat.setButtonText("SC->Sat"); bScToSat.setClickingTogglesState(true);
    bScAudition.setButtonText("Audition"); bScAudition.setClickingTogglesState(true);

    // --- 3. Panels ---
    panelDyn = std::make_unique<Panel>("Main Dynamics", *lnf);
    panelDet = std::make_unique<Panel>("Sidechain", *lnf);
    panelCrest = std::make_unique<Panel>("Crest", *lnf);
    panelTpFlux = std::make_unique<Panel>("Transient/Flux", *lnf);
    panelSat = std::make_unique<Panel>("Saturation", *lnf);
    panelEq = std::make_unique<Panel>("Color EQ", *lnf);

    addAndMakeVisible(*panelDyn);
    addAndMakeVisible(*panelDet);
    addAndMakeVisible(*panelCrest);
    addAndMakeVisible(*panelTpFlux);
    addAndMakeVisible(*panelSat);
    addAndMakeVisible(*panelEq);

    // --- 4. Bypass Buttons ---
    auto setupPowerBtn = [&](juce::ToggleButton& b, juce::String paramId, std::unique_ptr<ButtonAttachment>& att) {
        b.setButtonText(""); b.setClickingTogglesState(true);
        att = std::make_unique<ButtonAttachment>(audioProcessor.apvts, paramId, b);
        addAndMakeVisible(b);
        };
    setupPowerBtn(bActiveDyn, "active_dyn", aActiveDyn);
    setupPowerBtn(bActiveDet, "active_det", aActiveDet);
    setupPowerBtn(bActiveCrest, "active_crest", aActiveCrest);
    setupPowerBtn(bActiveTpFlux, "active_tf", aActiveTpFlux);
    setupPowerBtn(bActiveSat, "active_sat", aActiveSat);
    setupPowerBtn(bActiveEq, "active_eq", aActiveEq);

    // --- 5. Add Children ---
    panelDyn->addAndMakeVisible(*kThresh); panelDyn->addAndMakeVisible(*kRatio); panelDyn->addAndMakeVisible(*kKnee);
    panelDyn->addAndMakeVisible(*kAttack); panelDyn->addAndMakeVisible(bTurboAtt);
    panelDyn->addAndMakeVisible(*kRelease); panelDyn->addAndMakeVisible(bTurboRel);

    // Compressor Input/Output/Mix
    panelDyn->addAndMakeVisible(*kCompInput); panelDyn->addAndMakeVisible(*kMakeup);
    panelDyn->addAndMakeVisible(bCompMirror);
    panelDyn->addAndMakeVisible(cCompAutoGain); // Added to panel

    panelDyn->addAndMakeVisible(*kMix); panelDyn->addAndMakeVisible(cAutoRel);

    // Sidechain Panel Children
    panelDet->addAndMakeVisible(*kScHpf);
    panelDet->addAndMakeVisible(*kScLpf);
    panelDet->addAndMakeVisible(*kDetRms);
    panelDet->addAndMakeVisible(*kStereoLink); panelDet->addAndMakeVisible(*kMsBalance); panelDet->addAndMakeVisible(*kFbBlend); panelDet->addAndMakeVisible(*kScLevel); // NEW
    panelDet->addAndMakeVisible(cThrust);
    panelDet->addAndMakeVisible(cScMode);
    panelDet->addAndMakeVisible(cMsMode);
    // Add Routing Toggles to Det Panel
    panelDet->addAndMakeVisible(bScToComp);
    panelDet->addAndMakeVisible(bScToSat);
    panelDet->addAndMakeVisible(bScAudition);

    panelCrest->addAndMakeVisible(*kCrestTarget); panelCrest->addAndMakeVisible(*kCrestSpeed); panelCrest->addAndMakeVisible(cCtrlMode);
    panelTpFlux->addAndMakeVisible(*kTpAmt); panelTpFlux->addAndMakeVisible(*kTpRaise); panelTpFlux->addAndMakeVisible(*kFluxAmt);
    panelTpFlux->addAndMakeVisible(cTpMode); panelTpFlux->addAndMakeVisible(cFluxMode);

    panelSat->addAndMakeVisible(*kSatPre); panelSat->addAndMakeVisible(*kSatDrive); panelSat->addAndMakeVisible(*kSatTrim);
    panelSat->addAndMakeVisible(*kSatMix); panelSat->addAndMakeVisible(cSatMode); panelSat->addAndMakeVisible(cSatAutoGain);
    panelSat->addAndMakeVisible(cSignalFlow); panelSat->addAndMakeVisible(bMirror);

    panelEq->addAndMakeVisible(*kTone); panelEq->addAndMakeVisible(*kToneFreq);
    panelEq->addAndMakeVisible(*kBright); panelEq->addAndMakeVisible(*kBrightFreq);

    // --- 6. Bindings ---
    bindKnob(*kThresh, aThresh, "thresh", "dB"); bindKnob(*kRatio, aRatio, "ratio", ""); bindKnob(*kKnee, aKnee, "knee", "dB");
    bindKnob(*kAttack, aAttack, "att_ms", "ms"); bindKnob(*kRelease, aRelease, "rel_ms", "ms");
    bindKnob(*kCompInput, aCompInput, "comp_input", "dB");

    // Store initial value for tracking
    if (auto* p = audioProcessor.apvts.getParameter("comp_input"))
        lastCompInputVal = p->convertFrom0to1(p->getValue());

    bindKnob(*kMakeup, aMakeup, "makeup", "dB"); bindKnob(*kMix, aMix, "dry_wet", "%");

    bindKnob(*kScHpf, aScHpf, "sc_hp_freq", "Hz");
    bindKnob(*kScLpf, aScLpf, "sc_lp_freq", "Hz");
    bindKnob(*kDetRms, aDetRms, "det_rms", "ms");
    bindKnob(*kStereoLink, aStereoLink, "stereo_link", "%");
    bindKnob(*kMsBalance, aMsBalance, "ms_balance", "dB"); // NEW
    bindKnob(*kFbBlend, aFbBlend, "fb_blend", "%");
    bindKnob(*kScLevel, aScLevel, "sc_level_db", "dB");

    bindKnob(*kCrestTarget, aCrestTarget, "crest_target", "dB"); bindKnob(*kCrestSpeed, aCrestSpeed, "crest_speed", "ms");
    bindKnob(*kTpAmt, aTpAmt, "tp_amount", "%"); bindKnob(*kTpRaise, aTpRaise, "tp_thresh_raise", "dB");
    bindKnob(*kFluxAmt, aFluxAmt, "flux_amount", "%");
    bindKnob(*kSatPre, aSatPre, "sat_pre_gain", "dB"); bindKnob(*kSatDrive, aSatDrive, "sat_drive", "dB");
    bindKnob(*kSatTrim, aSatTrim, "sat_trim", "dB"); bindKnob(*kSatMix, aSatMix, "sat_mix", "%");
    bindKnob(*kTone, aTone, "sat_tone", "dB"); bindKnob(*kToneFreq, aToneFreq, "sat_tone_freq", "Hz");
    bindKnob(*kBright, aBright, "harm_bright", "dB"); bindKnob(*kBrightFreq, aBrightFreq, "harm_freq", "Hz");

    initCombo(cAutoRel, aAutoRel, "auto_rel");
    initCombo(cCompAutoGain, aCompAutoGain, "comp_autogain"); // NEW
    initCombo(cThrust, aThrust, "thrust_mode");
    initCombo(cCtrlMode, aCtrlMode, "ctrl_mode");
    initCombo(cTpMode, aTpMode, "tp_mode");
    initCombo(cFluxMode, aFluxMode, "flux_mode");
    initCombo(cSatMode, aSatMode, "sat_mode");
    initCombo(cSatAutoGain, aSatAutoGain, "sat_autogain");
    initCombo(cSignalFlow, aSignalFlow, "signal_flow");

    initCombo(cScMode, cScModeAtt, "sc_mode");
    initCombo(cMsMode, aMsMode, "ms_mode");

    aTurboAtt = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "turbo_att", bTurboAtt);
    aTurboRel = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "turbo_rel", bTurboRel);
    aMirror = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "sat_mirror", bMirror);

    aCompMirror = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "comp_mirror", bCompMirror);
    aScToComp = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "sc_to_comp", bScToComp);
    aScToSat = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "sc_to_sat", bScToSat);
    aScAudition = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "sc_audition", bScAudition);

    setResizable(true, true);
    setResizeLimits(1000, 600, 2000, 1200);
    setSize(1100, 680);
    startTimerHz(60);
}

UltimateCompAudioProcessorEditor::~UltimateCompAudioProcessorEditor() {
    stopTimer();
    setLookAndFeel(nullptr);
}

void UltimateCompAudioProcessorEditor::bindKnob(Knob& knob, std::unique_ptr<SliderAttachment>& attachment, const juce::String& paramID, const juce::String& suffix)
{
    knob.getSlider().setName(paramID);
    attachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, paramID, knob.getSlider());
    knob.setUnitSuffix(suffix);
    knob.updateLabelText();
}

void UltimateCompAudioProcessorEditor::initCombo(juce::ComboBox& box, std::unique_ptr<ComboBoxAttachment>& attachment, const juce::String& paramID)
{
    attachment = std::make_unique<ComboBoxAttachment>(audioProcessor.apvts, paramID, box);
}

void UltimateCompAudioProcessorEditor::timerCallback()
{
    const float decay = 0.85f;
    if (audioProcessor.meterInL > smoothInL) smoothInL = audioProcessor.meterInL; else smoothInL *= decay;
    if (audioProcessor.meterInR > smoothInR) smoothInR = audioProcessor.meterInR; else smoothInR *= decay;
    if (audioProcessor.meterOutL > smoothOutL) smoothOutL = audioProcessor.meterOutL; else smoothOutL *= decay;
    if (audioProcessor.meterOutR > smoothOutR) smoothOutR = audioProcessor.meterOutR; else smoothOutR *= decay;
    float gr = audioProcessor.meterGR;
    smoothGR = (gr < smoothGR) ? gr : (gr * 0.2f + smoothGR * 0.8f);
    float fl = audioProcessor.meterFlux;
    if (fl > smoothFlux) smoothFlux = fl; else smoothFlux *= 0.9f;
    float cr = audioProcessor.meterCrest;
    if (cr > smoothCrest) smoothCrest = cr; else smoothCrest *= 0.9f;
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
        // 1. Draw bigger, brighter logo
        g.setOpacity(1.0f);
        g.drawImageWithin(pluginLogo,
            headerX, headerY, logoSize, logoSize,
            juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize,
            false);

        // 2. Draw "mixBus" text to the right
        g.setColour(lnf->c(UltimateLNF::text));
        g.setFont(juce::FontOptions((float)textHeight).withStyle("bold"));

        int textX = headerX + logoSize + gap;
        int textY = headerY + (logoSize - textHeight) / 2;
        int textW = 200;

        g.drawText("mixBus", textX, textY, textW, textHeight, juce::Justification::left);
    }
    else
    {
        // Fallback if image load fails
        g.setColour(lnf->c(UltimateLNF::text));
        g.setFont(juce::FontOptions(22.0f).withStyle("bold"));
        g.drawText("NS - MixBus", 20, 10, 200, 30, juce::Justification::left);
    }
}

void UltimateCompAudioProcessorEditor::paintOverChildren(juce::Graphics& g)
{
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
        // FIXED: Renamed 'p' to 'path' to avoid shadowing warnings
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
    drawLabel(cSatAutoGain, "SAT-AGC"); drawLabel(cSignalFlow, "FLOW");

    // Draw Label for new Comp Auto-Gain
    drawLabel(cCompAutoGain, "AGC");

    // Labels for the integrated controls
    drawLabel(cScMode, "INPUT");
    drawLabel(cMsMode, "M/S ROUTING");
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

    auto topBar = r.removeFromTop(si(60.0f));
    const int meterH = si(24.0f); const int meterGap = si(6.0f);
    const int metersTotalWidth = juce::jmin(topBar.getWidth(), si(300.0f));
    auto centerMeters = topBar.withWidth(metersTotalWidth).withX((getWidth() - metersTotalWidth) / 2);
    inMeterArea = centerMeters.removeFromTop(meterH); centerMeters.removeFromTop(meterGap); outMeterArea = centerMeters.removeFromTop(meterH);

    const int rowH = r.getHeight() / 3;
    auto row1 = r.removeFromTop(rowH); auto row2 = r.removeFromTop(rowH); auto row3 = r;

    const int comboH = si(20.0f); const int comboW = si(90.0f);
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

        // NEW: Place Comp Auto-Gain Combo to the left of GR bar
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

        placeKnob(kCompInput.get(), cLocal.removeFromLeft(colW));
        auto rMake = cLocal.removeFromLeft(colW); placeKnob(kMakeup.get(), rMake);
        int autoW = si(40.0f);
        bCompMirror.setBounds(rMake.getX() + (rMake.getWidth() - autoW) / 2, kMakeup->getY() - btnH - si(4), autoW, btnH);

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

        int smallComboW = si(70.0f);
        const int badgeW = si(64.0f);
        const int badgeH = comboH;
        const int leftNeeded = smallComboW + (si(40.0f) * 2) + badgeW + smallComboW + si(12.0f);
        auto botLeft = bot.removeFromLeft(leftNeeded);

        cScMode.setBounds(botLeft.removeFromLeft(smallComboW).withSizeKeepingCentre(smallComboW, comboH));
        bScToComp.setBounds(botLeft.removeFromLeft(si(40.0f)).withSizeKeepingCentre(si(38.0f), badgeH));
        bScToSat.setBounds(botLeft.removeFromLeft(si(40.0f)).withSizeKeepingCentre(si(38.0f), badgeH));
        bScAudition.setBounds(botLeft.removeFromLeft(badgeW).withSizeKeepingCentre(badgeW, badgeH));
        cMsMode.setBounds(botLeft.removeFromLeft(smallComboW).withSizeKeepingCentre(smallComboW, comboH));

        const int w = c.getWidth() / 7;
        placeKnob(kScHpf.get(), c.removeFromLeft(w));
        placeKnob(kScLpf.get(), c.removeFromLeft(w));
        placeKnob(kDetRms.get(), c.removeFromLeft(w));
        placeKnob(kStereoLink.get(), c.removeFromLeft(w));
        placeKnob(kMsBalance.get(), c.removeFromLeft(w));
        placeKnob(kFbBlend.get(), c.removeFromLeft(w));
        placeKnob(kScLevel.get(), c);
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
        const int miniSlot = si(70.0f); const int miniW = si(65.0f);
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
        auto mirrorSlot = bot.removeFromRight(si(50.0f));
        bMirror.setBounds(mirrorSlot.withSizeKeepingCentre(si(40.0f), si(18.0f)));
        const int miniSlot = si(70.0f); const int miniW = si(65.0f);
        cSatMode.setBounds(bot.removeFromRight(miniSlot).withSizeKeepingCentre(miniW, comboH));
        cSatAutoGain.setBounds(bot.removeFromRight(miniSlot).withSizeKeepingCentre(miniW, comboH));
        cSignalFlow.setBounds(bot.removeFromRight(miniSlot).withSizeKeepingCentre(miniW, comboH));
        const int w = c.getWidth() / 4;
        placeKnob(kSatPre.get(), c.removeFromLeft(w)); placeKnob(kSatDrive.get(), c.removeFromLeft(w));
        placeKnob(kSatTrim.get(), c.removeFromLeft(w)); placeKnob(kSatMix.get(), c);
    }

    {
        auto c = panelEq->getContentBounds().reduced(si(4.0f));
        const int w = c.getWidth() / 4;
        placeKnob(kTone.get(), c.removeFromLeft(w)); placeKnob(kToneFreq.get(), c.removeFromLeft(w));
        placeKnob(kBright.get(), c.removeFromLeft(w)); placeKnob(kBrightFreq.get(), c);
    }
}