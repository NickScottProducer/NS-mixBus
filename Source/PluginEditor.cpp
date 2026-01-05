/*
  ==============================================================================
    PluginEditor.cpp
    Theme: "Cyber Schematic"
    Changes:
    - FIXED: Uniform Knob Sizes (64x74 scaled)
    - FIXED: Removed Hover Tooltips
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

//=============================================================================
// Utility
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
    }

    enum Palette
    {
        bgA, bgB, panel, panel2, edge, text, text2, accent, accent2, ok, warn, white, line
    };

    static juce::Colour c(Palette p)
    {
        switch (p)
        {
        case bgA:     return juce::Colour(0xff0a0910); // Deep Void
        case bgB:     return juce::Colour(0xff14121d); // Dark Purple base
        case panel:   return juce::Colour(0x00000000); // Transparent Body
        case panel2:  return juce::Colour(0xff110f18); // Darker recess
        case edge:    return juce::Colour(0xff382e4d); // Purple rim
        case text:    return juce::Colour(0xffe6e1ff); // White-purple
        case text2:   return juce::Colour(0xff9085ad); // Dim purple
        case accent:  return juce::Colour(0xffbd00ff); // NEON PURPLE
        case accent2: return juce::Colour(0xffd966ff); // Lighter Neon
        case ok:      return juce::Colour(0xff00f2ff); // Cyan (for meters)
        case warn:    return juce::Colour(0xffff0055); // Hot Pink (Warning)
        case white:   return juce::Colours::white;
        case line:    return juce::Colour(0xff2a2438); // Blueprint Grid Line
        }
        return juce::Colours::white;
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
        juce::Slider&) override
    {
        auto bounds = shrinkToSquare(juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height)).reduced(2.0f);
        const float radius = bounds.getWidth() * 0.5f;
        const auto centre = bounds.getCentre();
        const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        // Track
        const float trackWidth = 4.0f;
        juce::Path track;
        track.addCentredArc(centre.x, centre.y, radius - trackWidth, radius - trackWidth,
            0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(c(panel2).darker(0.5f));
        g.strokePath(track, juce::PathStrokeType(trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Value Arc
        juce::Path val;
        val.addCentredArc(centre.x, centre.y, radius - trackWidth, radius - trackWidth,
            0.0f, rotaryStartAngle, angle, true);
        g.setColour(c(accent).withAlpha(0.3f)); // Glow
        g.strokePath(val, juce::PathStrokeType(trackWidth + 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(c(accent)); // Core
        g.strokePath(val, juce::PathStrokeType(trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Body
        float knobR = radius - 10.0f;
        g.setColour(c(bgA).withAlpha(0.8f));
        g.fillEllipse(centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);
        g.setColour(c(edge));
        g.drawEllipse(centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f, 1.5f);

        // Dot
        float dotR = 3.0f;
        float dotDist = knobR - 5.0f;
        float dotX = centre.x + std::cos(angle - juce::MathConstants<float>::halfPi) * dotDist;
        float dotY = centre.y + std::sin(angle - juce::MathConstants<float>::halfPi) * dotDist;
        g.setColour(c(accent2));
        g.fillEllipse(dotX - dotR, dotY - dotR, dotR * 2, dotR * 2);
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool, bool) override
    {
        auto r = b.getLocalBounds().toFloat().reduced(2.0f);
        const float boxW = 32.0f;
        const float boxH = 16.0f;
        auto box = r.withWidth(boxW).withHeight(boxH).withY(r.getCentreY() - boxH / 2);

        bool on = b.getToggleState();

        g.setColour(c(panel2));
        g.fillRoundedRectangle(box, 8.0f);
        g.setColour(c(edge));
        g.drawRoundedRectangle(box, 8.0f, 1.0f);

        if (on) {
            g.setColour(c(accent).withAlpha(0.2f));
            g.fillRoundedRectangle(box, 8.0f);
            g.setColour(c(accent));
            g.drawRoundedRectangle(box, 8.0f, 1.0f);
        }

        float indSize = 10.0f;
        auto ind = box.withSizeKeepingCentre(indSize, indSize);
        if (on) ind.translate(8.0f, 0.0f); else ind.translate(-8.0f, 0.0f);

        g.setColour(on ? c(accent) : c(text2));
        g.fillEllipse(ind);

        g.setColour(c(text));
        g.setFont(juce::FontOptions(13.0f));
        g.drawFittedText(b.getButtonText(), r.withTrimmedLeft(boxW + 8.0f).toNearestInt(), juce::Justification::centredLeft, 1);
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
        p.addTriangle(arrow.getCentreX() - 3, arrow.getCentreY() - 2,
            arrow.getCentreX() + 3, arrow.getCentreY() - 2,
            arrow.getCentreX(), arrow.getCentreY() + 3);
        g.setColour(c(accent));
        g.fillPath(p);
    }
};

//=============================================================================
class UltimateCompAudioProcessorEditor::Knob final : public juce::Component
{
public:
    explicit Knob(UltimateLNF& lookAndFeel, juce::String labelText)
        : lnf(lookAndFeel), label(std::move(labelText))
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setLookAndFeel(&lnf);

        // --- 2. REMOVED POPUP TOOLTIP ---
        // slider.setPopupDisplayEnabled(true, true, this); 

        addAndMakeVisible(slider);
    }
    ~Knob() override { slider.setLookAndFeel(nullptr); }
    juce::Slider& getSlider() noexcept { return slider; }
    void setUnitSuffix(juce::String s) { suffix = std::move(s); }
    void setTextFromValue(std::function<juce::String(double)> fn) { textFromValue = std::move(fn); }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds();

        // Scale text regions with the knob size (prevents overlap when resized).
        const int textH = juce::jlimit(12, 20, (int)std::lround(getHeight() * 0.18f));
        auto top = b.removeFromTop(textH);
        auto bottom = b.removeFromBottom(textH);

        const float fontPx = juce::jlimit(10.0f, 15.0f, (float)textH * 0.80f);

        g.setColour(UltimateLNF::c(UltimateLNF::text2));
        g.setFont(juce::FontOptions(fontPx));
        g.drawFittedText(label, top, juce::Justification::centred, 1);

        g.setColour(UltimateLNF::c(UltimateLNF::accent));
        g.setFont(juce::FontOptions(fontPx));
        juce::String s = textFromValue ? textFromValue(slider.getValue())
            : slider.getTextFromValue(slider.getValue());
        if (suffix.isNotEmpty() && !s.contains(suffix))
            s << suffix;

        g.drawFittedText(s, bottom, juce::Justification::centred, 1);
    }

    void resized() override
    {
        const int textH = juce::jlimit(12, 20, (int)std::lround(getHeight() * 0.18f));

        // Slider takes full width; keep clear top/bottom bands for label + value.
        auto a = getLocalBounds().withTrimmedTop(textH).withTrimmedBottom(textH).reduced(2);
        slider.setBounds(a);
    }

    void updateValueLabel() { repaint(); }
private:
    UltimateLNF& lnf;
    juce::String label, suffix;
    juce::Slider slider;
    std::function<juce::String(double)> textFromValue;
};

//=============================================================================
class UltimateCompAudioProcessorEditor::Panel final : public juce::Component
{
public:
    Panel(juce::String titleText) : title(std::move(titleText)) {}

    void setHeaderHeight(int h)
    {
        headerH = juce::jmax(0, h);
        repaint();
    }

    int getHeaderHeight() const noexcept { return headerH; }

    juce::Rectangle<int> getContentBounds() const { return getLocalBounds().reduced(8).withTrimmedTop(headerH); }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(1.0f);

        // Transparent Body - Outline Only
        g.setColour(UltimateLNF::c(UltimateLNF::edge));
        g.drawRoundedRectangle(r, 6.0f, 1.5f);

        if (headerH > 0) {
            auto header = r.removeFromTop((float)headerH);
            g.setColour(UltimateLNF::c(UltimateLNF::bgB).withAlpha(0.6f));
            g.fillRoundedRectangle(header.getX(), header.getY(), header.getWidth(), header.getHeight(), 6.0f);

            g.setColour(UltimateLNF::c(UltimateLNF::text));
            const float headerFontPx = juce::jlimit(11.0f, 18.0f, header.getHeight() * 0.55f);
            g.setFont(juce::FontOptions(headerFontPx).withStyle("bold"));
            // Changed to Centered
            g.drawText(title, header.reduced(10, 0), juce::Justification::centred);

            g.setColour(UltimateLNF::c(UltimateLNF::edge));
            g.drawHorizontalLine((int)header.getBottom(), header.getX(), header.getRight());
        }
    }

    // We handle children placement manually in editor::resized, so Panel::resized() is empty
    void resized() override {}

private:
    juce::String title;
    int headerH = 26;
};

//=============================================================================
// EDITOR IMPLEMENTATION
//=============================================================================

UltimateCompAudioProcessorEditor::UltimateCompAudioProcessorEditor(UltimateCompAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    lnf = std::make_unique<UltimateLNF>();
    setLookAndFeel(lnf.get());

    auto makeKnob = [this](const juce::String& name) {
        auto k = std::make_unique<Knob>(*lnf, name);
        k->getSlider().onValueChange = [k = k.get()] { k->updateValueLabel(); };
        return k;
        };

    kThresh = makeKnob("Threshold"); kThresh->setUnitSuffix("dB");
    kRatio = makeKnob("Ratio"); kRatio->setTextFromValue([](double v) { return juce::String(v, 1) + ":1"; });
    kKnee = makeKnob("Knee"); kKnee->setUnitSuffix("dB");

    kAttack = makeKnob("Attack");
    kAttack->setTextFromValue([this](double v) {
        double val = bTurbo.getToggleState() ? (v * 0.1) : v;
        return juce::String(val, 2) + " ms";
        });

    kRelease = makeKnob("Release");
    kRelease->setTextFromValue([this](double v) {
        double val = bTurbo.getToggleState() ? (v * 0.1) : v;
        return juce::String(val, 2) + " ms";
        });

    kMakeup = makeKnob("Makeup"); kMakeup->setUnitSuffix("dB");
    kMix = makeKnob("Mix"); kMix->setUnitSuffix("%");

    kScHpf = makeKnob("SC HPF"); kScHpf->setUnitSuffix("Hz");
    kDetRms = makeKnob("RMS Win"); kDetRms->setUnitSuffix("ms");
    kStereoLink = makeKnob("Link"); kStereoLink->setUnitSuffix("%");
    kFbBlend = makeKnob("FB Blend"); kFbBlend->setUnitSuffix("%");

    kCrestTarget = makeKnob("Crest Tgt"); kCrestTarget->setUnitSuffix("dB");
    kCrestSpeed = makeKnob("Crest Spd"); kCrestSpeed->setUnitSuffix("ms");

    kTpAmt = makeKnob("TP Amt"); kTpAmt->setUnitSuffix("%");
    kTpRaise = makeKnob("TP Raise"); kTpRaise->setUnitSuffix("dB");
    kFluxAmt = makeKnob("Flux Amt"); kFluxAmt->setUnitSuffix("%");

    kSatDrive = makeKnob("Drive"); kSatDrive->setUnitSuffix("dB");
    kSatTrim = makeKnob("Trim"); kSatTrim->setUnitSuffix("dB");
    kSatMix = makeKnob("Mix"); kSatMix->setUnitSuffix("%");

    kTone = makeKnob("Tone"); kTone->setUnitSuffix("dB");
    kToneFreq = makeKnob("Freq"); kToneFreq->setUnitSuffix("Hz");
    kBright = makeKnob("Bright"); kBright->setUnitSuffix("dB");
    kBrightFreq = makeKnob("Freq"); kBrightFreq->setUnitSuffix("Hz");

    // Standardized ComboBox sizing logic
    auto prepCombo = [&](juce::ComboBox& b) {
        b.setJustificationType(juce::Justification::centred);
        // We set initial size here, but resized() is authoritative
        b.setSize(90, 20);
        };

    prepCombo(cAutoRel); cAutoRel.addItem("Manual", 1); cAutoRel.addItem("Auto", 2);
    prepCombo(cThrust); cThrust.addItem("Normal", 1); cThrust.addItem("Med", 2); cThrust.addItem("Loud", 3);
    prepCombo(cCtrlMode); cCtrlMode.addItem("Manual", 1); cCtrlMode.addItem("Auto", 2);
    prepCombo(cTpMode); cTpMode.addItem("Off", 1); cTpMode.addItem("On", 2);
    prepCombo(cFluxMode); cFluxMode.addItem("Off", 1); cFluxMode.addItem("On", 2);
    prepCombo(cSatMode); cSatMode.addItem("Clean", 1); cSatMode.addItem("Iron", 2); cSatMode.addItem("Steel", 3);
    prepCombo(cSignalFlow); cSignalFlow.addItem("Comp>Sat", 1); cSignalFlow.addItem("Sat>Comp", 2);

    bTurbo.setButtonText("FASTER/HARDER");
    bTurbo.setClickingTogglesState(true);
    bTurbo.onClick = [this]() {
        kAttack->updateValueLabel();
        kRelease->updateValueLabel();
        };

    panelDyn = std::make_unique<Panel>("Main Dynamics");
    panelDet = std::make_unique<Panel>("Sidechain");
    panelCrest = std::make_unique<Panel>("Crest");
    panelTpFlux = std::make_unique<Panel>("Transient/Flux");
    panelSat = std::make_unique<Panel>("Saturation");
    panelEq = std::make_unique<Panel>("Color EQ");

    addAndMakeVisible(*panelDyn);
    addAndMakeVisible(*panelDet);
    addAndMakeVisible(*panelCrest);
    addAndMakeVisible(*panelTpFlux);
    addAndMakeVisible(*panelSat);
    addAndMakeVisible(*panelEq);

    // Add controls to Panels
    panelDyn->addAndMakeVisible(*kThresh); panelDyn->addAndMakeVisible(*kRatio);
    panelDyn->addAndMakeVisible(*kKnee); panelDyn->addAndMakeVisible(*kAttack);
    panelDyn->addAndMakeVisible(*kRelease); panelDyn->addAndMakeVisible(*kMakeup);
    panelDyn->addAndMakeVisible(*kMix);
    panelDyn->addAndMakeVisible(cAutoRel);
    panelDyn->addAndMakeVisible(bTurbo);

    panelDet->addAndMakeVisible(*kScHpf); panelDet->addAndMakeVisible(*kDetRms);
    panelDet->addAndMakeVisible(*kStereoLink); panelDet->addAndMakeVisible(*kFbBlend);
    panelDet->addAndMakeVisible(cThrust);

    panelCrest->addAndMakeVisible(*kCrestTarget); panelCrest->addAndMakeVisible(*kCrestSpeed);
    panelCrest->addAndMakeVisible(cCtrlMode);

    panelTpFlux->addAndMakeVisible(*kTpAmt); panelTpFlux->addAndMakeVisible(*kTpRaise);
    panelTpFlux->addAndMakeVisible(*kFluxAmt);
    panelTpFlux->addAndMakeVisible(cTpMode);
    panelTpFlux->addAndMakeVisible(cFluxMode);

    panelSat->addAndMakeVisible(*kSatDrive); panelSat->addAndMakeVisible(*kSatTrim);
    panelSat->addAndMakeVisible(*kSatMix);
    panelSat->addAndMakeVisible(cSatMode);
    panelSat->addAndMakeVisible(cSignalFlow);

    panelEq->addAndMakeVisible(*kTone); panelEq->addAndMakeVisible(*kToneFreq);
    panelEq->addAndMakeVisible(*kBright); panelEq->addAndMakeVisible(*kBrightFreq);

    bindKnob(*kThresh, aThresh, "thresh", "dB"); bindKnob(*kRatio, aRatio, "ratio", "");
    bindKnob(*kKnee, aKnee, "knee", "dB"); bindKnob(*kAttack, aAttack, "att_ms", "ms");
    bindKnob(*kRelease, aRelease, "rel_ms", "ms"); bindKnob(*kMakeup, aMakeup, "makeup", "dB");
    bindKnob(*kMix, aMix, "dry_wet", "%");

    bindKnob(*kScHpf, aScHpf, "sc_hp_freq", "Hz"); bindKnob(*kDetRms, aDetRms, "det_rms", "ms");
    bindKnob(*kStereoLink, aStereoLink, "stereo_link", "%"); bindKnob(*kFbBlend, aFbBlend, "fb_blend", "%");

    bindKnob(*kCrestTarget, aCrestTarget, "crest_target", "dB"); bindKnob(*kCrestSpeed, aCrestSpeed, "crest_speed", "ms");

    bindKnob(*kTpAmt, aTpAmt, "tp_amount", "%"); bindKnob(*kTpRaise, aTpRaise, "tp_thresh_raise", "dB");
    bindKnob(*kFluxAmt, aFluxAmt, "flux_amount", "%");

    bindKnob(*kSatDrive, aSatDrive, "sat_drive", "dB"); bindKnob(*kSatTrim, aSatTrim, "sat_trim", "dB");
    bindKnob(*kSatMix, aSatMix, "sat_mix", "%");

    bindKnob(*kTone, aTone, "sat_tone", "dB"); bindKnob(*kToneFreq, aToneFreq, "sat_tone_freq", "Hz");
    bindKnob(*kBright, aBright, "harm_bright", "dB"); bindKnob(*kBrightFreq, aBrightFreq, "harm_freq", "Hz");

    initCombo(cAutoRel, aAutoRel, "auto_rel");
    initCombo(cThrust, aThrust, "thrust_mode");
    initCombo(cCtrlMode, aCtrlMode, "ctrl_mode");
    initCombo(cTpMode, aTpMode, "tp_mode");
    initCombo(cFluxMode, aFluxMode, "flux_mode");
    initCombo(cSatMode, aSatMode, "sat_mode");
    initCombo(cSignalFlow, aSignalFlow, "signal_flow");

    aTurbo = std::make_unique<ButtonAttachment>(audioProcessor.apvts, "turbo", bTurbo);

    setResizable(true, true);
    setResizeLimits(1000, 600, 2000, 1200);
    setSize(1100, 680);
    startTimerHz(60);
}

UltimateCompAudioProcessorEditor::~UltimateCompAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void UltimateCompAudioProcessorEditor::bindKnob(Knob& knob, std::unique_ptr<SliderAttachment>& attachment,
    const juce::String& paramID, const juce::String& suffix)
{
    knob.getSlider().setName(paramID);
    attachment = std::make_unique<SliderAttachment>(audioProcessor.apvts, paramID, knob.getSlider());
    knob.setUnitSuffix(suffix);
    knob.updateValueLabel();
}

void UltimateCompAudioProcessorEditor::initCombo(juce::ComboBox& box, std::unique_ptr<ComboBoxAttachment>& attachment,
    const juce::String& paramID)
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
    repaint();
}

void UltimateCompAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(UltimateLNF::c(UltimateLNF::bgA));
    g.setGradientFill(juce::ColourGradient(UltimateLNF::c(UltimateLNF::bgA), 0, 0,
        UltimateLNF::c(UltimateLNF::bgB), 0, (float)getHeight(), false));
    g.fillAll();

    g.setColour(UltimateLNF::c(UltimateLNF::line).withAlpha(0.20f));
    for (int x = 0; x < getWidth(); x += 20) g.drawVerticalLine(x, 0.0f, (float)getHeight());
    for (int y = 0; y < getHeight(); y += 20) g.drawHorizontalLine(y, 0.0f, (float)getWidth());

    g.setColour(UltimateLNF::c(UltimateLNF::text));
    g.setFont(juce::FontOptions(22.0f).withStyle("bold"));
    g.drawText("NS - MixBus", 20, 10, 200, 30, juce::Justification::left);
}

void UltimateCompAudioProcessorEditor::paintOverChildren(juce::Graphics& g)
{
    // HUGE METERS
    auto drawHugeMeter = [&](juce::Rectangle<int> r, float L, float R, juce::String label) {
        g.setColour(UltimateLNF::c(UltimateLNF::panel2).withAlpha(0.9f));
        g.fillRoundedRectangle(r.toFloat(), 6.0f);
        g.setColour(UltimateLNF::c(UltimateLNF::edge));
        g.drawRoundedRectangle(r.toFloat(), 6.0f, 1.5f);

        int w = r.getWidth() - 35;
        int h = r.getHeight() / 2 - 2;
        int barX = r.getX() + 30;
        int barY = r.getY() + 2;

        g.setColour(UltimateLNF::c(UltimateLNF::ok));
        int fillL = (int)(w * juce::jlimit(0.0f, 1.0f, L * 1.5f));
        g.fillRect(barX, barY, fillL, h);
        int fillR = (int)(w * juce::jlimit(0.0f, 1.0f, R * 1.5f));
        g.fillRect(barX, barY + h + 2, fillR, h);

        g.setColour(UltimateLNF::c(UltimateLNF::text));
        g.setFont(juce::FontOptions(11.0f).withStyle("bold"));
        g.drawText(label, r.getX(), r.getY(), 30, r.getHeight(), juce::Justification::centred);
        };

    drawHugeMeter(inMeterArea, smoothInL, smoothInR, "IN");
    drawHugeMeter(outMeterArea, smoothOutL, smoothOutR, "OUT");

    // GR BAR - LARGER
    if (!grBarArea.isEmpty()) {
        g.setColour(UltimateLNF::c(UltimateLNF::panel2));
        g.fillRoundedRectangle(grBarArea.toFloat(), 6.0f);

        float grInv = std::abs(smoothGR);
        float w = (float)grBarArea.getWidth() * (juce::jlimit(0.0f, 24.0f, grInv) / 24.0f);

        if (w > 1.0f) {
            juce::Graphics::ScopedSaveState ss(g);
            g.reduceClipRegion((int)(grBarArea.getRight() - w), grBarArea.getY(), (int)w, grBarArea.getHeight());

            g.setColour(UltimateLNF::c(UltimateLNF::warn));
            for (int i = -20; i < (int)w + 20; i += 6) {
                g.drawLine((float)(grBarArea.getRight() - w + i), (float)grBarArea.getBottom(),
                    (float)(grBarArea.getRight() - w + i + 4), (float)grBarArea.getY(), 2.0f);
            }
        }

        g.setColour(UltimateLNF::c(UltimateLNF::white));
        g.setFont(juce::FontOptions(16.0f).withStyle("bold"));
        g.drawText(juce::String(smoothGR, 1) + " dB", grBarArea, juce::Justification::centred);

        g.setColour(UltimateLNF::c(UltimateLNF::text2));
        g.setFont(juce::FontOptions(12.0f));
        g.drawText("GR", grBarArea.getX() - 30, grBarArea.getY(), 25, grBarArea.getHeight(), juce::Justification::right);
    }

    auto r = fluxDotArea.toFloat();
    g.setColour(UltimateLNF::c(UltimateLNF::panel2));
    g.fillEllipse(r);
    if (smoothFlux > 0.01f) {
        g.setColour(UltimateLNF::c(UltimateLNF::warn).withAlpha(juce::jlimit(0.2f, 1.0f, smoothFlux)));
        g.fillEllipse(r.reduced(2));
    }

    // DRAW CONNECTING LINES (Attack/Release -> Faster/Harder)
    if (panelDyn && kAttack && kRelease && bTurbo.isVisible())
    {
        auto pPos = panelDyn->getPosition();
        // Calculate Global Positions
        auto btnC = bTurbo.getBounds().getCentre() + pPos;
        auto attC = kAttack->getBounds().getCentre() + pPos;
        auto relC = kRelease->getBounds().getCentre() + pPos;

        // Shift knob connection points up to the "Top/Label" area of the knob
        int yOffset = 25;
        attC.y -= yOffset;
        relC.y -= yOffset;

        // Connection point on button (Bottom Center)
        auto btnBot = btnC;
        btnBot.y += 10; // Slightly below center of button

        g.setColour(UltimateLNF::c(UltimateLNF::ok).withAlpha(0.6f));

        g.drawLine(juce::Line<float>(attC.toFloat(), btnBot.toFloat()), 1.5f);
        g.drawLine(juce::Line<float>(relC.toFloat(), btnBot.toFloat()), 1.5f);
    }

    // --- NEW: Draw Labels Above Dropdowns ---
    auto drawLabel = [&](juce::Component& c, juce::String text) {
        if (auto* p = c.getParentComponent()) {
            auto r = c.getBoundsInParent(); // Relative to Panel
            // Convert to Editor coordinates
            auto rEditor = r + p->getPosition();

            g.setColour(UltimateLNF::c(UltimateLNF::text2));
            g.setFont(juce::FontOptions(10.0f).withStyle("bold")); // Small, readable

            // Draw slightly above the component
            juce::Rectangle<int> labelRect(rEditor.getX(), rEditor.getY() - 12, rEditor.getWidth(), 12);
            g.drawText(text, labelRect, juce::Justification::centred);
        }
        };

    drawLabel(cAutoRel, "RELEASE");
    drawLabel(cThrust, "THRUST");
    drawLabel(cCtrlMode, "CONTROL");

    drawLabel(cFluxMode, "FLUX");
    drawLabel(cTpMode, "TRANSIENT");

    drawLabel(cSatMode, "TRANSFORMER");
    drawLabel(cSignalFlow, "FLOW");
}

void UltimateCompAudioProcessorEditor::resized()
{
    //--------------------------------------------------------------------------
    // Responsive scaling anchored to the default editor size.
    // This keeps all controls proportional and prevents slot overflow (overlap)
    // as the editor is resized within the configured limits.
    //--------------------------------------------------------------------------
    constexpr float baseW = 1100.0f;
    constexpr float baseH = 680.0f;

    const float s = juce::jlimit(0.75f, 2.0f,
        juce::jmin(getWidth() / baseW,
            getHeight() / baseH));

    auto si = [s](float v) -> int
        {
            return juce::jmax(1, (int)std::lround(v * s));
        };

    const int outerPad = si(15.0f);
    auto r = getLocalBounds().reduced(outerPad);

    // Scale panel headers to match UI scale
    const int headerH = si(26.0f);
    panelDyn->setHeaderHeight(headerH);
    panelDet->setHeaderHeight(headerH);
    panelCrest->setHeaderHeight(headerH);
    panelTpFlux->setHeaderHeight(headerH);
    panelSat->setHeaderHeight(headerH);
    panelEq->setHeaderHeight(headerH);

    // Top meters
    auto topBar = r.removeFromTop(si(60.0f));

    const int meterH = si(24.0f);
    const int meterGap = si(6.0f);
    const int metersTotalWidth = juce::jmin(topBar.getWidth(), si(300.0f));

    auto centerMeters = topBar.withWidth(metersTotalWidth)
        .withX((getWidth() - metersTotalWidth) / 2);

    inMeterArea = centerMeters.removeFromTop(meterH);
    centerMeters.removeFromTop(meterGap);
    outMeterArea = centerMeters.removeFromTop(meterH);

    // Rows
    const int rowH = r.getHeight() / 3;
    auto row1 = r.removeFromTop(rowH);
    auto row2 = r.removeFromTop(rowH);
    auto row3 = r;

    // Standard control dimensions (scaled)
    const int comboH = si(20.0f);
    const int comboW = si(90.0f);

    const int panelPadX = si(2.0f);
    const int panelPadY = si(5.0f);

    // --- 1. FIXED KNOB SIZES (SCALED) ---
    // 64x74 creates a nice uniform size for all knobs
    const int fixedKnobW = si(64.0f);
    const int fixedKnobH = si(74.0f);

    // Helper: centers a fixed size knob in the given slot
    auto placeKnob = [&](Knob* k, juce::Rectangle<int> slot)
        {
            if (k == nullptr) return;
            // Center the fixed dimensions within the slot
            k->setBounds(slot.withSizeKeepingCentre(fixedKnobW, fixedKnobH));
        };

    // --- DYNAMICS ---
    panelDyn->setBounds(row1.reduced(0, panelPadY));
    {
        auto cLocal = panelDyn->getContentBounds();

        // Bottom strip for GR bar + Auto Release
        auto bot = cLocal.removeFromBottom(si(44.0f));

        // GR Bar (centered, scaled, never exceeds available width)
        const int barW = juce::jmin(bot.getWidth(), si(300.0f));
        const int barH = juce::jmin(bot.getHeight(), si(24.0f));
        auto bar = bot.withWidth(barW).withHeight(barH);
        bar.setX(bot.getCentreX() - barW / 2);
        bar.setY(bot.getY() + (bot.getHeight() - barH) / 2);
        grBarArea = bar.translated(panelDyn->getX(), panelDyn->getY());

        // AutoRel bottom-right
        auto botRight = bot.removeFromRight(comboW + si(10.0f));
        cAutoRel.setBounds(botRight.withSizeKeepingCentre(comboW, comboH));

        // Reserve top strip for the "Faster/Harder" button
        auto topStrip = cLocal.removeFromTop(si(24.0f));

        const int colW = cLocal.getWidth() / 7;

        placeKnob(kThresh.get(), cLocal.removeFromLeft(colW));
        placeKnob(kRatio.get(), cLocal.removeFromLeft(colW));
        placeKnob(kKnee.get(), cLocal.removeFromLeft(colW));

        // Capture layout slots for Attack/Release so the Turbo button can anchor between them
        auto rAttackSlot = cLocal.removeFromLeft(colW);
        placeKnob(kAttack.get(), rAttackSlot);

        auto rReleaseSlot = cLocal.removeFromLeft(colW);
        placeKnob(kRelease.get(), rReleaseSlot);

        placeKnob(kMakeup.get(), cLocal.removeFromLeft(colW));
        placeKnob(kMix.get(), cLocal.removeFromLeft(colW));

        // Turbo button positioned in the top strip, centered over Attack/Release boundary
        auto turboSpan = topStrip.withX(rAttackSlot.getX())
            .withWidth(rAttackSlot.getWidth() + rReleaseSlot.getWidth());

        const int turboW = juce::jmin(turboSpan.getWidth(), si(140.0f));
        const int turboH = juce::jmin(turboSpan.getHeight(), si(24.0f));

        bTurbo.setBounds(turboSpan.withSizeKeepingCentre(turboW, turboH));
    }

    // --- DETECTOR & CREST ---
    // Split row 2
    {
        const int detW = (int)std::lround(row2.getWidth() * 0.6f);
        panelDet->setBounds(row2.removeFromLeft(detW).reduced(panelPadX, panelPadY));
        panelCrest->setBounds(row2.reduced(panelPadX, panelPadY));
    }

    {
        // Detector
        auto c = panelDet->getContentBounds().reduced(si(2.0f));
        auto bot = c.removeFromBottom(si(44.0f));
        cThrust.setBounds(bot.removeFromRight(comboW).withSizeKeepingCentre(comboW, comboH));

        const int w = c.getWidth() / 4;
        placeKnob(kScHpf.get(), c.removeFromLeft(w));
        placeKnob(kDetRms.get(), c.removeFromLeft(w));
        placeKnob(kStereoLink.get(), c.removeFromLeft(w));
        placeKnob(kFbBlend.get(), c);
    }

    {
        // Crest
        auto c = panelCrest->getContentBounds().reduced(si(2.0f));
        auto bot = c.removeFromBottom(si(44.0f));
        cCtrlMode.setBounds(bot.removeFromRight(comboW).withSizeKeepingCentre(comboW, comboH));

        const int w = c.getWidth() / 2;
        placeKnob(kCrestTarget.get(), c.removeFromLeft(w));
        placeKnob(kCrestSpeed.get(), c);
    }

    // --- TP, SAT, EQ ---
    const int w3 = row3.getWidth() / 3;
    panelTpFlux->setBounds(row3.removeFromLeft(w3).reduced(panelPadX, panelPadY));
    panelSat->setBounds(row3.removeFromLeft(w3).reduced(panelPadX, panelPadY));
    panelEq->setBounds(row3.reduced(panelPadX, panelPadY));

    {
        // TP / Flux
        auto c = panelTpFlux->getContentBounds().reduced(si(2.0f));
        auto bot = c.removeFromBottom(si(44.0f));

        const int miniSlot = si(70.0f);
        const int miniW = si(65.0f);
        cFluxMode.setBounds(bot.removeFromRight(miniSlot).withSizeKeepingCentre(miniW, comboH));
        cTpMode.setBounds(bot.removeFromRight(miniSlot).withSizeKeepingCentre(miniW, comboH));

        const int w = c.getWidth() / 3;
        placeKnob(kTpAmt.get(), c.removeFromLeft(w));
        placeKnob(kTpRaise.get(), c.removeFromLeft(w));
        placeKnob(kFluxAmt.get(), c);
    }

    {
        // Saturation
        auto c = panelSat->getContentBounds().reduced(si(4.0f));

        // "Flux" indicator dot (scaled)
        auto satRect = panelSat->getBounds();
        const int dotPadR = si(30.0f);
        const int dotY = si(8.0f);
        const int dotS = si(12.0f);
        fluxDotArea = juce::Rectangle<int>(satRect.getRight() - dotPadR, satRect.getY() + dotY, dotS, dotS);

        auto bot = c.removeFromBottom(si(44.0f));

        const int miniSlot = si(70.0f);
        const int miniW = si(65.0f);
        cSatMode.setBounds(bot.removeFromRight(miniSlot).withSizeKeepingCentre(miniW, comboH));
        cSignalFlow.setBounds(bot.removeFromRight(miniSlot).withSizeKeepingCentre(miniW, comboH));

        const int w = c.getWidth() / 3;
        placeKnob(kSatDrive.get(), c.removeFromLeft(w));
        placeKnob(kSatTrim.get(), c.removeFromLeft(w));
        placeKnob(kSatMix.get(), c);
    }

    {
        // EQ (No switches here, just knobs)
        auto c = panelEq->getContentBounds().reduced(si(4.0f));
        const int w = c.getWidth() / 4;
        placeKnob(kTone.get(), c.removeFromLeft(w));
        placeKnob(kToneFreq.get(), c.removeFromLeft(w));
        placeKnob(kBright.get(), c.removeFromLeft(w));
        placeKnob(kBrightFreq.get(), c);
    }
}