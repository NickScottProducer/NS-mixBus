/*
  ==============================================================================

    UltimateCompDSP.h
    Port of Ultimate Mix Bus Compressor v3.5 (JSFX)
    Updated per request:
      - SAT AUTO GAIN: Toggleable 1/x compensation
      - STEEL FIX: Normalized baseline gain to match other modes
      - OUTPUT: Bipolar Output Gain handling

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include "SimpleBiquad.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class UltimateCompDSP
{
public:
    UltimateCompDSP() { resetState(); }

    // ==============================================================================
    // PUBLIC PARAMETERS
    // ==============================================================================

    // --- GLOBAL ---
    int   p_signal_flow = 0; // 0 = Comp > Sat, 1 = Sat > Comp

    // --- DYNAMICS ---
    float p_thresh = -20.0f;
    float p_ratio = 4.0f;
    float p_knee = 6.0f;
    float p_att_ms = 10.0f;
    float p_rel_ms = 100.0f;
    int   p_auto_rel = 0;
    int   p_turbo = 0;

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
    float p_sat_drive = 0.0f;
    int   p_sat_autogain = 0;    // NEW: Auto Gain Toggle
    float p_sat_trim = 0.0f;
    float p_sat_tone = 3.0f;
    float p_sat_tone_freq = 5500.0f;
    float p_sat_mix = 100.0f;

    // --- HARMONIC BRIGHTNESS ---
    float p_harm_bright = 0.0f;
    float p_harm_freq = 4500.0f;

    // --- OUTPUT ---
    float p_makeup = 0.0f;
    float p_dry_wet = 100.0f;
    float p_out_trim = 0.0f; // Bipolar -24 to +24

    // ==============================================================================
    // GETTERS FOR METERING
    // ==============================================================================
    float getGainReductiondB() const { return (float)env_db; }          // negative = reduction
    float getFluxSaturation() const { return (float)flux_env; }      // 0..1+ indicator

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

        // Oversampling
        os_stages = 2; // 4x
        os_factor = 1 << os_stages;

        os = std::make_unique<juce::dsp::Oversampling<float>>(
            2,
            os_stages,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true);

        os->initProcessing((size_t)max_block);

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

        iron_pre_l.reset(); iron_pre_r.reset();
        steel_pre_l.reset(); steel_pre_r.reset();

        if (os) os->reset();

        steel_phi_l = steel_phi_r = 0.0;
        steel_prev_x_l = steel_prev_x_r = 0.0;
        steel_prev_y_l = steel_prev_y_r = 0.0;

        fb_prev_l = fb_prev_r = 0.0;
        det_env = 0.0;
        env_db = 0.0;

        env_fast_db = 0.0;
        env_slow_db = 0.0;

        cf_peak_env = 0.0;
        cf_rms_sum = 0.0;
        cf_amt = 0.0;

        flux_env = 0.0;

        thresh_sm = p_thresh;
        ratio_sm = std::max(1.0, (double)p_ratio);
        knee_sm = std::max(0.0, (double)p_knee);

        makeup_lin_sm = dbToLin((double)p_makeup);
        out_lin_sm = dbToLin((double)p_out_trim);

        sat_drive_lin_sm = dbToLin((double)p_sat_drive);
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

        const int nChIn = buffer.getNumChannels();
        const bool isMono = (nChIn < 2);

        // Copy input
        dry_buf.setSize(2, nSamp, false, false, true);
        wet_buf.setSize(2, nSamp, false, false, true);

        if (isMono)
        {
            const float* in = buffer.getReadPointer(0);
            float* d0 = dry_buf.getWritePointer(0);
            float* d1 = dry_buf.getWritePointer(1);
            for (int i = 0; i < nSamp; ++i) { d0[i] = in[i]; d1[i] = in[i]; }
        }
        else
        {
            dry_buf.copyFrom(0, 0, buffer, 0, 0, nSamp);
            dry_buf.copyFrom(1, 0, buffer, 1, 0, nSamp);
        }

        wet_buf.makeCopyOf(dry_buf, true);

        // Process Chain
        if (p_signal_flow == 1) {
            processSaturationBlock(wet_buf);
            processCompressorBlock(wet_buf);
        }
        else {
            processCompressorBlock(wet_buf);
            processSaturationBlock(wet_buf);
        }

        // Mix & Output
        const double dw_target = juce::jlimit(0.0, 1.0, (double)p_dry_wet / 100.0);
        drywet_sm = smooth1p(drywet_sm, dw_target, smooth_alpha);
        const double wetMix = drywet_sm;
        const double dryMix = 1.0 - wetMix;

        out_lin_sm = smooth1p(out_lin_sm, out_lin_target, smooth_alpha);
        const double outG = out_lin_sm;

        if (isMono)
        {
            float* out = buffer.getWritePointer(0);
            const float* dL = dry_buf.getReadPointer(0);
            const float* wL = wet_buf.getReadPointer(0);
            for (int i = 0; i < nSamp; ++i)
                out[i] = (float)((wL[i] * wetMix + dL[i] * dryMix) * outG);
        }
        else
        {
            float* outL = buffer.getWritePointer(0);
            float* outR = buffer.getWritePointer(1);
            const float* dL = dry_buf.getReadPointer(0);
            const float* dR = dry_buf.getReadPointer(1);
            const float* wL = wet_buf.getReadPointer(0);
            const float* wR = wet_buf.getReadPointer(1);
            for (int i = 0; i < nSamp; ++i)
            {
                outL[i] = (float)((wL[i] * wetMix + dL[i] * dryMix) * outG);
                outR[i] = (float)((wR[i] * wetMix + dR[i] * dryMix) * outG);
            }
        }
    }

    // ==============================================================================
    // PARAMETER UPDATE
    // ==============================================================================

    void updateParameters()
    {
        const double turboMul = (p_turbo ? 0.1 : 1.0);
        const double att_ms = std::max(0.05, (double)p_att_ms * turboMul);
        const double rel_ms = std::max(1.0, (double)p_rel_ms * turboMul);

        att_coeff = std::exp(-1000.0 / (att_ms * s_rate));
        rel_coeff_manual = std::exp(-1000.0 / (rel_ms * s_rate));

        auto_rel_fast_coeff = std::exp(-1000.0 / (100.0 * s_rate));
        auto_rel_slow_coeff = std::exp(-1000.0 / (2000.0 * s_rate));
        tp_rel_fast = std::exp(-1000.0 / (50.0 * s_rate));

        // RMS
        use_rms = (p_det_rms > 0.0f);
        if (use_rms)
        {
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

        makeup_lin_target = dbToLin((double)p_makeup);
        out_lin_target = dbToLin((double)p_out_trim);

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

        iron_pre_l.update_peak(100.0, 2.0, 0.8, os_srate);
        iron_pre_r.update_peak(100.0, 2.0, 0.8, os_srate);

        steel_pre_l.update_peak(40.0, 3.0, 1.5, os_srate);
        steel_pre_r.update_peak(40.0, 3.0, 1.5, os_srate);

        sat_drive_lin_target = dbToLin((double)p_sat_drive);
        sat_mix_target = juce::jlimit(0.0, 1.0, (double)p_sat_mix / 100.0);
        sat_trim_lin = dbToLin((double)p_sat_trim);

        if (p_sat_mode != last_sat_mode) {
            steel_phi_l = steel_phi_r = 0.0;
            steel_prev_x_l = steel_prev_x_r = 0.0;
            steel_prev_y_l = steel_prev_y_r = 0.0;
            last_sat_mode = p_sat_mode;
        }

        if (p_ctrl_mode != last_ctrl_mode) {
            cf_peak_env = 0.0; cf_rms_sum = 0.0; cf_amt = 0.0;
            last_ctrl_mode = p_ctrl_mode;
        }
    }

private:
    static inline double dbToLin(double db) { return std::pow(10.0, db / 20.0); }
    static inline double linToDb(double lin) { return 20.0 * std::log10(std::max(lin, 1.0e-20)); }
    static inline double smooth1p(double c, double t, double a) { return c + (t - c) * (1.0 - a); }

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

            const double sc_l = in_l * (1.0 - fb_blend) + fb_prev_l * fb_blend;
            const double sc_r = in_r * (1.0 - fb_blend) + fb_prev_r * fb_blend;

            runDetector(sc_l, sc_r);

            const double lin_gain = std::pow(10.0, env_db / 20.0);
            const double out_l = in_l * lin_gain * makeup_lin_sm;
            const double out_r = in_r * lin_gain * makeup_lin_sm;

            fb_prev_l = out_l; fb_prev_r = out_r;

            l[i] = (float)out_l;
            r[i] = (float)out_r;
        }
    }

    void runDetector(double sc_in_l, double sc_in_r)
    {
        double s_l = sc_hp_l.process(sc_in_l);
        double s_r = sc_hp_r.process(sc_in_r);
        if (p_thrust_mode > 0) { s_l = sc_shelf_l.process(s_l); s_r = sc_shelf_r.process(s_r); }

        double det_l_raw, det_r_raw;
        if (use_rms) {
            const double pAvg = 0.5 * (s_l * s_l + s_r * s_r);
            rms_sum += pAvg - rms_ring[(size_t)rms_pos];
            rms_ring[(size_t)rms_pos] = pAvg;
            rms_pos++; if (rms_pos >= rms_window) rms_pos = 0;
            double rms = std::sqrt(std::max(0.0, rms_sum / (double)rms_window));
            det_l_raw = rms; det_r_raw = rms;
        }
        else {
            det_l_raw = std::abs(s_l); det_r_raw = std::abs(s_r);
        }

        const double det_avg = std::sqrt(0.5 * (det_l_raw * det_l_raw + det_r_raw * det_r_raw));
        const double det_max = std::max(det_l_raw, det_r_raw);
        const double det = det_avg * (1.0 - stereo_link) + det_max * stereo_link;

        double eff_thresh_db = thresh_sm;
        if (tp_enabled) {
            const double pk = det_max;
            const double det_fast = (pk > det_env) ? (att_coeff * det_env + (1.0 - att_coeff) * pk)
                : (tp_rel_fast * det_env + (1.0 - tp_rel_fast) * pk);
            const double tp_metric_db = linToDb(std::max(0.0, det_fast - det_avg) + 1.0);
            eff_thresh_db += (tp_metric_db * tp_raise_db * tp_amt * 2.0);
        }

        double eff_ratio = ratio_sm;
        if (p_ctrl_mode != 0) {
            cf_peak_env = smooth1p(cf_peak_env, det_max, crest_coeff);
            cf_rms_sum = smooth1p(cf_rms_sum, det_avg * det_avg, crest_coeff);
            const double rms = std::sqrt(std::max(0.0, cf_rms_sum));
            const double crest = linToDb((cf_peak_env + 1e-20) / (rms + 1e-20));
            cf_amt = juce::jlimit(0.0, 1.0, cf_amt + (crest - crest_target_db) * 0.001);
            eff_ratio = ratio_sm * (1.0 + cf_amt * 2.0);
            eff_thresh_db += cf_amt * 3.0;
        }

        if (flux_enabled) {
            sat_drive_lin_sm = smooth1p(sat_drive_lin_sm, sat_drive_lin_target, smooth_alpha);
            flux_env = std::max(det_max * sat_drive_lin_sm * 0.25, flux_env * 0.995);
            eff_thresh_db += linToDb(flux_env + 1.0) * flux_amt * 5.0;
        }

        const double det_db = linToDb(det + 1e-20);
        double gr_db = 0.0;
        if (knee_sm > 0.0) {
            double x = det_db - eff_thresh_db;
            double half = knee_sm * 0.5;
            if (x >= half) gr_db = -(x - x / eff_ratio);
            else if (x > -half) {
                double t = (x + half) / (knee_sm + 1e-20);
                gr_db = -(x - x / eff_ratio) * (t * t * (3.0 - 2.0 * t));
            }
        }
        else {
            double x = det_db - eff_thresh_db;
            if (x > 0.0) gr_db = -(x - x / eff_ratio);
        }

        const double target = gr_db;
        if (target < env_db) {
            env_db = att_coeff * env_db + (1.0 - att_coeff) * target;
            env_fast_db = env_db; env_slow_db = env_db;
        }
        else {
            if (p_auto_rel != 0) {
                env_fast_db = auto_rel_fast_coeff * env_fast_db + (1.0 - auto_rel_fast_coeff) * target;
                env_slow_db = auto_rel_slow_coeff * env_slow_db + (1.0 - auto_rel_slow_coeff) * target;
                env_db = std::min(env_fast_db, env_slow_db);
            }
            else {
                env_db = rel_coeff_manual * env_db + (1.0 - rel_coeff_manual) * target;
            }
        }
        det_env = det;
    }

    void processSaturationBlock(juce::AudioBuffer<float>& io)
    {
        if (p_sat_mode == 0) return;

        sat_drive_lin_sm = smooth1p(sat_drive_lin_sm, sat_drive_lin_target, smooth_alpha);
        sat_mix_sm = smooth1p(sat_mix_sm, sat_mix_target, smooth_alpha);
        const double satMix = sat_mix_sm;
        if (satMix <= 0.0001) return;

        const bool needClean = (satMix < 0.9999);
        if (needClean) {
            sat_clean_buf.setSize(2, io.getNumSamples(), false, false, true);
            sat_clean_buf.makeCopyOf(io, true);
        }

        juce::dsp::AudioBlock<float> block(io);
        auto osBlock = os->processSamplesUp(block);
        const int osSamples = (int)osBlock.getNumSamples();
        const int channels = (int)osBlock.getNumChannels();

        for (int ch = 0; ch < channels; ++ch)
        {
            float* x = osBlock.getChannelPointer((size_t)ch);
            double& phi = (ch == 0) ? steel_phi_l : steel_phi_r;
            double& prevy = (ch == 0) ? steel_prev_y_l : steel_prev_y_r;
            SimpleBiquad& pre = (ch == 0) ? harm_pre_l : harm_pre_r;
            SimpleBiquad& post = (ch == 0) ? harm_post_l : harm_post_r;
            SimpleBiquad& ironF = (ch == 0) ? iron_pre_l : iron_pre_r;
            SimpleBiquad& steelF = (ch == 0) ? steel_pre_l : steel_pre_r;

            for (int i = 0; i < osSamples; ++i)
            {
                sat_drive_lin_sm_os = smooth1p(sat_drive_lin_sm_os, sat_drive_lin_sm, smooth_alpha_os);
                const double drive = sat_drive_lin_sm_os;
                double s = (double)x[i];

                s = pre.process(s);
                s *= drive;

                if (p_sat_mode == 1) { // IRON
                    s = ironF.process(s);
                    const double bias = 0.15;
                    s = (std::tanh(s + bias) - std::tanh(bias)) * 1.05;
                }
                else { // STEEL
                    s = steelF.process(s);
                    const double dt = 1.0 / (os_srate + 1e-12);
                    const double leak = 30.0;
                    const double core_drive = 40.0; // High gain core
                    phi = phi * (1.0 - leak * dt) + s * core_drive * dt;
                    const double sat_flux = std::tanh(phi);
                    double dy = (sat_flux - prevy) * os_srate;
                    prevy = sat_flux;
                    // Blend derivative with raw saturated signal
                    s = dy * 0.1 + std::tanh(s) * 0.9;

                    // FIXED: Steel Compensation.
                    // Steel is inherently ~8-10dB louder due to core gain mechanics.
                    // We apply a static pad here so that Drive=0dB matches Unity gain better.
                    s *= 0.4;
                }

                // --- NEW AUTO GAIN LOGIC ---
                // If enabled, apply strict inverse linear compensation.
                // If disabled, allow full volume swell.
                if (p_sat_autogain) {
                    if (drive > 1.0e-5) s *= (1.0 / drive);
                }

                s = post.process(s);
                x[i] = (float)s;
            }
        }

        os->processSamplesDown(block);

        const int nSamp = io.getNumSamples();
        float* l = io.getWritePointer(0);
        float* r = io.getWritePointer(1);

        for (int i = 0; i < nSamp; ++i)
        {
            double yl = (double)l[i] * sat_trim_lin;
            double yr = (double)r[i] * sat_trim_lin;
            yl = sat_tone_l.process(yl);
            yr = sat_tone_r.process(yr);
            l[i] = (float)yl; r[i] = (float)yr;
        }

        if (needClean) {
            const float* cl = sat_clean_buf.getReadPointer(0);
            const float* cr = sat_clean_buf.getReadPointer(1);
            const double dryMix = 1.0 - satMix;
            for (int i = 0; i < nSamp; ++i) {
                l[i] = (float)((double)l[i] * satMix + (double)cl[i] * dryMix);
                r[i] = (float)((double)r[i] * satMix + (double)cr[i] * dryMix);
            }
        }
    }

    // ==============================================================================
    // INTERNAL STATE
    // ==============================================================================

    double s_rate = 44100.0;
    int max_block = 512;

    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    int os_stages = 2;
    int os_factor = 4;
    double os_srate = 176400.0;

    SimpleBiquad sc_hp_l, sc_hp_r;
    SimpleBiquad sc_shelf_l, sc_shelf_r;
    SimpleBiquad sat_tone_l, sat_tone_r;
    SimpleBiquad harm_pre_l, harm_pre_r;
    SimpleBiquad harm_post_l, harm_post_r;
    SimpleBiquad iron_pre_l, iron_pre_r;
    SimpleBiquad steel_pre_l, steel_pre_r;

    double fb_prev_l = 0.0, fb_prev_r = 0.0;
    double det_env = 0.0;
    double env_db = 0.0;

    double att_coeff = 0.999;
    double rel_coeff_manual = 0.999;
    double auto_rel_fast_coeff = 0.90;
    double auto_rel_slow_coeff = 0.9995;
    double env_fast_db = 0.0;
    double env_slow_db = 0.0;
    double tp_rel_fast = 0.90;

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

    bool tp_enabled = false;
    double tp_amt = 0.5;
    double tp_raise_db = 12.0;

    bool flux_enabled = false;
    double flux_amt = 0.3;
    double flux_env = 0.0;

    double steel_phi_l = 0.0, steel_phi_r = 0.0;
    double steel_prev_x_l = 0.0, steel_prev_x_r = 0.0;
    double steel_prev_y_l = 0.0, steel_prev_y_r = 0.0;

    double thrust_gain_db = 0.0;

    double makeup_lin_target = 1.0;
    double makeup_lin_sm = 1.0;

    double out_lin_target = 1.0;
    double out_lin_sm = 1.0;

    double sat_drive_lin_target = 1.0;
    double sat_drive_lin_sm = 1.0;
    double sat_drive_lin_sm_os = 1.0;
    double sat_trim_lin = 1.0;
    double sat_mix_target = 1.0;
    double sat_mix_sm = 1.0;

    double drywet_sm = 1.0;

    double smooth_alpha = 0.999;
    double smooth_alpha_os = 0.999;

    double thresh_sm = -20.0;
    double ratio_sm = 4.0;
    double knee_sm = 6.0;

    int last_sat_mode = -1;
    int last_ctrl_mode = -1;

    juce::AudioBuffer<float> dry_buf;
    juce::AudioBuffer<float> wet_buf;
    juce::AudioBuffer<float> sat_clean_buf;
};