/*
  ==============================================================================
    UltimateCompDSP.h
    Port of Ultimate Mix Bus Compressor v3.10 (Auto-Gain Tuned & Bi-Polar Out)
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

    // --- DYNAMICS ---
    float p_thresh = -20.0f;
    float p_ratio = 4.0f;
    float p_knee = 6.0f;
    float p_att_ms = 10.0f;
    float p_rel_ms = 100.0f;
    int   p_auto_rel = 0;

    // NEW: Split Turbo
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
    float p_fb_blend = 0.0f;

    // --- TRANSIENT PRIORITY ---
    int   p_tp_mode = 0;
    float p_tp_amount = 50.0f;
    float p_tp_thresh_raise = 12.0f;

    // --- FLUX COUPLED ---
    int   p_flux_mode = 0;
    float p_flux_amount = 30.0f;

    // --- SATURATION ---
    int   p_sat_mode = 0;

    // NEW: Pre-Gain and Mirror
    float p_sat_pre_gain = 0.0f;
    bool  p_sat_mirror = false;

    float p_sat_drive = 0.0f; // Internal drive (extra character)
    float p_sat_trim = 0.0f;
    float p_sat_tone = 0.0f;
    float p_sat_tone_freq = 5500.0f;
    float p_sat_mix = 100.0f;
    int   p_sat_autogain_mode = 1;

    // --- HARMONIC BRIGHTNESS ---
    float p_harm_bright = 0.0f;
    float p_harm_freq = 4500.0f;

    // --- OUTPUT ---
    float p_makeup = 0.0f; // Now Bi-Polar (-24 to +24)
    bool  p_auto_makeup = false;
    float p_dry_wet = 100.0f;
    float p_out_trim = 0.0f;

    // ==============================================================================
    // GETTERS
    // ==============================================================================
    float getGainReductiondB() const { return (float)env; }
    float getFluxSaturation() const { return (float)flux_env; }
    float getCrestAmt() const { return (float)cf_amt; }

    double getLatency() const
    {
        if ((p_active_sat || p_active_eq) && os)
            return os->getLatencyInSamples();
        return 0.0;
    }

    // ==============================================================================
    // LIFECYCLE
    // ==============================================================================

    void prepare(double sampleRate, int maxBlockSamples)
    {
        s_rate = (sampleRate > 1.0 ? sampleRate : 44100.0);
        max_block = std::max(1, maxBlockSamples);

        dry_buf.setSize(2, max_block, false, false, true);
        wet_buf.setSize(2, max_block, false, false, true);
        sat_clean_buf.setSize(2, max_block, false, false, true);

        // Oversampling (Wet Path)
        os_stages = 2;
        os_factor = 1 << os_stages;
        os = std::make_unique<juce::dsp::Oversampling<float>>(
            2, os_stages, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        os->initProcessing((size_t)max_block);

        // Oversampling (Dry Path - Twin Path Strategy)
        os_dry = std::make_unique<juce::dsp::Oversampling<float>>(
            2, os_stages, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        os_dry->initProcessing((size_t)max_block);

        // Internal Delay for Sat Mix
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = s_rate;
        spec.maximumBlockSize = (juce::uint32)max_block;
        spec.numChannels = 2;

        satInternalDelay.prepare(spec);
        satInternalDelay.reset();
        satInternalDelay.setMaximumDelayInSamples(4096);

        resetState();
        updateParameters();
    }

    void resetState()
    {
        sc_hp_l.reset(); sc_hp_r.reset();
        sc_shelf_l.reset(); sc_shelf_r.reset();
        sat_tone_l.reset(); sat_tone_r.reset();
        harm_pre_l.reset(); harm_pre_r.reset();
        harm_post_l.reset(); harm_post_r.reset();
        iron_voicing_l.reset(); iron_voicing_r.reset();
        steel_low_l.reset(); steel_low_r.reset();
        steel_high_l.reset(); steel_high_r.reset();

        if (os) os->reset();
        if (os_dry) os_dry->reset();
        satInternalDelay.reset();

        steel_phi_l = steel_phi_r = 0.0;
        steel_prev_x_l = steel_prev_x_r = 0.0;
        sat_agc_gain_sm = 1.0;

        fb_prev_l = fb_prev_r = 0.0;
        det_env = 0.0; env = 0.0; env_fast = 0.0; env_slow = 0.0;

        cf_peak_env = 0.0; cf_rms_sum = 0.0; cf_amt = 0.0; cf_ratio_mix = 0.0;
        flux_env = 0.0;

        thresh_sm = p_thresh;
        ratio_sm = std::max(1.0, (double)p_ratio);
        knee_sm = std::max(0.0, (double)p_knee);
        makeup_lin_sm = dbToLin((double)p_makeup);

        sat_pre_lin_sm = dbToLin((double)p_sat_pre_gain);
        sat_drive_lin_sm = dbToLin((double)p_sat_drive);
        sat_trim_lin_sm = dbToLin((double)p_sat_trim);
        sat_mix_sm = juce::jlimit(0.0, 1.0, (double)p_sat_mix / 100.0);

        last_sat_mode = -1;
        last_ctrl_mode = -1;
    }

    // ==============================================================================
    // MAIN PROCESS
    // ==============================================================================

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int nSamp = buffer.getNumSamples();
        if (nSamp <= 0) return;

        // 1. Prepare Dry Buffer
        dry_buf.setSize(2, nSamp, false, false, true);
        if (buffer.getNumChannels() == 1) {
            dry_buf.copyFrom(0, 0, buffer, 0, 0, nSamp);
            dry_buf.copyFrom(1, 0, buffer, 0, 0, nSamp);
        }
        else {
            dry_buf.makeCopyOf(buffer, true);
        }

        // 2. Prepare Wet Buffer
        wet_buf.makeCopyOf(dry_buf, true);

        // Smoothing
        smooth_alpha_block = std::exp(-(double)nSamp / (0.020 * s_rate));

        // 3. Process Wet Path
        if (p_signal_flow == 1) {
            processSaturationBlock(wet_buf);
            processCompressorBlock(wet_buf);
        }
        else {
            processCompressorBlock(wet_buf);
            processSaturationBlock(wet_buf);
        }

        // 4. TWIN-PATH OVERSAMPLING FOR DRY SIGNAL
        if ((p_active_sat || p_active_eq) && os && os_dry)
        {
            juce::dsp::AudioBlock<float> dryBlock(dry_buf);
            os_dry->processSamplesUp(dryBlock);
            os_dry->processSamplesDown(dryBlock);
        }

        // 5. Final Mix
        const double dw_target = juce::jlimit(0.0, 1.0, (double)p_dry_wet / 100.0);
        drywet_sm = smooth1p(drywet_sm, dw_target, smooth_alpha_block);

        // Calculate Output Gain Logic
        // Base output trim
        double total_out_db = (double)p_out_trim;

        // MIRROR LOGIC: If Mirror ON, subtract Pre-Gain from output
        if (p_sat_mirror && p_active_sat) {
            total_out_db -= (double)p_sat_pre_gain;
        }

        double final_gain_target = dbToLin(total_out_db);
        out_lin_sm = smooth1p(out_lin_sm, final_gain_target, smooth_alpha_block);

        const float finalGain = (float)out_lin_sm;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            float* out = buffer.getWritePointer(ch);
            const float* wet = wet_buf.getReadPointer(ch);
            const float* dry = dry_buf.getReadPointer(ch);
            const float wm = (float)drywet_sm;
            const float dm = 1.0f - wm;

            for (int i = 0; i < nSamp; ++i)
            {
                out[i] = (wet[i] * wm + dry[i] * dm) * finalGain;
            }
        }
    }

    void updateParameters()
    {
        // NEW: Separate Turbo Logic
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
            const int n = std::max(1, (int)std::round((win_ms * 0.001) * s_rate));
            rms_window = n;
            if ((int)rms_ring.size() != rms_window) {
                rms_ring.assign((size_t)rms_window, 0.0);
                rms_pos = 0; rms_sum = 0.0;
            }
        }

        stereo_link = juce::jlimit(0.0, 1.0, (double)p_stereo_link / 100.0);
        fb_blend = juce::jlimit(0.0, 1.0, (double)p_fb_blend / 100.0);

        sc_hp_l.update_hpf((double)p_sc_hp_freq, 0.707, s_rate);
        sc_hp_r.update_hpf((double)p_sc_hp_freq, 0.707, s_rate);

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

        // REFINED: Auto Makeup Gain Calculation
        // Adjusted heuristic to be less aggressive when GR hits.
        // We use a milder slope constant (0.3) instead of 0.5.
        // Also added a clamp to prevent runaway gain at extreme settings.
        double auto_gain_lin = 1.0;
        if (p_auto_makeup) {
            double r_eff = std::max(1.0, (double)p_ratio);
            double slope = 1.0 - (1.0 / r_eff);
            double gain_est_db = std::abs((double)p_thresh) * slope * 0.3; // Scaled down to 0.3 for musical behavior
            gain_est_db = std::min(gain_est_db, 20.0); // Hard clamp at +20dB for safety
            auto_gain_lin = std::pow(10.0, gain_est_db / 20.0);
        }

        // Apply Manual Output + Auto Gain
        makeup_lin_target = dbToLin((double)p_makeup) * auto_gain_lin;

        // Output trim target handled in process loop for mirror logic

        smooth_alpha = std::exp(-1.0 / (0.020 * s_rate));

        os_srate = s_rate * (double)os_factor;
        smooth_alpha_os = std::exp(-1.0 / (0.020 * os_srate));

        sat_tone_l.update_shelf((double)p_sat_tone_freq, (double)p_sat_tone, 0.707, s_rate);
        sat_tone_r.update_shelf((double)p_sat_tone_freq, (double)p_sat_tone, 0.707, s_rate);

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

    void processCompressorBlock(juce::AudioBuffer<float>& io)
    {
        const int nSamp = io.getNumSamples();
        float* l = io.getWritePointer(0);
        float* r = io.getWritePointer(1);

        const double thresh_target = (double)p_thresh;
        const double ratio_target = std::max(1.0, (double)p_ratio);
        const double knee_target = std::max(0.0, (double)p_knee);

        for (int i = 0; i < nSamp; ++i)
        {
            thresh_sm = smooth1p(thresh_sm, thresh_target, smooth_alpha);
            ratio_sm = smooth1p(ratio_sm, ratio_target, smooth_alpha);
            knee_sm = smooth1p(knee_sm, knee_target, smooth_alpha);
            makeup_lin_sm = smooth1p(makeup_lin_sm, makeup_lin_target, smooth_alpha);

            const double in_l = (double)l[i];
            const double in_r = (double)r[i];

            if (!p_active_dyn)
            {
                fb_prev_l = in_l; fb_prev_r = in_r;
                env = 0.0;
                continue;
            }

            const double sc_l = in_l * (1.0 - fb_blend) + fb_prev_l * fb_blend;
            const double sc_r = in_r * (1.0 - fb_blend) + fb_prev_r * fb_blend;

            runDetector(sc_l, sc_r);

            const double lin_gain = std::pow(10.0, env / 20.0);
            const double vca_l = in_l * lin_gain;
            const double vca_r = in_r * lin_gain;

            fb_prev_l = vca_l; fb_prev_r = vca_r;

            l[i] = (float)(vca_l * makeup_lin_sm);
            r[i] = (float)(vca_r * makeup_lin_sm);
        }
    }

    void runDetector(double sc_in_l, double sc_in_r)
    {
        double s_l = sc_in_l;
        double s_r = sc_in_r;

        if (p_active_det)
        {
            s_l = sc_hp_l.process(s_l);
            s_r = sc_hp_r.process(s_r);
            if (p_thrust_mode > 0) {
                s_l = sc_shelf_l.process(s_l);
                s_r = sc_shelf_r.process(s_r);
            }
        }

        double det_l_raw = 0.0, det_r_raw = 0.0;
        if (use_rms) {
            const double pL = s_l * s_l;
            const double pR = s_r * s_r;
            const double pAvg = 0.5 * (pL + pR);
            rms_sum += pAvg - rms_ring[(size_t)rms_pos];
            rms_ring[(size_t)rms_pos] = pAvg;
            rms_pos++; if (rms_pos >= rms_window) rms_pos = 0;
            const double rms = std::sqrt(std::max(0.0, rms_sum / (double)rms_window));
            det_l_raw = rms; det_r_raw = rms;
        }
        else {
            det_l_raw = std::abs(s_l);
            det_r_raw = std::abs(s_r);
        }

        const double det_avg = std::sqrt(0.5 * (det_l_raw * det_l_raw + det_r_raw * det_r_raw));
        const double det_max = std::max(det_l_raw, det_r_raw);
        const double det = det_avg * (1.0 - stereo_link) + det_max * stereo_link;

        // TRANSIENT
        double eff_thresh_db = thresh_sm;
        if (p_active_tf && tp_enabled)
        {
            const double pk = det_max;
            const double det_fast = (pk > det_env)
                ? (att_coeff * det_env + (1.0 - att_coeff) * pk)
                : (auto_rel_fast * det_env + (1.0 - auto_rel_fast) * pk);
            det_env = det_fast;
            const double fast_db = linToDb(det_fast + 1e-20);
            const double avg_db = linToDb(det_avg + 1e-20);
            const double metric = juce::jlimit(0.0, 1.0, (fast_db - avg_db) / 12.0);
            eff_thresh_db += metric * tp_raise_db * tp_amt;
        }
        else {
            det_env = det;
        }

        // CREST
        double eff_ratio = ratio_sm;
        if (p_active_crest && p_ctrl_mode != 0)
        {
            const double pk = det_max;
            if (pk > cf_peak_env) cf_peak_env = pk;
            else cf_peak_env = smooth1p(cf_peak_env, pk, crest_coeff);

            const double p = det_avg * det_avg;
            cf_rms_sum = smooth1p(cf_rms_sum, p, crest_coeff);
            const double rms = std::sqrt(std::max(0.0, cf_rms_sum));

            const double crest = linToDb((cf_peak_env + 1e-20) / (rms + 1e-20));
            const double err = crest - crest_target_db;
            const double cf_step = (1.0 - crest_coeff) * 0.002;
            cf_amt = juce::jlimit(0.0, 1.0, cf_amt + err * cf_step);

            eff_ratio = ratio_sm * (1.0 + cf_amt * 2.0);
            eff_thresh_db -= cf_amt * 3.0;
        }
        else {
            cf_amt = 0.0;
        }

        // FLUX
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

        // CURVE
        const double det_db = linToDb(det + 1e-20);
        const double knee = knee_sm;
        double gr_db = 0.0;

        if (knee > 0.0) {
            const double x = det_db - eff_thresh_db;
            const double half = knee * 0.5;
            if (x <= -half) gr_db = 0.0;
            else if (x >= half) gr_db = -(x - x / eff_ratio);
            else {
                const double t = (x + half) / (knee + 1e-20);
                const double y = t * t * (3.0 - 2.0 * t);
                const double full = -(x - x / eff_ratio);
                gr_db = full * y;
            }
        }
        else {
            const double x = det_db - eff_thresh_db;
            if (x > 0.0) gr_db = -(x - x / eff_ratio);
        }

        const double target = gr_db;
        if (target < env) {
            env = att_coeff * env + (1.0 - att_coeff) * target;
            env_fast = env_slow = env;
            rel_coeff = att_coeff;
        }
        else {
            if (p_auto_rel != 0) {
                env_fast = auto_rel_fast * env_fast + (1.0 - auto_rel_fast) * target;
                env_slow = auto_rel_slow * env_slow + (1.0 - auto_rel_slow) * target;
                env = std::min(env_fast, env_slow);
                rel_coeff = auto_rel_slow;
            }
            else {
                env = rel_coeff_manual * env + (1.0 - rel_coeff_manual) * target;
                env_fast = env_slow = env;
                rel_coeff = rel_coeff_manual;
            }
        }
    }

    void processSaturationBlock(juce::AudioBuffer<float>& io)
    {
        if (!p_active_sat && !p_active_eq) return;
        if (!os) return;

        const int nCh = io.getNumChannels();
        const int nS = io.getNumSamples();

        sat_clean_buf.setSize(nCh, nS, false, false, true);
        sat_clean_buf.makeCopyOf(io);

        // DELAY ALIGN
        if (p_active_sat)
        {
            float latency = os->getLatencyInSamples();
            satInternalDelay.setDelay(latency);

            juce::dsp::AudioBlock<float> block(sat_clean_buf);
            juce::dsp::ProcessContextReplacing<float> context(block);
            satInternalDelay.process(context);
        }

        const float satMix01 = juce::jlimit(0.0f, 100.0f, p_sat_mix) * 0.01f;

        // NEW: PRE GAIN SMOOTHING
        sat_pre_lin_sm = smooth1p(sat_pre_lin_sm, sat_pre_lin_target, smooth_alpha_block);

        // Drive Smoothing Update
        sat_drive_lin_sm = smooth1p(sat_drive_lin_sm, sat_drive_lin_target, smooth_alpha_block);

        const float pre_gain = (float)sat_pre_lin_sm;

        // Apply Pre-Gain
        if (p_active_sat) {
            for (int ch = 0; ch < nCh; ++ch) {
                float* x = io.getWritePointer(ch);
                for (int i = 0; i < nS; ++i) x[i] *= pre_gain;
            }
        }

        // Sat AGC Input Measure
        double inPow = 0.0;
        if (p_active_sat && p_sat_autogain_mode != 0) {
            for (int ch = 0; ch < nCh; ++ch) {
                const float* x = io.getReadPointer(ch);
                double sum = 0.0;
                for (int i = 0; i < nS; ++i) sum += (double)x[i] * (double)x[i];
                inPow += sum;
            }
            inPow /= (double)(nS * nCh);
        }

        auto block = juce::dsp::AudioBlock<float>(io);
        auto osBlock = os->processSamplesUp(block);
        const int mode = p_sat_mode;

        const double drive = (double)sat_drive_lin_sm;

        const int osN = (int)osBlock.getNumSamples();

        // NULL TEST FIX: Strict bypass check
        const bool eq_tone_active = p_active_eq && (std::abs(p_sat_tone) > 0.01f);
        const bool eq_bright_active = p_active_eq && (std::abs(p_harm_bright) > 0.01f);

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

                // NULL FIX: Check bypass if gain is near zero
                if (eq_bright_active) s = pre.process(s);

                if (p_active_sat) {
                    s *= drive;
                    if (mode == 1) { // IRON - Fixed Unbounded Polynomial
                        const double bias = 0.075;
                        const double y0 = std::tanh(bias);
                        const double y = std::tanh(s + bias) - y0;

                        // FIX: Clamp input to cubic term
                        const double s_sat = std::tanh(s);
                        const double poly = s_sat - (s_sat * s_sat * s_sat) * 0.333333333333;

                        s = 0.82 * y + 0.18 * poly;
                    }
                    else if (mode == 2) { // STEEL
                        phi = phi * steel_leak_coeff + s * steel_dt;
                        const double y = std::tanh(phi * 7.0);
                        double dy = (y - yPrev) * steel_dy_gain;
                        yPrev = y;
                        dy = std::tanh(dy * 0.85);
                        const double base = std::tanh(s * 1.05);
                        s = 0.65 * dy + 0.35 * base;
                    }
                }

                // NULL FIX: Check bypass
                if (eq_bright_active) s = post.process(s);
                data[i] = (float)s;
            }
        }

        os->processSamplesDown(block);

        // Sat AGC Apply
        if (p_active_sat && p_sat_autogain_mode != 0 && inPow > 1e-24)
        {
            double outPow = 0.0;
            for (int ch = 0; ch < nCh; ++ch) {
                const float* y = io.getReadPointer(ch);
                double sum = 0.0;
                for (int i = 0; i < nS; ++i) sum += (double)y[i] * (double)y[i];
                outPow += sum;
            }
            outPow /= (double)(nS * nCh);
            if (outPow > 1e-24) {
                double g = std::sqrt((inPow + 1e-24) / (outPow + 1e-24));
                g = juce::jlimit(0.125, 8.0, g);
                const double exponent = (p_sat_autogain_mode == 1) ? 0.5 : 1.0;
                const double gTarget = std::pow(g, exponent);
                const double alpha = std::exp(-(double)nS / (0.050 * s_rate));
                sat_agc_gain_sm = sat_agc_gain_sm * alpha + gTarget * (1.0 - alpha);
                const float gSm = (float)sat_agc_gain_sm;
                for (int ch = 0; ch < nCh; ++ch) {
                    float* y = io.getWritePointer(ch);
                    for (int i = 0; i < nS; ++i) y[i] *= gSm;
                }
            }
        }

        // Voicing
        if (p_active_sat && (mode == 1 || mode == 2)) {
            for (int ch = 0; ch < nCh; ++ch) {
                float* y = io.getWritePointer(ch);
                auto& ironV = (ch == 0) ? iron_voicing_l : iron_voicing_r;
                auto& stLo = (ch == 0) ? steel_low_l : steel_low_r;
                auto& stHi = (ch == 0) ? steel_high_l : steel_high_r;
                for (int i = 0; i < nS; ++i) {
                    double s = (double)y[i];
                    if (mode == 1) s = ironV.process(s);
                    else { s = stLo.process(s); s = stHi.process(s); }
                    y[i] = (float)s;
                }
            }
        }

        // Trim/Tone
        sat_trim_lin_sm = smooth1p(sat_trim_lin_sm, sat_trim_lin, smooth_alpha_block);
        const double trim = (double)sat_trim_lin_sm;

        for (int ch = 0; ch < nCh; ++ch)
        {
            float* y = io.getWritePointer(ch);
            auto& tone = (ch == 0) ? sat_tone_l : sat_tone_r;
            for (int i = 0; i < nS; ++i)
            {
                double s = (double)y[i];
                if (p_active_sat) s *= trim;

                // Safety Clipper (Soft Limit)
                if (p_active_sat) s = std::tanh(s);

                // NULL FIX: Check bypass
                if (eq_tone_active) s = tone.process(s);
                y[i] = (float)s;
            }
        }

        // Clean Mix (Phase Aligned)
        if (p_active_sat)
        {
            for (int ch = 0; ch < nCh; ++ch) {
                float* y = io.getWritePointer(ch);
                const float* x = sat_clean_buf.getReadPointer(ch);
                for (int i = 0; i < nS; ++i) {
                    y[i] = x[i] + (y[i] - x[i]) * satMix01;
                }
            }
        }
    }

    // ... Internal state ...
    double s_rate = 44100.0;
    int max_block = 512;

    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    std::unique_ptr<juce::dsp::Oversampling<float>> os_dry;

    int os_stages = 2;
    int os_factor = 4;
    double os_srate = 176400.0;

    juce::dsp::DelayLine<float> satInternalDelay{ 4096 };

    SimpleBiquad sc_hp_l, sc_hp_r, sc_shelf_l, sc_shelf_r;
    SimpleBiquad sat_tone_l, sat_tone_r, harm_pre_l, harm_pre_r, harm_post_l, harm_post_r;
    SimpleBiquad iron_voicing_l, iron_voicing_r, steel_low_l, steel_low_r, steel_high_l, steel_high_r;

    double fb_prev_l = 0.0, fb_prev_r = 0.0;
    double det_env = 0.0, env = 0.0, env_fast = 0.0, env_slow = 0.0;
    double att_coeff = 0.999, rel_coeff_manual = 0.999, rel_coeff = 0.999, auto_rel_slow = 0.999, auto_rel_fast = 0.90;

    bool use_rms = false;
    int rms_window = 1;
    std::vector<double> rms_ring;
    int rms_pos = 0;
    double rms_sum = 0.0;
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
    double sat_agc_gain_sm = 1.0;
    double thrust_gain_db = 0.0;

    double makeup_lin_target = 1.0, makeup_lin_sm = 1.0;
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

    juce::AudioBuffer<float> dry_buf, wet_buf, sat_clean_buf;
};