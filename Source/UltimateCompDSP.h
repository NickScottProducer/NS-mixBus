/*
  ==============================================================================
    UltimateCompDSP.h
    Port of Ultimate Mix Bus Compressor v3.20
    - Fixed Latency Reporting (Constant)
    - True Compressor Auto-Gain (Hybrid RMS)
    - Fixed Saturator Auto-Gain (Proper Staging)
    - Fixed Feedback Detector Tap
    - FIXED: Member variable naming in Audition Block
    - UPDATED: Removed SC->Sat, Extended Filter Ranges
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include "SimpleBiquad.h"

class UltimateCompDSP
{
public:
    UltimateCompDSP() { resetState(); }

    // ==============================================================================
    // PUBLIC PARAMETERS
    // ==============================================================================

    // --- GLOBAL ---
    int   p_signal_flow = 0; // 0 = Comp > Sat, 1 = Sat > Comp

    // --- MODULE BYPASS STATES ---
    bool p_active_dyn = true;
    bool p_active_det = true;
    bool p_active_crest = true;
    bool p_active_tf = true;
    bool p_active_sat = true;
    bool p_active_eq = true;

    // --- SIDECHAIN EXPERT ---
    int  p_sc_input_mode = 0; // 0 = Internal, 1 = External
    bool p_sc_to_comp = true;
    // REMOVED: p_sc_to_sat
    int  p_ms_mode = 0;
    float p_ms_balance_db = 0.0f; // NEW: M/S balance for cross modes

    // --- DYNAMICS ---
    float p_comp_input = 0.0f;
    bool  p_comp_mirror = false;

    float p_thresh = -20.0f;
    float p_ratio = 4.0f;
    float p_knee = 6.0f;
    float p_att_ms = 10.0f;
    float p_rel_ms = 100.0f;
    int   p_auto_rel = 0;

    // NEW: Compressor Auto-Gain
    int   p_comp_autogain_mode = 0; // 0=Off, 1=Partial, 2=Full

    bool  p_turbo_att = false;
    bool  p_turbo_rel = false;

    // --- AUTO CREST ---
    int   p_ctrl_mode = 0;
    float p_crest_target = 12.0f;
    float p_crest_speed = 400.0f;

    // --- DETECTOR ---
    int   p_thrust_mode = 0;
    float p_det_rms = 0.0f;
    float p_stereo_link = 100.0f;
    float p_sc_hp_freq = 20.0f;
    float p_sc_lp_freq = 20000.0f;
    float p_fb_blend = 0.0f;
    float p_sc_level_db = 0.0f; // Sidechain Level Trim (dB)
    bool  p_sc_audition = false; // Monitor detector feed

    // --- SIDECHAIN TRANSIENT DESIGNER (Detector Conditioning) ---
    float p_sc_td_amt = 0.0f;   // -100..100 (transient emphasis)
    float p_sc_td_ms = 0.0f;   // 0..100 (0=Mid focus, 100=Side focus)

    // --- TRANSIENT PRIORITY ---
    int   p_tp_mode = 0;
    float p_tp_amount = 50.0f;
    float p_tp_thresh_raise = 12.0f;

    // --- FLUX COUPLED ---
    int   p_flux_mode = 0;
    float p_flux_amount = 30.0f;

    // --- SATURATION ---
    int   p_sat_mode = 0;
    float p_sat_pre_gain = 0.0f;
    bool  p_sat_mirror = false;
    float p_sat_drive = 0.0f;
    float p_sat_trim = 0.0f;
    float p_sat_tone = 0.0f;
    float p_sat_tone_freq = 5500.0f;
    float p_sat_mix = 100.0f;
    int   p_sat_autogain_mode = 1;

    // --- HARMONIC BRIGHTNESS ---
    float p_harm_bright = 0.0f;
    float p_harm_freq = 4500.0f;

    // --- COLOR EQ (Pultec-style low-end) ---
    float p_girth = 0.0f;        // dB
    int   p_girth_freq_sel = 2;  // 0=20,1=30,2=60,3=100

    // --- DEBUG TUNING KNOBS (Temporary) ---
    float p_debug_boost_q = 0.5f;
    float p_debug_dip_q = 0.5f;
    float p_debug_ratio = 0.35f;

    // --- OUTPUT ---
    float p_makeup = 0.0f;
    float p_dry_wet = 100.0f;
    float p_out_trim = 0.0f;

    // ==============================================================================
    // GETTERS
    // ==============================================================================
    float getGainReductiondB() const { return (float)env; }
    float getFluxSaturation() const { return (float)flux_env; }
    float getCrestAmt() const { return (float)cf_amt; }

    // Latency is only incurred when the Sat/EQ oversampled block is active.
    double getLatency() const { return (p_active_sat ? (double)os_latency_samples : 0.0); }
// ==============================================================================
    // LIFECYCLE
    // ==============================================================================

    void prepare(double sampleRate, int maxBlockSamples)
    {
        s_rate = (sampleRate > 1.0 ? sampleRate : 44100.0);
        max_block = std::max(1, maxBlockSamples);

        // Prepare Oversampling (fixed 4x)
        os_stages = 2;
        os_factor = 1 << os_stages;

        dry_buf.setSize(2, max_block, false, false, true);
        wet_buf.setSize(2, max_block, false, false, true);
        sc_internal_buf.setSize(2, max_block, false, false, true);
        // Work buffers are base-rate; Oversampling maintains its own internal up/down buffers.
        sat_clean_buf.setSize(2, max_block, false, false, true);
        sat_proc_buf.setSize(2, max_block, false, false, true);

        os = std::make_unique<juce::dsp::Oversampling<float>>(
            2, os_stages, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        os->initProcessing((size_t)max_block);

        os_dry = std::make_unique<juce::dsp::Oversampling<float>>(
            2, os_stages, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        os_dry->initProcessing((size_t)max_block);

        // Cache oversampling latency for host reporting
        os_latency_samples = (int)os->getLatencyInSamples();

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = s_rate;
        spec.maximumBlockSize = (juce::uint32)max_block;
        spec.numChannels = 2;

        satInternalDelay.prepare(spec);
        satInternalDelay.reset();
// Pre-size RMS ring buffer (max 300 ms) so detector window changes never allocate on the audio thread.
        rms_window_max = juce::jmax(1, (int)std::ceil(0.300 * s_rate));
        rms_ring_l.assign((size_t)rms_window_max, 0.0);
        rms_ring_r.assign((size_t)rms_window_max, 0.0);
        rms_window = 1;
        rms_pos = 0;
        rms_sum_l = rms_sum_r = 0.0;

        resetState();
        updateParameters();
    }

    // Safe to call from the message thread (e.g., releaseResources) to clear DSP state.
    void reset() noexcept
    {
        if (os) os->reset();
        if (os_dry) os_dry->reset();

        satInternalDelay.reset();
resetState();
    }


    void resetState()
    {
        // Filters
        sc_hp_l.reset(); sc_hp_r.reset(); sc_hp_l_2.reset(); sc_hp_r_2.reset();
        sc_lp_l.reset(); sc_lp_r.reset(); sc_lp_l_2.reset(); sc_lp_r_2.reset();
        sat_tone_l.reset(); sat_tone_r.reset();
        girth_bump_l.reset(); girth_bump_r.reset(); girth_dip_l.reset(); girth_dip_r.reset();
        harm_pre_l.reset(); harm_pre_r.reset(); harm_post_l.reset(); harm_post_r.reset();
        iron_voicing_l.reset(); iron_voicing_r.reset();
        steel_low_l.reset(); steel_low_r.reset(); steel_high_l.reset(); steel_high_r.reset();

        if (os) os->reset();
        if (os_dry) os_dry->reset();
        satInternalDelay.reset();
steel_phi_l = steel_phi_r = 0.0;
        steel_prev_x_l = steel_prev_x_r = 0.0;
        sat_agc_gain_sm = 1.0;
        sc_level_sm = 1.0;
        ms_bal_sm = 1.0;


        sc_td_amt_sm = 0.0;
        sc_td_ms_sm = 0.0;
        sc_td_fast_mid = sc_td_slow_mid = 0.0;
        sc_td_fast_side = sc_td_slow_side = 0.0;

        fb_prev_l = fb_prev_r = 0.0;
        det_env = 0.0;
        env = 0.0;
        env_l = env_r = 0.0;
        env_fast = env_slow = 0.0;
        env_fast_l = env_fast_r = 0.0;
        env_slow_l = env_slow_r = 0.0;
        cf_peak_env = 0.0; cf_rms_sum = 0.0; cf_amt = 0.0; cf_ratio_mix = 0.0;
        flux_env = 0.0;

        // Auto-Gain States
        comp_agc_gain_sm = 1.0;
        sat_agc_gain_sm = 1.0;

        thresh_sm = p_thresh;
        ratio_sm = std::max(1.0, (double)p_ratio);
        knee_sm = std::max(0.0, (double)p_knee);
        makeup_lin_sm = dbToLin((double)p_makeup);
        comp_in_sm = dbToLin((double)p_comp_input);

        sat_pre_lin_sm = dbToLin((double)p_sat_pre_gain);
        sat_drive_lin_sm = dbToLin((double)p_sat_drive);
        sat_trim_lin_sm = dbToLin((double)p_sat_trim);
        sat_mix_sm = juce::jlimit(0.0, 1.0, (double)p_sat_mix / 100.0);

        last_sat_mode = -1;
        last_ctrl_mode = -1;

        // Topology-change click smoothing (start stable)
        topologyRamp = 1.0;
        topologyInc = 0.0;
        prevTopoSatEq = (p_active_sat || p_active_eq);
        prevTopoFlow = p_signal_flow;
        prevTopoAudition = p_sc_audition;
        prevTopoMsMode = p_ms_mode;
        prevTopoScMode = p_sc_input_mode;
        prevTopoScToComp = p_sc_to_comp;
    }


    void armTopologyFade() noexcept
    {
        const int maxFade = juce::jmax(16, max_block);
        const int fadeSamples = juce::jlimit(16, maxFade, (int)std::round(0.010 * s_rate)); // 10 ms
        topologyRamp = 0.0;
        topologyInc = 1.0 / (double)fadeSamples;
    }

    void handleTopologyChangeIfNeeded() noexcept
    {
        const bool satEq = (p_active_sat || p_active_eq);
        const bool audition = p_sc_audition;
        const int flow = p_signal_flow;
        const int msMode = p_ms_mode;
        const int scMode = p_sc_input_mode;
        const bool scToComp = p_sc_to_comp;

        const bool changed =
            (satEq != prevTopoSatEq) ||
            (audition != prevTopoAudition) ||
            (flow != prevTopoFlow) ||
            (msMode != prevTopoMsMode) ||
            (scMode != prevTopoScMode) ||
            (scToComp != prevTopoScToComp);

        if (!changed) return;

        // Reset latency-matching paths and oversampling state; then fade the wet contribution back in.
        if (os) os->reset();
        if (os_dry) os_dry->reset();

        satInternalDelay.reset();
armTopologyFade();

        prevTopoSatEq = satEq;
        prevTopoAudition = audition;
        prevTopoFlow = flow;
        prevTopoMsMode = msMode;
        prevTopoScMode = scMode;
        prevTopoScToComp = scToComp;
    }

    void process(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>* sidechainBuffer = nullptr)
    {
        juce::ScopedNoDenormals noDenormals;

        const int totalSamples = buffer.getNumSamples();
        if (totalSamples <= 0) return;

        // If the host ever delivers a larger-than-expected block, we process it in fixed-size chunks
        // so we never need to resize/allocate on the audio thread.
        const int chunkSize = juce::jmax(1, max_block);

        // Smooth transitions for topology-affecting changes (order, routing, oversampling path).
        handleTopologyChangeIfNeeded();

        int offset = 0;
        while (offset < totalSamples)
        {
            const int nSamp = juce::jmin(chunkSize, totalSamples - offset);

            // 1) Snapshot Input for Dry/Wet mix later (chunk)
            dry_buf.setSize(2, nSamp, false, false, true);
            wet_buf.setSize(2, nSamp, false, false, true);
            sc_internal_buf.setSize(2, nSamp, false, false, true);

            const float* inL = buffer.getReadPointer(0) + offset;
            const float* inR = (buffer.getNumChannels() > 1) ? (buffer.getReadPointer(1) + offset) : inL;

            dry_buf.copyFrom(0, 0, inL, nSamp);
            dry_buf.copyFrom(1, 0, inR, nSamp);

            wet_buf.copyFrom(0, 0, inL, nSamp);
            wet_buf.copyFrom(1, 0, inR, nSamp);

            // 2) Prepare Sidechain Buffer (chunk)
            if (p_sc_input_mode == 1 && sidechainBuffer != nullptr && sidechainBuffer->getNumChannels() > 0)
            {
                const float* scL = sidechainBuffer->getReadPointer(0) + offset;
                const float* scR = (sidechainBuffer->getNumChannels() > 1) ? (sidechainBuffer->getReadPointer(1) + offset) : scL;

                sc_internal_buf.copyFrom(0, 0, scL, nSamp);
                sc_internal_buf.copyFrom(1, 0, scR, nSamp);
            }
            else
            {
                sc_internal_buf.copyFrom(0, 0, dry_buf, 0, 0, nSamp);
                sc_internal_buf.copyFrom(1, 0, dry_buf, 1, 0, nSamp);
            }

            smooth_alpha_block = std::exp(-(double)nSamp / (0.020 * s_rate));

            // Update coefficients and control values for this chunk.
            updateParameters();

            // 3) Processing Chain (on wet_buf)
            if (p_sc_audition)
            {
                // Monitor the detector feed (post SC gain + HP/LP + Thrust + M/S selection),
                // without applying compression/saturation.
                processAuditionBlock(wet_buf);

                // Preserve oversampling latency behavior so toggling audition does not change timing.
                if (p_active_sat && os)
                {
                    auto block = juce::dsp::AudioBlock<float>(wet_buf);
                    auto osBlock = os->processSamplesUp(block);
                    (void)osBlock;
                    os->processSamplesDown(block);
                }
            }
            else
            {
                if (p_signal_flow == 1) // Sat > Comp
                {
                    processSaturationBlock(wet_buf);
                    processCompressorBlock(wet_buf);
                }
                else // Comp > Sat
                {
                    processCompressorBlock(wet_buf);
                    processSaturationBlock(wet_buf);
                }
            }

            // 4) Latency Compensation for DRY signal
            // If Saturation/EQ block ran, the wet signal is delayed by OS.
            // We must delay the dry signal to match.
            if (p_active_sat && os && os_dry)
            {
                auto block = juce::dsp::AudioBlock<float>(dry_buf);
                auto osBlock = os_dry->processSamplesUp(block);
                (void)osBlock;
                os_dry->processSamplesDown(block);
            }
// 5) Final Mixer (write into the output buffer segment)
            const double dw_target = p_sc_audition ? 1.0 : juce::jlimit(0.0, 1.0, (double)p_dry_wet / 100.0);
            drywet_sm = smooth1p(drywet_sm, dw_target, smooth_alpha_block);

            const double final_gain_target = dbToLin((double)p_out_trim);
            out_lin_sm = smooth1p(out_lin_sm, final_gain_target, smooth_alpha_block);
            const float finalGain = (float)out_lin_sm;

            float* outL = buffer.getWritePointer(0) + offset;
            float* outR = (buffer.getNumChannels() > 1) ? (buffer.getWritePointer(1) + offset) : nullptr;

            const float* wetL = wet_buf.getReadPointer(0);
            const float* wetR = wet_buf.getReadPointer(1);
            const float* dryL = dry_buf.getReadPointer(0);
            const float* dryR = dry_buf.getReadPointer(1);

            for (int i = 0; i < nSamp; ++i)
            {
                // Fade in the "wet contribution" after topology changes to avoid clicks.
                if (topologyRamp < 1.0)
                    topologyRamp = std::min(1.0, topologyRamp + topologyInc);

                const float wm = (float)(drywet_sm * topologyRamp);
                const float dm = 1.0f - wm;

                outL[i] = (wetL[i] * wm + dryL[i] * dm) * finalGain;
                if (outR)
                    outR[i] = (wetR[i] * wm + dryR[i] * dm) * finalGain;
            }

            offset += nSamp;
        }
    }

    void updateParameters()
    {
        const double attMul = (p_turbo_att ? 0.1 : 1.0);
        const double relMul = (p_turbo_rel ? 0.1 : 1.0);
        const double att_ms = std::max(0.05, (double)p_att_ms * attMul);
        const double rel_ms = std::max(1.0, (double)p_rel_ms * relMul);

        att_coeff = std::exp(-1000.0 / (att_ms * s_rate));
        rel_coeff_manual = std::exp(-1000.0 / (rel_ms * s_rate));
        auto_rel_slow = std::exp(-1000.0 / (1200.0 * s_rate));
        auto_rel_fast = std::exp(-1000.0 / (80.0 * s_rate));

        use_rms = (p_det_rms > 0.0f);
        if (use_rms) {
            const double win_ms = std::max(1.0, (double)p_det_rms);
            const int desired = std::max(1, (int)std::round((win_ms * 0.001) * s_rate));
            const int clamped = std::min(desired, rms_window_max);
            if (clamped != rms_window)
            {
                rms_window = clamped;
                rms_pos = 0;
                rms_sum_l = rms_sum_r = 0.0;
                std::fill(rms_ring_l.begin(), rms_ring_l.begin() + (size_t)rms_window, 0.0);
                std::fill(rms_ring_r.begin(), rms_ring_r.begin() + (size_t)rms_window, 0.0);
            }
        }

        stereo_link = juce::jlimit(0.0, 1.0, (double)p_stereo_link / 100.0);
        fb_blend = juce::jlimit(0.0, 1.0, (double)p_fb_blend / 100.0);

        sc_hp_l.update_hpf((double)p_sc_hp_freq, 0.707, s_rate);
        sc_hp_r.update_hpf((double)p_sc_hp_freq, 0.707, s_rate);
        sc_hp_l_2.update_hpf((double)p_sc_hp_freq, 0.707, s_rate);
        sc_hp_r_2.update_hpf((double)p_sc_hp_freq, 0.707, s_rate);

        sc_lp_l.update_lpf(std::max(40.0, (double)p_sc_lp_freq), 0.707, s_rate);
        sc_lp_r.update_lpf(std::max(40.0, (double)p_sc_lp_freq), 0.707, s_rate);
        sc_lp_l_2.update_lpf(std::max(40.0, (double)p_sc_lp_freq), 0.707, s_rate);
        sc_lp_r_2.update_lpf(std::max(40.0, (double)p_sc_lp_freq), 0.707, s_rate);

        thrust_gain_db = 0.0;
        if (p_thrust_mode == 1) thrust_gain_db = 3.0;
        if (p_thrust_mode == 2) thrust_gain_db = 6.0;
        if (p_thrust_mode > 0) {
            sc_shelf_l.update_shelf(90.0, thrust_gain_db, 0.707, s_rate);
            sc_shelf_r.update_shelf(90.0, thrust_gain_db, 0.707, s_rate);
        }

        crest_target_db = (double)p_crest_target;
        crest_speed_ms = std::max(5.0, (double)p_crest_speed);
        crest_coeff = std::exp(-1000.0 / (crest_speed_ms * s_rate));

        tp_enabled = (p_tp_mode != 0);
        tp_amt = juce::jlimit(0.0, 1.0, (double)p_tp_amount / 100.0);
        tp_raise_db = std::max(0.0, (double)p_tp_thresh_raise);

        flux_enabled = (p_flux_mode != 0);
        flux_amt = juce::jlimit(0.0, 1.0, (double)p_flux_amount / 100.0);

        comp_in_target = dbToLin((double)p_comp_input);
        makeup_lin_target = dbToLin((double)p_makeup);
        sc_level_target = dbToLin((double)p_sc_level_db);
        ms_bal_target = dbToLin((double)p_ms_balance_db);

        // Sidechain transient designer
        sc_td_amt_target = juce::jlimit(-1.0, 1.0, (double)p_sc_td_amt / 100.0);
        sc_td_ms_target = juce::jlimit(0.0, 1.0, (double)p_sc_td_ms / 100.0);
        sc_td_fast_att = std::exp(-1000.0 / (1.0 * s_rate));
        sc_td_fast_rel = std::exp(-1000.0 / (30.0 * s_rate));
        sc_td_slow_att = std::exp(-1000.0 / (25.0 * s_rate));
        sc_td_slow_rel = std::exp(-1000.0 / (250.0 * s_rate));

        smooth_alpha = std::exp(-1.0 / (0.020 * s_rate));
        os_srate = s_rate * (double)os_factor;
        smooth_alpha_os = std::exp(-1.0 / (0.020 * os_srate));

        sat_tone_l.update_shelf((double)p_sat_tone_freq, (double)p_sat_tone, 0.707, s_rate);
        sat_tone_r.update_shelf((double)p_sat_tone_freq, (double)p_sat_tone, 0.707, s_rate);

        // --- PULTEC-STYLE LOW-END TRICK (TUNED) ---
        {
            const int idx = juce::jlimit(0, 3, p_girth_freq_sel);
            static const double freqs[4] = { 20.0, 30.0, 60.0, 100.0 };
            static const double dips[4] = { 65.0, 97.5, 195.0, 325.0 };

            const double f0 = freqs[idx];
            const double fd = dips[idx];

            const double bumpQ = 1.0;
            const double dipQ  = 0.6;

            const double bumpDb = (double)p_girth;
            const double dipDb  = -(double)p_girth * 0.80;
            girth_bump_l.update_low_shelf(f0 * 4.0, bumpDb, bumpQ, s_rate);
            girth_bump_r.update_low_shelf(f0 * 4.0, bumpDb, bumpQ, s_rate);
            girth_dip_l.update_peak(fd, dipDb, dipQ, s_rate);
            girth_dip_r.update_peak(fd, dipDb, dipQ, s_rate);
        }

        const double hb = (double)p_harm_bright;
        harm_pre_l.update_shelf((double)p_harm_freq, -hb, 0.707, os_srate);
        harm_pre_r.update_shelf((double)p_harm_freq, -hb, 0.707, os_srate);
        harm_post_l.update_shelf((double)p_harm_freq, hb, 0.707, os_srate);
        harm_post_r.update_shelf((double)p_harm_freq, hb, 0.707, os_srate);

        iron_voicing_l.update_shelf(100.0, 1.0, 0.707, s_rate);
        iron_voicing_r.update_shelf(100.0, 1.0, 0.707, s_rate);
        steel_low_l.update_shelf(40.0, 1.5, 0.707, s_rate);
        steel_low_r.update_shelf(40.0, 1.5, 0.707, s_rate);
        steel_high_l.update_lpf(9000.0, 0.707, s_rate);
        steel_high_r.update_lpf(9000.0, 0.707, s_rate);

        if (os_srate > 0.0) {
            steel_dt = 1.0 / os_srate;
            steel_dy_gain = os_srate;
            const double leak_hz = 6.0;
            steel_leak_coeff = std::exp(-2.0 * juce::MathConstants<double>::pi * leak_hz / os_srate);
        }

        sat_pre_lin_target = dbToLin((double)p_sat_pre_gain);
        sat_drive_lin_target = dbToLin((double)p_sat_drive);
        sat_mix_target = juce::jlimit(0.0, 1.0, (double)p_sat_mix / 100.0);
        sat_trim_lin = dbToLin((double)p_sat_trim);

        if (p_sat_mode != last_sat_mode) {
            steel_phi_l = steel_phi_r = 0.0;
            steel_prev_x_l = steel_prev_x_r = 0.0;
            sat_agc_gain_sm = 1.0;
            last_sat_mode = p_sat_mode;
        }

        if (p_ctrl_mode != last_ctrl_mode) {
            cf_peak_env = 0.0; cf_rms_sum = 0.0; cf_amt = 0.0; cf_ratio_mix = 0.0;
            last_ctrl_mode = p_ctrl_mode;
        }
    }

private:
    static inline double dbToLin(double db) { return std::pow(10.0, db / 20.0); }
    static inline double linToDb(double lin) { return 20.0 * std::log10(std::max(lin, 1.0e-20)); }
    static inline double smooth1p(double current, double target, double alpha) { return current + (target - current) * (1.0 - alpha); }

    inline double scTdProcessSample(double x, double& fastEnv, double& slowEnv, double amt) noexcept
    {
        const double ax = std::abs(x);

        const double cFast = (ax > fastEnv) ? sc_td_fast_att : sc_td_fast_rel;
        fastEnv = fastEnv * cFast + ax * (1.0 - cFast);

        const double cSlow = (ax > slowEnv) ? sc_td_slow_att : sc_td_slow_rel;
        slowEnv = slowEnv * cSlow + ax * (1.0 - cSlow);

        const double eps = 1.0e-12;
        double ratio = (fastEnv + eps) / (slowEnv + eps);
        ratio = juce::jlimit(0.25, 4.0, ratio);

        // amt is -1..1, depth scales aggression (detector-only, so we can be reasonably assertive)
        const double depth = 2.0;
        double g = std::exp(std::log(ratio) * (amt * depth));
        g = juce::jlimit(0.25, 4.0, g);

        return x * g;
    }

    inline void applySidechainTransientDesigner(double& s_l, double& s_r) noexcept
    {
        const double amt = juce::jlimit(-1.0, 1.0, sc_td_amt_sm);
        if (std::abs(amt) < 1.0e-9)
            return;

        const double blend = juce::jlimit(0.0, 1.0, sc_td_ms_sm);
        const double amtMid = amt * (1.0 - blend);
        const double amtSide = amt * blend;

        const double mid = (s_l + s_r) * 0.5;
        const double side = (s_l - s_r) * 0.5;

        const double midP = scTdProcessSample(mid, sc_td_fast_mid, sc_td_slow_mid, amtMid);
        const double sideP = scTdProcessSample(side, sc_td_fast_side, sc_td_slow_side, amtSide);

        s_l = midP + sideP;
        s_r = midP - sideP;
    }

    void processCompressorBlock(juce::AudioBuffer<float>& io)
    {
        const int nSamp = io.getNumSamples();
        float* l = io.getWritePointer(0);
        float* r = io.getWritePointer(1);
        float* sc_l = sc_internal_buf.getWritePointer(0);
        float* sc_r = sc_internal_buf.getWritePointer(1);

        // TRUE BYPASS: If the Dynamics module is bypassed, do not touch the program signal.
        // This guarantees null/bit-transparent behavior for "all modules bypassed" scenarios.
        if (!p_active_dyn)
        {
            det_env = 0.0; env = 0.0;
            env_l = env_r = 0.0;
            env_fast = env_slow = 0.0;
            env_fast_l = env_fast_r = 0.0;
            env_slow_l = env_slow_r = 0.0;
            fb_prev_l = fb_prev_r = 0.0;
            return;
        }

        const double thresh_target = (double)p_thresh;
        const double ratio_target = std::max(1.0, (double)p_ratio);
        const double knee_target = std::max(0.0, (double)p_knee);

        // Auto-Gain Measurement Accumulators
        double sum_in_rms = 0.0;
        double sum_out_rms = 0.0;

        for (int i = 0; i < nSamp; ++i)
        {
            thresh_sm = smooth1p(thresh_sm, thresh_target, smooth_alpha);
            ratio_sm = smooth1p(ratio_sm, ratio_target, smooth_alpha);
            knee_sm = smooth1p(knee_sm, knee_target, smooth_alpha);

            comp_in_sm = smooth1p(comp_in_sm, comp_in_target, smooth_alpha);
            makeup_lin_sm = smooth1p(makeup_lin_sm, makeup_lin_target, smooth_alpha);
            sc_level_sm = smooth1p(sc_level_sm, sc_level_target, smooth_alpha);
            sc_td_amt_sm = smooth1p(sc_td_amt_sm, sc_td_amt_target, smooth_alpha);
            sc_td_ms_sm = smooth1p(sc_td_ms_sm, sc_td_ms_target, smooth_alpha);
            sc_td_amt_sm = smooth1p(sc_td_amt_sm, sc_td_amt_target, smooth_alpha);
            sc_td_ms_sm = smooth1p(sc_td_ms_sm, sc_td_ms_target, smooth_alpha);
            ms_bal_sm = smooth1p(ms_bal_sm, ms_bal_target, smooth_alpha);
            // 1. Apply Input Gain (Drive)
            double in_gain = comp_in_sm;
            l[i] *= (float)in_gain;
            r[i] *= (float)in_gain;

            // RMS Input Measurement (Post-Input Gain, Pre-GR)
            if (p_comp_autogain_mode > 0) {
                sum_in_rms += (double)l[i] * (double)l[i] + (double)r[i] * (double)r[i];
            }

            // --- 2. SIDECHAIN CONDITIONING ---
            double s_l = (double)sc_l[i];
            double s_r = (double)sc_r[i];

            if (p_sc_input_mode == 0) {
                s_l *= in_gain;
                s_r *= in_gain;
            }

            if (!p_sc_to_comp) {
                s_l = (double)l[i];
                s_r = (double)r[i];
            }
            else {
                s_l *= sc_level_sm;
                s_r *= sc_level_sm;

                if (p_active_det) {
                    s_l = sc_hp_l_2.process(sc_hp_l.process(s_l));
                    s_r = sc_hp_r_2.process(sc_hp_r.process(s_r));
                    s_l = sc_lp_l_2.process(sc_lp_l.process(s_l));
                    s_r = sc_lp_r_2.process(sc_lp_r.process(s_r));
                    if (p_thrust_mode > 0) {
                        s_l = sc_shelf_l.process(s_l);
                        s_r = sc_shelf_r.process(s_r);
                    }
                }
            }

            // Sidechain transient designer (post filters)
            applySidechainTransientDesigner(s_l, s_r);

            // --- 3. DETECTOR ---
            double det_in_l = s_l;
            double det_in_r = s_r;

            if (p_ms_mode > 0) {
                double mid = (s_l + s_r) * 0.5;
                double side = (s_l - s_r) * 0.5;
                if (p_ms_mode == 1) { det_in_l = mid; det_in_r = mid; }
                else if (p_ms_mode == 2) { det_in_l = side; det_in_r = side; }
                else if (p_ms_mode == 3) { det_in_l = mid; det_in_r = mid; }
                else if (p_ms_mode == 4) { det_in_l = side; det_in_r = side; }
            }
            // FIXED: Feedback uses fb_prev stored BEFORE makeup gain
            det_in_l = det_in_l * (1.0 - fb_blend) + fb_prev_l * fb_blend;
            det_in_r = det_in_r * (1.0 - fb_blend) + fb_prev_r * fb_blend;
            runDetector(det_in_l, det_in_r);
// --- 4. APPLY GAIN REDUCTION ---
            const double lin_gain_l = std::pow(10.0, env_l / 20.0);
            const double lin_gain_r = std::pow(10.0, env_r / 20.0);
            const double lin_gain_mono = std::pow(10.0, env / 20.0);

            // Apply GR first (Pre-Makeup)
            double pre_make_l, pre_make_r;
            double in_l = (double)l[i];
            double in_r = (double)r[i];

            if (p_ms_mode == 0) {
                pre_make_l = in_l * lin_gain_l;
                pre_make_r = in_r * lin_gain_r;
            }
            else {
                double mid = (in_l + in_r) * 0.5;
                double side = (in_l - in_r) * 0.5;
                if (p_ms_mode == 1) mid *= lin_gain_mono;
                else if (p_ms_mode == 2) side *= lin_gain_mono;
                else if (p_ms_mode == 3) side *= lin_gain_mono;
                else if (p_ms_mode == 4) mid *= lin_gain_mono;

                // NEW: M/S balance tilt for cross-modes (keeps energy roughly consistent)
                if (p_ms_mode == 3) { mid *= (1.0 / ms_bal_sm); side *= ms_bal_sm; }
                else if (p_ms_mode == 4) { mid *= ms_bal_sm; side *= (1.0 / ms_bal_sm); }
                pre_make_l = mid + side;
                pre_make_r = mid - side;
            }

            // Store Feedback Tap (Pre-Makeup)
            fb_prev_l = pre_make_l;
            fb_prev_r = pre_make_r;

            // RMS Output Measurement (Pre-Makeup)
            if (p_comp_autogain_mode > 0) {
                sum_out_rms += pre_make_l * pre_make_l + pre_make_r * pre_make_r;
            }

            // 5. Apply Makeup & Auto-Gain
            const double final_agc = (double)comp_agc_gain_sm;
            const double mirror = (p_comp_mirror) ? (1.0 / std::max(1e-6, comp_in_sm)) : 1.0;
            l[i] = (float)(pre_make_l * makeup_lin_sm * final_agc * mirror);
            r[i] = (float)(pre_make_r * makeup_lin_sm * final_agc * mirror);
        }

        // --- COMPRESSOR AUTO-GAIN LOGIC (Block Level) ---
        if (p_comp_autogain_mode > 0 && sum_in_rms > 1e-12) {
            double rms_in = std::sqrt(sum_in_rms / (double)(nSamp * 2));
            double rms_out = std::sqrt(sum_out_rms / (double)(nSamp * 2)); // This is Post-GR, Pre-Makeup

            // Only update gain if signal is above noise floor (-60dB)
            if (rms_in > 0.001) {
                double g_req = rms_in / (rms_out + 1e-24);

                // Limit extreme corrections
                g_req = juce::jlimit(0.25, 4.0, g_req); // +/- 12dB max

                // Modes
                double strength = (p_comp_autogain_mode == 1) ? 0.5 : 1.0;
                double g_target = std::pow(g_req, strength);

                // Slow smoothing for stability (300ms)
                double agc_alpha = std::exp(-(double)nSamp / (0.300 * s_rate));
                comp_agc_gain_sm = comp_agc_gain_sm * agc_alpha + g_target * (1.0 - agc_alpha);
            }
        }
        else if (p_comp_autogain_mode == 0) {
            // Slowly release back to unity if disabled
            double agc_alpha = std::exp(-(double)nSamp / (0.100 * s_rate));
            comp_agc_gain_sm = comp_agc_gain_sm * agc_alpha + 1.0 * (1.0 - agc_alpha);
        }
    }



    void processAuditionBlock(juce::AudioBuffer<float>& buf)
    {
        const int nSamp = buf.getNumSamples();
        auto* l = buf.getWritePointer(0);
        auto* r = buf.getNumChannels() > 1 ? buf.getWritePointer(1) : nullptr;

        for (int i = 0; i < nSamp; ++i)
        {
            // Maintain the same basic gain smoothing behavior as the main path
            comp_in_sm = smooth1p(comp_in_sm, comp_in_target, smooth_alpha);
            sc_level_sm = smooth1p(sc_level_sm, sc_level_target, smooth_alpha);

            // --- ADD THESE TWO LINES ---
            sc_td_amt_sm = smooth1p(sc_td_amt_sm, sc_td_amt_target, smooth_alpha);
            sc_td_ms_sm = smooth1p(sc_td_ms_sm, sc_td_ms_target, smooth_alpha);
            // ---------------------------

            // Pull sidechain source (internal/external)
            double s_l = (double)sc_internal_buf.getSample(0, i);
            double s_r = (double)((sc_internal_buf.getNumChannels() > 1) ? sc_internal_buf.getSample(1, i) : sc_internal_buf.getSample(0, i));

            // If "Key to Comp" is disabled, detector hears the program input
            if (!p_sc_to_comp)
            {
                s_l = (double)l[i];
                s_r = (double)(r ? r[i] : l[i]);
            }
            else
            {
                // Apply SC level trim
                s_l *= sc_level_sm;
                s_r *= sc_level_sm;

                // Internal SC: respect input trim (so audition matches detector behavior)
                if (p_sc_input_mode == 0)
                {
                    s_l *= comp_in_sm;
                    s_r *= comp_in_sm;
                }

                if (p_active_det)
                {
                    // HPF/LPF - FIXED: Changed scHP_L to sc_hp_l etc to match class members
                    s_l = sc_hp_l_2.process(sc_hp_l.process(s_l));
                    s_r = sc_hp_r_2.process(sc_hp_r.process(s_r));
                    s_l = sc_lp_l_2.process(sc_lp_l.process(s_l));
                    s_r = sc_lp_r_2.process(sc_lp_r.process(s_r));

                    // Thrust voicing
                    if (p_thrust_mode != 0) {
                        s_l = sc_shelf_l.process(s_l);
                        s_r = sc_shelf_r.process(s_r);
                    }
                }

                // Sidechain transient designer (post filters)
                applySidechainTransientDesigner(s_l, s_r);

            }

            // M/S detector selection (affects what you hear in audition)
            double det_in_l = s_l;
            double det_in_r = s_r;

            if (p_ms_mode > 0)
            {
                const double mid = (s_l + s_r) * 0.5;
                const double side = (s_l - s_r) * 0.5;

                if (p_ms_mode == 1) { det_in_l = det_in_r = mid; }
                else if (p_ms_mode == 2) { det_in_l = det_in_r = side; }
                else if (p_ms_mode == 3) { det_in_l = det_in_r = mid; }   // M>S: detect Mid
                else if (p_ms_mode == 4) { det_in_l = det_in_r = side; }  // S>M: detect Side
            }

            l[i] = (float)det_in_l;
            if (r) r[i] = (float)det_in_r;
        }
    }


    void runDetector(double s_l, double s_r)
    {
        // Detector raw (pre-link)
        double det_l_raw = 0.0, det_r_raw = 0.0;

        if (use_rms) {
            const double pL = s_l * s_l;
            const double pR = s_r * s_r;

            rms_sum_l += pL - rms_ring_l[(size_t)rms_pos];
            rms_sum_r += pR - rms_ring_r[(size_t)rms_pos];

            rms_ring_l[(size_t)rms_pos] = pL;
            rms_ring_r[(size_t)rms_pos] = pR;

            rms_pos++; if (rms_pos >= rms_window) rms_pos = 0;

            det_l_raw = std::sqrt(std::max(0.0, rms_sum_l / (double)rms_window));
            det_r_raw = std::sqrt(std::max(0.0, rms_sum_r / (double)rms_window));
        }
        else {
            det_l_raw = std::abs(s_l);
            det_r_raw = std::abs(s_r);
        }

        const double det_avg = std::sqrt(0.5 * (det_l_raw * det_l_raw + det_r_raw * det_r_raw));
        const double det_max = std::max(det_l_raw, det_r_raw);

        // Global detector value for control-layer options (TP / Crest / Flux)
        const double det = det_max;

        double eff_thresh_db = thresh_sm;
        if (p_active_tf && tp_enabled)
        {
            const double pk = det_max;
            const double det_fast = (pk > det_env)
                ? (att_coeff * det_env + (1.0 - att_coeff) * pk)
                : (auto_rel_fast * det_env + (1.0 - auto_rel_fast) * pk);
            det_env = det_fast;

            const double tp_metric = juce::jlimit(0.0, 1.0, (linToDb(det_env + 1e-20) - linToDb(det_avg + 1e-20)) / 24.0);
            const double tp_boost = tp_metric * tp_amt * tp_raise_db;
            eff_thresh_db += tp_boost;
        }
        else {
            if (!p_active_tf) det_env = 0.0;
        }

        double eff_ratio = ratio_sm;
        if (p_active_tf && p_ctrl_mode == 1)
        {
            // Crest-factor thresh/ratio (optional)
            const double crest_coeff_local = crest_coeff;
            cf_peak_env = std::max(det_max, cf_peak_env * crest_coeff_local);
            const double rms_p = det_avg * det_avg;
            cf_rms_sum = smooth1p(cf_rms_sum, rms_p, crest_coeff_local);
            const double rms = std::sqrt(std::max(0.0, cf_rms_sum));
            const double crest = linToDb((cf_peak_env + 1e-20) / (rms + 1e-20));

            const double err = crest - crest_target_db;
            const double cf_step = (1.0 - crest_coeff_local) * 0.002;
            cf_amt = juce::jlimit(0.0, 1.0, cf_amt + err * cf_step);

            eff_ratio = ratio_sm * (1.0 + cf_amt * 2.0);
            eff_thresh_db -= cf_amt * 3.0;
        }
        else {
            cf_amt = 0.0;
        }

        if (p_active_tf && flux_enabled)
        {
            const double drive = sat_drive_lin_sm;
            const double meas_pk = det_max * drive;
            const double meas_db = linToDb(meas_pk + 1e-20);
            const double metric = juce::jlimit(0.0, 1.0, (meas_db - (-24.0)) / 24.0);
            flux_env = std::max(metric, flux_env * 0.995);
            eff_thresh_db += flux_env * (6.0 * flux_amt);
        }
        else {
            if (!p_active_tf) flux_env = 0.0;
        }

        auto compute_gr_db = [&](double det_db) -> double
            {
                const double knee = knee_sm;
                double gr_db = 0.0;

                if (knee > 0.0) {
                    const double x = det_db - eff_thresh_db;
                    const double half = knee * 0.5;
                    if (x <= -half) gr_db = 0.0;
                    else if (x >= half) gr_db = -(x - x / eff_ratio);
                    else {
                        const double t = (x + half) / knee; // 0..1
                        const double y = t * t * (3.0 - 2.0 * t); // smoothstep
                        const double x2 = x - (-half);
                        const double gr_full = -(x2 - x2 / eff_ratio);
                        gr_db = gr_full * y;
                    }
                }
                else {
                    const double x = det_db - eff_thresh_db;
                    gr_db = (x > 0.0) ? -(x - x / eff_ratio) : 0.0;
                }

                return gr_db;
            };

        // --- Stereo link: 0% = dual-mono, 100% = fully linked.
        if (p_ms_mode == 0)
        {
            const double link = stereo_link; // 0..1

            const double det_db_l = linToDb(det_l_raw + 1e-20);
            const double det_db_r = linToDb(det_r_raw + 1e-20);
            const double det_db_link = linToDb(det_max + 1e-20);

            const double gr_l_un = compute_gr_db(det_db_l);
            const double gr_r_un = compute_gr_db(det_db_r);
            const double gr_link = compute_gr_db(det_db_link);

            const double target_l = gr_l_un + (gr_link - gr_l_un) * link;
            const double target_r = gr_r_un + (gr_link - gr_r_un) * link;

            // Smooth per channel (attack / release / auto-release)
            auto updateEnv = [&](double target, double& envC, double& fastC, double& slowC)
                {
                    if (target < envC) {
                        envC = att_coeff * envC + (1.0 - att_coeff) * target;
                        fastC = envC;
                        slowC = envC;
                        rel_coeff = att_coeff;
                    }
                    else {
                        if (p_auto_rel) {
                            fastC = auto_rel_fast * fastC + (1.0 - auto_rel_fast) * target;
                            slowC = auto_rel_slow * slowC + (1.0 - auto_rel_slow) * target;
                            envC = std::min(fastC, slowC);
                        }
                        else {
                            envC = rel_coeff_manual * envC + (1.0 - rel_coeff_manual) * target;
                            fastC = envC;
                            slowC = envC;
                            rel_coeff = rel_coeff_manual;
                        }
                    }
                };

            updateEnv(target_l, env_l, env_fast_l, env_slow_l);
            updateEnv(target_r, env_r, env_fast_r, env_slow_r);

            env = 0.5 * (env_l + env_r);
            env_fast = 0.5 * (env_fast_l + env_fast_r);
            env_slow = 0.5 * (env_slow_l + env_slow_r);
        }
        else
        {
            // M/S modes are single-detector (by design), since you are explicitly compressing mid or side.
            const double det_db = linToDb(det + 1e-20);
            const double gr_db = compute_gr_db(det_db);

            const double target = gr_db;
            if (target < env) {
                env = att_coeff * env + (1.0 - att_coeff) * target;
                env_fast = env;
                env_slow = env;
                rel_coeff = att_coeff;
            }
            else {
                if (p_auto_rel) {
                    env_fast = auto_rel_fast * env_fast + (1.0 - auto_rel_fast) * target;
                    env_slow = auto_rel_slow * env_slow + (1.0 - auto_rel_slow) * target;
                    env = std::min(env_fast, env_slow);
                }
                else {
                    env = rel_coeff_manual * env + (1.0 - rel_coeff_manual) * target;
                    env_fast = env;
                    env_slow = env;
                    rel_coeff = rel_coeff_manual;
                }
            }
        }
    }


    void processSaturationBlock(juce::AudioBuffer<float>& io)
    {
        if (!p_active_sat && !p_active_eq) return;

        const int nCh = io.getNumChannels();
        const int nS  = io.getNumSamples();

        // ----------------------------------------------------------------------
        // EQ-only path: when Saturation is bypassed but Color EQ is active,
        // we process at the native sample rate (NO oversampling).
        // This avoids the "phasey/top-end" delta residue caused by the OS up/down filters
        // when the nonlinearity is not in use.
        // ----------------------------------------------------------------------
        if (!p_active_sat && p_active_eq)
        {
            // Snapshot DRY (for the section's Mix blend)
            sat_clean_buf.setSize(nCh, nS, false, false, true);
            sat_proc_buf.setSize(nCh, nS, false, false, true);

            for (int ch = 0; ch < nCh; ++ch)
            {
                sat_clean_buf.copyFrom(ch, 0, io, ch, 0, nS);
                sat_proc_buf.copyFrom(ch, 0, io, ch, 0, nS);
            }

            const bool eq_tone_active  = (std::abs(p_sat_tone) > 0.01f);
            const bool eq_girth_active = (std::abs(p_girth) > 0.01f);

            for (int ch = 0; ch < nCh; ++ch)
            {
                float* y = sat_proc_buf.getWritePointer(ch);
                auto& tone  = (ch == 0) ? sat_tone_l     : sat_tone_r;
                auto& gBump = (ch == 0) ? girth_bump_l   : girth_bump_r;
                auto& gDip  = (ch == 0) ? girth_dip_l    : girth_dip_r;

                for (int i = 0; i < nS; ++i)
                {
                    double s = (double)y[i];
                    if (eq_girth_active) { s = gBump.process(s); s = gDip.process(s); }
                    if (eq_tone_active)  { s = tone.process(s); }
                    y[i] = (float)s;
                }
            }

            // Smooth mix to avoid zipper noise during automation.
            sat_mix_sm = smooth1p(sat_mix_sm, sat_mix_target, smooth_alpha_block);
            const float satMix01 = (float)juce::jlimit(0.0, 1.0, sat_mix_sm);

            // Blend processed EQ against the dry snapshot (preserves existing behavior of Sat Mix).
            for (int ch = 0; ch < nCh; ++ch)
            {
                float* out = io.getWritePointer(ch);
                const float* wet = sat_proc_buf.getReadPointer(ch);
                const float* dry = sat_clean_buf.getReadPointer(ch);

                for (int i = 0; i < nS; ++i)
                    out[i] = dry[i] + (wet[i] - dry[i]) * satMix01;
            }

            return;
        }

        if (!os) return;

// 1. Snapshot DRY (Source for AutoGain)
        sat_clean_buf.setSize(nCh, nS, false, false, true);
        for (int ch = 0; ch < nCh; ++ch)
            sat_clean_buf.copyFrom(ch, 0, io, ch, 0, nS);

        // Align Dry buffer with Oversampling latency
        if (p_active_sat) {
            float latency = os->getLatencyInSamples();
            satInternalDelay.setDelay(latency);
            juce::dsp::AudioBlock<float> block(sat_clean_buf);
            juce::dsp::ProcessContextReplacing<float> context(block);
            satInternalDelay.process(context);
        }

        sat_proc_buf.setSize(nCh, nS, false, false, true);
        for (int ch = 0; ch < nCh; ++ch)
            sat_proc_buf.copyFrom(ch, 0, io, ch, 0, nS);

        sat_pre_lin_sm = smooth1p(sat_pre_lin_sm, sat_pre_lin_target, smooth_alpha_block);
        sat_drive_lin_sm = smooth1p(sat_drive_lin_sm, sat_drive_lin_target, smooth_alpha_block);

        const float pre_gain = (float)sat_pre_lin_sm;
        if (p_active_sat) {
            for (int ch = 0; ch < nCh; ++ch) {
                float* x = sat_proc_buf.getWritePointer(ch);
                for (int i = 0; i < nS; ++i) x[i] *= pre_gain;
            }
        }

        // REMOVED: SC Filters for Sat (p_sc_to_sat functionality)

        // --- OVERSAMPLED PROCESSING ---
        auto block = juce::dsp::AudioBlock<float>(sat_proc_buf);
        auto osBlock = os->processSamplesUp(block);
        const int mode = p_sat_mode;
        const double drive = (double)sat_drive_lin_sm;
        const int osN = (int)osBlock.getNumSamples();
        const bool eq_tone_active = p_active_eq && (std::abs(p_sat_tone) > 0.01f);
        const bool eq_bright_active = p_active_eq && (std::abs(p_harm_bright) > 0.01f);
        const bool eq_girth_active = p_active_eq && (std::abs(p_girth) > 0.01f);

        for (int ch = 0; ch < nCh; ++ch)
        {
            float* data = osBlock.getChannelPointer(ch);
            auto& pre = (ch == 0) ? harm_pre_l : harm_pre_r;
            auto& post = (ch == 0) ? harm_post_l : harm_post_r;
            double& phi = (ch == 0) ? steel_phi_l : steel_phi_r;
            double& yPrev = (ch == 0) ? steel_prev_x_l : steel_prev_x_r;

            for (int i = 0; i < osN; ++i)
            {
                double s = (double)data[i];
                if (eq_bright_active) s = pre.process(s);

                if (p_active_sat) {
                    s *= drive;
                    if (mode == 1) { // Iron
                        const double bias = 0.075;
                        const double y0 = std::tanh(bias);
                        const double y = std::tanh(s + bias) - y0;
                        const double s_sat = std::tanh(s);
                        const double poly = s_sat - (s_sat * s_sat * s_sat) * 0.333333333333;
                        s = 0.82 * y + 0.18 * poly;
                    }
                    else if (mode == 2) { // Steel
                        phi = phi * steel_leak_coeff + s * steel_dt;
                        const double y = std::tanh(phi * 7.0);
                        double dy = (y - yPrev) * steel_dy_gain;
                        yPrev = y;
                        dy = std::tanh(dy * 0.85);
                        const double base = std::tanh(s * 1.05);
                        s = 0.65 * dy + 0.35 * base;
                    }
                }

                if (eq_bright_active) s = post.process(s);
                data[i] = (float)s;
            }
        }
        os->processSamplesDown(block);

        // --- PREPARE FOR AUTOGAIN MEASUREMENT (APPLY MIRROR FIRST) ---
        const double mirror_comp = (p_sat_mirror) ? (1.0 / std::max(1e-6, (double)pre_gain)) : 1.0;

        // We need to apply Mirror comp to the buffer BEFORE measuring Output power
        // so that the AutoGain sees the 'final' level relative to input.
        if (p_active_sat && p_sat_mirror) {
            for (int ch = 0; ch < nCh; ++ch) {
                float* y = sat_proc_buf.getWritePointer(ch);
                for (int i = 0; i < nS; ++i) y[i] *= (float)mirror_comp;
            }
        }

        // --- POST-PROCESSING (Voicing + Color EQ; pre-trim) ---
        const bool sat_agc_active = (p_active_sat && (p_sat_autogain_mode != 0));
        double outPow_post = 0.0;

        for (int ch = 0; ch < nCh; ++ch)
        {
            float* y = sat_proc_buf.getWritePointer(ch);
            auto& ironV = (ch == 0) ? iron_voicing_l : iron_voicing_r;
            auto& stLo = (ch == 0) ? steel_low_l : steel_low_r;
            auto& stHi = (ch == 0) ? steel_high_l : steel_high_r;
            auto& tone = (ch == 0) ? sat_tone_l : sat_tone_r;
            auto& gBump = (ch == 0) ? girth_bump_l : girth_bump_r;
            auto& gDip = (ch == 0) ? girth_dip_l : girth_dip_r;

            for (int i = 0; i < nS; ++i)
            {
                double s = (double)y[i];

                // Transformer voicing
                if (p_active_sat && (mode == 1 || mode == 2)) {
                    if (mode == 1) s = ironV.process(s);
                    else { s = stLo.process(s); s = stHi.process(s); }
                }

                // Color EQ
                if (eq_girth_active) { s = gBump.process(s); s = gDip.process(s); }
                if (eq_tone_active)  s = tone.process(s);

                y[i] = (float)s;

                if (sat_agc_active) outPow_post += s * s;
            }
        }

        // --- SATURATION AUTO-GAIN (Measured post-voicing/EQ, pre-trim) ---
        {
            const double alpha = std::exp(-(double)nS / (0.300 * s_rate)); // ~300ms

            if (sat_agc_active)
            {
                // Measure Clean (Delayed)
                double inPow = 0.0;
                for (int ch = 0; ch < nCh; ++ch) {
                    const float* x = sat_clean_buf.getReadPointer(ch);
                    for (int i = 0; i < nS; ++i) inPow += (double)x[i] * (double)x[i];
                }

                if (inPow > 1e-20 && outPow_post > 1e-20) {
                    double g = std::sqrt(inPow / outPow_post);
                    g = juce::jlimit(0.125, 8.0, g); // +/- 18dB limit
                    const double exponent = (p_sat_autogain_mode == 1) ? 0.5 : 1.0;
                    const double gTarget = std::pow(g, exponent);
                    sat_agc_gain_sm = sat_agc_gain_sm * alpha + gTarget * (1.0 - alpha);

                    const float gSm = (float)sat_agc_gain_sm;
                    for (int ch = 0; ch < nCh; ++ch) {
                        float* y = sat_proc_buf.getWritePointer(ch);
                        for (int i = 0; i < nS; ++i) y[i] *= gSm;
                    }
                }
            }
            else
            {
                // Release AGC if disabled
                sat_agc_gain_sm = sat_agc_gain_sm * alpha + 1.0 * (1.0 - alpha);
            }
        }

        // --- SAT TRIM (applied after AGC so Trim remains a real output control) ---
        sat_trim_lin_sm = smooth1p(sat_trim_lin_sm, sat_trim_lin, smooth_alpha_block);
        const double trim = (double)sat_trim_lin_sm;

        if (p_active_sat)
        {
            for (int ch = 0; ch < nCh; ++ch) {
                float* y = sat_proc_buf.getWritePointer(ch);
                for (int i = 0; i < nS; ++i) y[i] *= (float)trim;
            }
        }

        // Smooth mix to avoid zipper noise during automation.
        sat_mix_sm = smooth1p(sat_mix_sm, sat_mix_target, smooth_alpha_block);
        const float satMix01 = (float)juce::jlimit(0.0, 1.0, sat_mix_sm);

        // Apply block back to io if either Saturation or EQ is active.
        if (p_active_sat || p_active_eq)
        {
            for (int ch = 0; ch < nCh; ++ch) {
                float* finalOut = io.getWritePointer(ch);
                const float* wet = sat_proc_buf.getReadPointer(ch);
                const float* dry = sat_clean_buf.getReadPointer(ch);
                for (int i = 0; i < nS; ++i) {
                    finalOut[i] = dry[i] + (wet[i] - dry[i]) * satMix01;
                }
            }
        }
    }

    double s_rate = 44100.0;
    int max_block = 512;
    int os_latency_samples = 0; // Oversampling latency (samples)

    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    std::unique_ptr<juce::dsp::Oversampling<float>> os_dry;

    int os_stages = 2;
    int os_factor = 4;
    double os_srate = 176400.0;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Thiran> satInternalDelay{ 8192 };
// Filters (Comp)
    SimpleBiquad sc_hp_l, sc_hp_r, sc_hp_l_2, sc_hp_r_2;
    SimpleBiquad sc_lp_l, sc_lp_r, sc_lp_l_2, sc_lp_r_2;
    // REMOVED: Filters (Sat)

    SimpleBiquad sc_shelf_l, sc_shelf_r;
    SimpleBiquad sat_tone_l, sat_tone_r;
    SimpleBiquad girth_bump_l, girth_bump_r, girth_dip_l, girth_dip_r;
    SimpleBiquad harm_pre_l, harm_pre_r, harm_post_l, harm_post_r;
    SimpleBiquad iron_voicing_l, iron_voicing_r, steel_low_l, steel_low_r, steel_high_l, steel_high_r;

    double fb_prev_l = 0.0, fb_prev_r = 0.0;
    double det_env = 0.0, env = 0.0;
    double env_l = 0.0, env_r = 0.0;
    double env_fast = 0.0, env_slow = 0.0;
    double env_fast_l = 0.0, env_fast_r = 0.0;
    double env_slow_l = 0.0, env_slow_r = 0.0;
    double att_coeff = 0.999, rel_coeff_manual = 0.999, rel_coeff = 0.999, auto_rel_slow = 0.999, auto_rel_fast = 0.90;

    bool use_rms = false;
    int rms_window = 1;
    int rms_window_max = 1;
    std::vector<double> rms_ring_l;
    std::vector<double> rms_ring_r;
    int rms_pos = 0;
    double rms_sum_l = 0.0, rms_sum_r = 0.0;
    double stereo_link = 1.0;
    double fb_blend = 0.0;

    double crest_target_db = 12.0;
    double crest_speed_ms = 400.0;
    double crest_coeff = 0.999;
    double cf_peak_env = 0.0;
    double cf_rms_sum = 0.0;
    double cf_amt = 0.0;
    double cf_ratio_mix = 0.0;

    bool tp_enabled = false;
    double tp_amt = 0.5;
    double tp_raise_db = 12.0;
    bool flux_enabled = false;
    double flux_amt = 0.3;
    double flux_env = 0.0;

    double steel_phi_l = 0.0, steel_phi_r = 0.0;
    double steel_prev_x_l = 0.0, steel_prev_x_r = 0.0;
    double steel_dt = 0.0, steel_dy_gain = 1.0, steel_leak_coeff = 1.0;

    // Gain States
    double sat_agc_gain_sm = 1.0;
    double comp_agc_gain_sm = 1.0;

    double thrust_gain_db = 0.0;

    double makeup_lin_target = 1.0, makeup_lin_sm = 1.0;
    double comp_in_target = 1.0, comp_in_sm = 1.0;
    double sc_level_target = 1.0, sc_level_sm = 1.0;
    double ms_bal_target = 1.0, ms_bal_sm = 1.0;


    // Sidechain transient designer (detector conditioning)
    double sc_td_amt_target = 0.0, sc_td_amt_sm = 0.0;
    double sc_td_ms_target = 0.0, sc_td_ms_sm = 0.0;

    double sc_td_fast_att = 0.999, sc_td_fast_rel = 0.999;
    double sc_td_slow_att = 0.999, sc_td_slow_rel = 0.999;

    double sc_td_fast_mid = 0.0, sc_td_slow_mid = 0.0;
    double sc_td_fast_side = 0.0, sc_td_slow_side = 0.0;

    double out_lin_target = 1.0, out_lin_sm = 1.0;
    double sat_pre_lin_target = 1.0, sat_pre_lin_sm = 1.0;
    double sat_drive_lin_target = 1.0, sat_drive_lin_sm = 1.0;
    double sat_trim_lin = 1.0, sat_trim_lin_sm = 1.0;
    double sat_mix_target = 1.0, sat_mix_sm = 1.0;
    double drywet_sm = 1.0;

    double smooth_alpha = 0.999, smooth_alpha_block = 0.999, smooth_alpha_os = 0.999;
    double thresh_sm = -20.0, ratio_sm = 4.0, knee_sm = 6.0;

    int last_sat_mode = -1;
    int last_ctrl_mode = -1;


    // Topology-change click smoothing (fade the "wet contribution" back in over a short ramp)
    double topologyRamp = 1.0;
    double topologyInc = 0.0;

    bool prevTopoSatEq = false;
    int  prevTopoFlow = 0;
    bool prevTopoAudition = false;
    int  prevTopoMsMode = 0;
    int  prevTopoScMode = 0;
    bool prevTopoScToComp = false;

    juce::AudioBuffer<float> dry_buf, wet_buf, sc_internal_buf;
    juce::AudioBuffer<float> sat_clean_buf, sat_proc_buf;
};