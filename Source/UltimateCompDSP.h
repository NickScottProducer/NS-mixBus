/*
  ==============================================================================

    UltimateCompDSP.h
    Port of Ultimate Mix Bus Compressor v3.4 (JSFX)

    CHANGES:
    - FIX: Added missing member declaration 'sat_trim_lin_sm' to fix compile error.
    - CLEANUP: Replaced M_PI macro with juce::MathConstants to fix macro warnings.

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
    float p_sat_trim = 0.0f;
    float p_sat_tone = 3.0f;
    float p_sat_tone_freq = 5500.0f;
    float p_sat_mix = 100.0f;
    int   p_sat_autogain_mode = 1; // 0=Off, 1=Partial, 2=Full

    // --- HARMONIC BRIGHTNESS ---
    float p_harm_bright = 0.0f;
    float p_harm_freq = 4500.0f;

    // --- OUTPUT ---
    float p_makeup = 0.0f;
    float p_dry_wet = 100.0f;
    float p_out_trim = 0.0f;

    // ==============================================================================
    // GETTERS FOR METERING
    // ==============================================================================
    float getGainReductiondB() const { return (float)env; }          // negative = reduction
    float getFluxSaturation() const { return (float)flux_env; }      // 0..1+ indicator

    // ==============================================================================
    // LIFECYCLE
    // ==============================================================================

    void prepare(double sampleRate, int maxBlockSamples)
    {
        s_rate = (sampleRate > 1.0 ? sampleRate : 44100.0);
        max_block = std::max(1, maxBlockSamples);

        // Internal buffers are always 2ch to keep mono handling simple.
        dry_buf.setSize(2, max_block, false, false, true);
        wet_buf.setSize(2, max_block, false, false, true);
        sat_clean_buf.setSize(2, max_block, false, false, true);

        // Oversampling (used for saturation processing only)
        // Stages: 2 => 4x
        os_stages = 2;
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

        iron_voicing_l.reset(); iron_voicing_r.reset();
        steel_low_l.reset(); steel_low_r.reset();
        steel_high_l.reset(); steel_high_r.reset();

        if (os)
            os->reset();

        // Saturation state (steel integrator)
        steel_phi_l = steel_phi_r = 0.0;
        steel_prev_x_l = steel_prev_x_r = 0.0;
        sat_agc_gain_sm = 1.0;

        // Compressor state
        fb_prev_l = fb_prev_r = 0.0;
        det_env = 0.0;
        env = 0.0;
        env_fast = 0.0;
        env_slow = 0.0;

        // Crest-follow state
        cf_peak_env = 0.0;
        cf_rms_sum = 0.0;
        cf_amt = 0.0;
        cf_ratio_mix = 0.0;

        // Flux state
        flux_env = 0.0;

        // Parameter smoothing state
        thresh_sm = p_thresh;
        ratio_sm = std::max(1.0, (double)p_ratio);
        knee_sm = std::max(0.0, (double)p_knee);

        makeup_lin_sm = dbToLin((double)p_makeup);
        out_lin_sm = dbToLin((double)p_out_trim);

        sat_drive_lin_sm = dbToLin((double)p_sat_drive);
        sat_trim_lin_sm = dbToLin((double)p_sat_trim); // Used here, now declared below
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
        if (nSamp <= 0)
            return;

        const int nChIn = buffer.getNumChannels();
        const bool isMono = (nChIn < 2);

        // Copy input to internal dry/wet buffers (2ch always)
        dry_buf.setSize(2, nSamp, false, false, true);
        wet_buf.setSize(2, nSamp, false, false, true);

        if (isMono)
        {
            const float* in = buffer.getReadPointer(0);
            float* d0 = dry_buf.getWritePointer(0);
            float* d1 = dry_buf.getWritePointer(1);
            for (int i = 0; i < nSamp; ++i)
            {
                d0[i] = in[i];
                d1[i] = in[i];
            }
        }
        else
        {
            dry_buf.copyFrom(0, 0, buffer, 0, 0, nSamp);
            dry_buf.copyFrom(1, 0, buffer, 1, 0, nSamp);
        }

        wet_buf.makeCopyOf(dry_buf, true);

        // Signal flow
        if (p_signal_flow == 1)
        {
            // Sat > Comp
            processSaturationBlock(wet_buf);
            processCompressorBlock(wet_buf);
        }
        else
        {
            // Comp > Sat
            processCompressorBlock(wet_buf);
            processSaturationBlock(wet_buf);
        }

        // Dry/Wet mix (post chain)
        const double dw_target = juce::jlimit(0.0, 1.0, (double)p_dry_wet / 100.0);
        drywet_sm = smooth1p(drywet_sm, dw_target, smooth_alpha);
        const double wetMix = drywet_sm;
        const double dryMix = 1.0 - wetMix;

        // Output trim (post mix)
        out_lin_sm = smooth1p(out_lin_sm, out_lin_target, smooth_alpha);
        const double outG = out_lin_sm;

        if (isMono)
        {
            float* out = buffer.getWritePointer(0);
            const float* dL = dry_buf.getReadPointer(0);
            const float* wL = wet_buf.getReadPointer(0);
            for (int i = 0; i < nSamp; ++i)
            {
                double y = (double)wL[i] * wetMix + (double)dL[i] * dryMix;
                y *= outG;
                out[i] = (float)y;
            }
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
                double yL = (double)wL[i] * wetMix + (double)dL[i] * dryMix;
                double yR = (double)wR[i] * wetMix + (double)dR[i] * dryMix;
                yL *= outG;
                yR *= outG;
                outL[i] = (float)yL;
                outR[i] = (float)yR;
            }
        }
    }

    // ==============================================================================
    // PARAMETER UPDATE (call once per block)
    // ==============================================================================

    void updateParameters()
    {
        // --- timing ---
        const double turboMul = (p_turbo ? 0.1 : 1.0);
        const double att_ms = std::max(0.05, (double)p_att_ms * turboMul);
        const double rel_ms = std::max(1.0, (double)p_rel_ms * turboMul);

        att_coeff = std::exp(-1000.0 / (att_ms * s_rate));
        rel_coeff_manual = std::exp(-1000.0 / (rel_ms * s_rate));

        // Auto-rel baseline
        auto_rel_slow = std::exp(-1000.0 / (1200.0 * s_rate));
        auto_rel_fast = std::exp(-1000.0 / (80.0 * s_rate));

        // RMS window
        use_rms = (p_det_rms > 0.0f);
        if (use_rms)
        {
            const double win_ms = std::max(1.0, (double)p_det_rms);
            const int n = std::max(1, (int)std::round((win_ms * 0.001) * s_rate));
            rms_window = n;
            if ((int)rms_ring.size() != rms_window)
            {
                rms_ring.assign((size_t)rms_window, 0.0);
                rms_pos = 0;
                rms_sum = 0.0;
            }
        }

        // Stereo link and feedback blend
        stereo_link = juce::jlimit(0.0, 1.0, (double)p_stereo_link / 100.0);
        fb_blend = juce::jlimit(0.0, 1.0, (double)p_fb_blend / 100.0);

        // Sidechain HPF
        sc_hp_l.update_hpf((double)p_sc_hp_freq, 0.707, s_rate);
        sc_hp_r.update_hpf((double)p_sc_hp_freq, 0.707, s_rate);

        // Thrust shelf
        thrust_gain_db = 0.0;
        if (p_thrust_mode == 1) thrust_gain_db = 3.0;       // Med
        if (p_thrust_mode == 2) thrust_gain_db = 6.0;       // Loud / Pink-ish
        if (p_thrust_mode > 0)
        {
            sc_shelf_l.update_shelf(90.0, thrust_gain_db, 0.707, s_rate);
            sc_shelf_r.update_shelf(90.0, thrust_gain_db, 0.707, s_rate);
        }

        // Crest-follow
        crest_target_db = (double)p_crest_target;
        crest_speed_ms = std::max(5.0, (double)p_crest_speed);
        crest_coeff = std::exp(-1000.0 / (crest_speed_ms * s_rate));

        // TP / Flux
        tp_enabled = (p_tp_mode != 0);
        tp_amt = juce::jlimit(0.0, 1.0, (double)p_tp_amount / 100.0);
        tp_raise_db = std::max(0.0, (double)p_tp_thresh_raise);

        flux_enabled = (p_flux_mode != 0);
        flux_amt = juce::jlimit(0.0, 1.0, (double)p_flux_amount / 100.0);

        // Output gain targets
        makeup_lin_target = dbToLin((double)p_makeup);
        out_lin_target = dbToLin((double)p_out_trim);

        // Parameter smoothing alphas (20 ms)
        smooth_alpha = std::exp(-1.0 / (0.020 * s_rate));

        // Oversampled rate used by saturation
        os_srate = s_rate * (double)os_factor;
        smooth_alpha_os = std::exp(-1.0 / (0.020 * os_srate));

        // Sat tone (post downsample)
        sat_tone_l.update_shelf((double)p_sat_tone_freq, (double)p_sat_tone, 0.707, s_rate);
        sat_tone_r.update_shelf((double)p_sat_tone_freq, (double)p_sat_tone, 0.707, s_rate);

        // Harmonic brightness shelves (oversampled)
        const double hb = (double)p_harm_bright;
        harm_pre_l.update_shelf((double)p_harm_freq, -hb, 0.707, os_srate);
        harm_pre_r.update_shelf((double)p_harm_freq, -hb, 0.707, os_srate);
        harm_post_l.update_shelf((double)p_harm_freq, hb, 0.707, os_srate);
        harm_post_r.update_shelf((double)p_harm_freq, hb, 0.707, os_srate);


        // Fixed transformer voicing (base rate)
        iron_voicing_l.update_shelf(100.0, 1.0, 0.707, s_rate);
        iron_voicing_r.update_shelf(100.0, 1.0, 0.707, s_rate);

        steel_low_l.update_shelf(40.0, 1.5, 0.707, s_rate);
        steel_low_r.update_shelf(40.0, 1.5, 0.707, s_rate);

        steel_high_l.update_lpf(9000.0, 0.707, s_rate);
        steel_high_r.update_lpf(9000.0, 0.707, s_rate);

        // Steel integrator/differentiator calibration at oversampled rate
        if (os_srate > 0.0)
        {
            steel_dt = 1.0 / os_srate;
            steel_dy_gain = os_srate;
            const double leak_hz = 6.0;
            steel_leak_coeff = std::exp(-2.0 * juce::MathConstants<double>::pi * leak_hz / os_srate);
        }

        // Sat gains
        sat_drive_lin_target = dbToLin((double)p_sat_drive);
        sat_mix_target = juce::jlimit(0.0, 1.0, (double)p_sat_mix / 100.0);
        sat_trim_lin = dbToLin((double)p_sat_trim);

        // Reset sat state if mode changes
        if (p_sat_mode != last_sat_mode)
        {
            steel_phi_l = steel_phi_r = 0.0;
            steel_prev_x_l = steel_prev_x_r = 0.0;
            sat_agc_gain_sm = 1.0;
            last_sat_mode = p_sat_mode;
        }

        // Reset crest state if mode changes
        if (p_ctrl_mode != last_ctrl_mode)
        {
            cf_peak_env = 0.0;
            cf_rms_sum = 0.0;
            cf_amt = 0.0;
            cf_ratio_mix = 0.0;
            last_ctrl_mode = p_ctrl_mode;
        }
    }

private:
    // ==============================================================================
    // Utilities
    // ==============================================================================

    static inline double dbToLin(double db)
    {
        return std::pow(10.0, db / 20.0);
    }

    static inline double linToDb(double lin)
    {
        const double x = std::max(lin, 1.0e-20);
        return 20.0 * std::log10(x);
    }

    static inline double smooth1p(double current, double target, double alpha)
    {
        // alpha in (0..1), closer to 1 => slower
        return current + (target - current) * (1.0 - alpha);
    }

    // ==============================================================================
    // Compressor block (in place, stereo, base rate)
    // ==============================================================================

    void processCompressorBlock(juce::AudioBuffer<float>& io)
    {
        const int nSamp = io.getNumSamples();
        float* l = io.getWritePointer(0);
        float* r = io.getWritePointer(1);

        // Targets for smoothed params
        const double thresh_target = (double)p_thresh;
        const double ratio_target = std::max(1.0, (double)p_ratio);
        const double knee_target = std::max(0.0, (double)p_knee);

        for (int i = 0; i < nSamp; ++i)
        {
            // Smooth core params at audio rate
            thresh_sm = smooth1p(thresh_sm, thresh_target, smooth_alpha);
            ratio_sm = smooth1p(ratio_sm, ratio_target, smooth_alpha);
            knee_sm = smooth1p(knee_sm, knee_target, smooth_alpha);

            makeup_lin_sm = smooth1p(makeup_lin_sm, makeup_lin_target, smooth_alpha);

            // Detector input (with feedback blend) uses *pre-makeup* previous output
            const double in_l = (double)l[i];
            const double in_r = (double)r[i];

            const double sc_l = in_l * (1.0 - fb_blend) + fb_prev_l * fb_blend;
            const double sc_r = in_r * (1.0 - fb_blend) + fb_prev_r * fb_blend;

            runDetector(sc_l, sc_r);

            // gain reduction
            const double lin_gain = std::pow(10.0, env / 20.0);

            const double vca_l = in_l * lin_gain;
            const double vca_r = in_r * lin_gain;

            // Feedback tap (FIX): use pre-makeup VCA output
            fb_prev_l = vca_l;
            fb_prev_r = vca_r;

            // Makeup is post VCA
            const double out_l = vca_l * makeup_lin_sm;
            const double out_r = vca_r * makeup_lin_sm;

            l[i] = (float)out_l;
            r[i] = (float)out_r;
        }
    }

    void runDetector(double sc_in_l, double sc_in_r)
    {
        // 1) HPF + optional thrust shelf
        double s_l = sc_hp_l.process(sc_in_l);
        double s_r = sc_hp_r.process(sc_in_r);

        if (p_thrust_mode > 0)
        {
            s_l = sc_shelf_l.process(s_l);
            s_r = sc_shelf_r.process(s_r);
        }

        // 2) detector magnitude (peak or RMS)
        double det_l_raw = 0.0, det_r_raw = 0.0;

        if (use_rms)
        {
            const double pL = s_l * s_l;
            const double pR = s_r * s_r;

            // Ring RMS across *linked* energy (keeps behavior consistent across stereo link)
            const double pAvg = 0.5 * (pL + pR);
            rms_sum += pAvg - rms_ring[(size_t)rms_pos];
            rms_ring[(size_t)rms_pos] = pAvg;
            rms_pos++; if (rms_pos >= rms_window) rms_pos = 0;

            const double rms = std::sqrt(std::max(0.0, rms_sum / (double)rms_window));
            det_l_raw = rms;
            det_r_raw = rms;
        }
        else
        {
            det_l_raw = std::abs(s_l);
            det_r_raw = std::abs(s_r);
        }

        // 3) Stereo link (FIX): blend between energy-average and max
        //    link=0% => average (more independent); link=100% => max (fully linked)
        const double det_avg = std::sqrt(0.5 * (det_l_raw * det_l_raw + det_r_raw * det_r_raw));
        const double det_max = std::max(det_l_raw, det_r_raw);
        const double det = det_avg * (1.0 - stereo_link) + det_max * stereo_link;

        // 4) Transient priority (optional)
        double eff_thresh_db = thresh_sm;
        if (tp_enabled)
        {
            const double pk = det_max;

            // Smooth peak envelope in linear domain
            const double det_fast = (pk > det_env)
                ? (att_coeff * det_env + (1.0 - att_coeff) * pk)
                : (auto_rel_fast * det_env + (1.0 - auto_rel_fast) * pk);

            det_env = det_fast;

            const double fast_db = linToDb(det_fast + 1e-20);
            const double avg_db = linToDb(det_avg + 1e-20);

            // Normalized transient-ness over ~12 dB
            const double metric = juce::jlimit(0.0, 1.0, (fast_db - avg_db) / 12.0);

            // Raise threshold during transients (dB-domain)
            eff_thresh_db += metric * tp_raise_db * tp_amt;
        }
        else
        {
            // Keep envelope tracking stable when toggling TP
            det_env = det;
        }

        // 5) Crest-follow (optional) modifies effective ratio/threshold
        double eff_ratio = ratio_sm;
        if (p_ctrl_mode != 0)
        {
            const double pk = det_max;

            // FIX: Use instant attack for peak envelope to truly track crest factor.
            if (pk > cf_peak_env)
                cf_peak_env = pk;
            else
                cf_peak_env = smooth1p(cf_peak_env, pk, crest_coeff);

            const double p = det_avg * det_avg;
            cf_rms_sum = smooth1p(cf_rms_sum, p, crest_coeff);
            const double rms = std::sqrt(std::max(0.0, cf_rms_sum));

            const double crest = linToDb((cf_peak_env + 1e-20) / (rms + 1e-20));
            const double err = crest - crest_target_db;

            // map to amount
            const double cf_step = (1.0 - crest_coeff) * 0.002;
            cf_amt = juce::jlimit(0.0, 1.0, cf_amt + err * cf_step);

            eff_ratio = ratio_sm * (1.0 + cf_amt * 2.0);
            eff_thresh_db -= cf_amt * 3.0;
        }

        // 6) Flux-coupled
        if (flux_enabled)
        {
            sat_drive_lin_sm = smooth1p(sat_drive_lin_sm, sat_drive_lin_target, smooth_alpha);
            const double drive = sat_drive_lin_sm;

            const double meas_pk = det_max * drive;
            const double meas_db = linToDb(meas_pk + 1e-20);
            const double metric = juce::jlimit(0.0, 1.0, (meas_db - (-24.0)) / 24.0);
            flux_env = std::max(metric, flux_env * 0.995);
            eff_thresh_db += flux_env * (6.0 * flux_amt);
        }

        // 7) static curve (soft knee)
        const double det_db = linToDb(det + 1e-20);
        const double knee = knee_sm;

        double gr_db = 0.0;
        if (knee > 0.0)
        {
            const double x = det_db - eff_thresh_db;
            const double half = knee * 0.5;
            if (x <= -half)
            {
                gr_db = 0.0;
            }
            else if (x >= half)
            {
                gr_db = -(x - x / eff_ratio);
            }
            else
            {
                const double t = (x + half) / (knee + 1e-20);
                const double y = t * t * (3.0 - 2.0 * t); // smoothstep
                const double full = -(x - x / eff_ratio);
                gr_db = full * y;
            }
        }
        else
        {
            const double x = det_db - eff_thresh_db;
            if (x > 0.0)
                gr_db = -(x - x / eff_ratio);
        }

        // 8) Attack/release smoothing for GR in dB
        const double target = gr_db;

        if (target < env) // attack (more reduction)
        {
            env = att_coeff * env + (1.0 - att_coeff) * target;
            env_fast = env_slow = env;
            rel_coeff = att_coeff;
        }
        else // release
        {
            if (p_auto_rel != 0)
            {
                // Dual-stage auto release (fast + slow), take the more-negative envelope
                env_fast = auto_rel_fast * env_fast + (1.0 - auto_rel_fast) * target;
                env_slow = auto_rel_slow * env_slow + (1.0 - auto_rel_slow) * target;
                env = std::min(env_fast, env_slow);
                rel_coeff = auto_rel_slow;
            }
            else
            {
                env = rel_coeff_manual * env + (1.0 - rel_coeff_manual) * target;
                env_fast = env_slow = env;
                rel_coeff = rel_coeff_manual;
            }
        }
    }


    // ==============================================================================
    // Saturation block (in place, stereo)
    // ==============================================================================

    void processSaturationBlock(juce::AudioBuffer<float>& io)
    {
        if (!os) return;

        const int nCh = io.getNumChannels();
        const int nS = io.getNumSamples();
        if (nCh <= 0 || nS <= 0) return;

        const float satMix01 = juce::jlimit(0.0f, 100.0f, p_sat_mix) * 0.01f;
        const bool needClean = (satMix01 < 0.999f);

        // Input power (for saturation AGC)
        double inPow = 0.0;
        if (p_sat_autogain_mode != 0)
        {
            for (int ch = 0; ch < nCh; ++ch)
            {
                const float* x = io.getReadPointer(ch);
                double sum = 0.0;
                for (int i = 0; i < nS; ++i)
                    sum += (double)x[i] * (double)x[i];
                inPow += sum;
            }
            inPow /= (double)(nS * nCh);
        }

        if (needClean)
        {
            sat_clean_buf.setSize(nCh, nS, false, false, true);
            sat_clean_buf.makeCopyOf(io);
        }

        auto block = juce::dsp::AudioBlock<float>(io);
        auto osBlock = os->processSamplesUp(block);

        const int mode = p_sat_mode;
        const double drive = (double)sat_drive_lin_sm;

        const int osN = (int)osBlock.getNumSamples();

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

                // Pre-emphasis (user "Harmonic Brightness")
                s = pre.process(s);

                // Drive
                s *= drive;

                if (mode == 1) // IRON: even-weighted, smooth
                {
                    const double bias = 0.075;
                    const double y0 = std::tanh(bias);
                    const double y = std::tanh(s + bias) - y0;

                    // small polynomial blend for texture without getting excessively dark
                    const double poly = s - (s * s * s) * 0.333333333333;
                    s = 0.82 * y + 0.18 * poly;
                }
                else if (mode == 2) // STEEL: leaky integrator -> tanh -> differentiator
                {
                    // Leaky integrator in "flux" domain
                    phi = phi * steel_leak_coeff + s * steel_dt;

                    const double y = std::tanh(phi * 7.0);               // core saturation
                    double dy = (y - yPrev) * steel_dy_gain;        // differentiator
                    yPrev = y;

                    // Band-limit and blend with a modest direct path for audibility
                    dy = std::tanh(dy * 0.85);
                    const double base = std::tanh(s * 1.05);
                    s = 0.65 * dy + 0.35 * base;
                }

                // De-emphasis
                s = post.process(s);

                data[i] = (float)s;
            }
        }

        os->processSamplesDown(block);

        // Saturation AGC (block RMS match, pre user trim/tone, pre mix)
        if (p_sat_autogain_mode != 0 && inPow > 1e-24)
        {
            double outPow = 0.0;
            for (int ch = 0; ch < nCh; ++ch)
            {
                const float* y = io.getReadPointer(ch);
                double sum = 0.0;
                for (int i = 0; i < nS; ++i)
                    sum += (double)y[i] * (double)y[i];
                outPow += sum;
            }
            outPow /= (double)(nS * nCh);

            if (outPow > 1e-24)
            {
                double g = std::sqrt((inPow + 1e-24) / (outPow + 1e-24));
                g = juce::jlimit(0.125, 8.0, g);

                const double exponent = (p_sat_autogain_mode == 1) ? 0.5 : 1.0;
                const double gTarget = std::pow(g, exponent);

                const double alpha = std::exp(-(double)nS / (0.050 * s_rate));
                sat_agc_gain_sm = sat_agc_gain_sm * alpha + gTarget * (1.0 - alpha);

                const float gSm = (float)sat_agc_gain_sm;
                for (int ch = 0; ch < nCh; ++ch)
                {
                    float* y = io.getWritePointer(ch);
                    for (int i = 0; i < nS; ++i)
                        y[i] *= gSm;
                }
            }
        }

        // Mode-dependent fixed voicing (base rate, pre user trim/tone)
        if (mode == 1 || mode == 2)
        {
            for (int ch = 0; ch < nCh; ++ch)
            {
                float* y = io.getWritePointer(ch);

                auto& ironV = (ch == 0) ? iron_voicing_l : iron_voicing_r;
                auto& stLo = (ch == 0) ? steel_low_l : steel_low_r;
                auto& stHi = (ch == 0) ? steel_high_l : steel_high_r;

                for (int i = 0; i < nS; ++i)
                {
                    double s = (double)y[i];

                    if (mode == 1)
                    {
                        s = ironV.process(s);
                    }
                    else // mode == 2
                    {
                        s = stLo.process(s);
                        s = stHi.process(s);
                    }

                    y[i] = (float)s;
                }
            }
        }

        // User trim + tone shelf
        // Smooth the trim value towards the target (sat_trim_lin)
        sat_trim_lin_sm = smooth1p(sat_trim_lin_sm, sat_trim_lin, smooth_alpha);
        const double trim = (double)sat_trim_lin_sm;

        for (int ch = 0; ch < nCh; ++ch)
        {
            float* y = io.getWritePointer(ch);
            auto& tone = (ch == 0) ? sat_tone_l : sat_tone_r;

            for (int i = 0; i < nS; ++i)
            {
                double s = (double)y[i];
                s *= trim;
                s = tone.process(s);
                y[i] = (float)s;
            }
        }

        // Mix
        if (needClean)
        {
            for (int ch = 0; ch < nCh; ++ch)
            {
                float* y = io.getWritePointer(ch);
                const float* x = sat_clean_buf.getReadPointer(ch);

                for (int i = 0; i < nS; ++i)
                    y[i] = x[i] + (y[i] - x[i]) * satMix01;
            }
        }
    }


    // ==============================================================================
    // INTERNAL STATE
    // ==============================================================================

    double s_rate = 44100.0;
    int max_block = 512;

    // oversampling
    std::unique_ptr<juce::dsp::Oversampling<float>> os;
    int os_stages = 2;
    int os_factor = 4;
    double os_srate = 176400.0;

    // filters
    SimpleBiquad sc_hp_l, sc_hp_r;
    SimpleBiquad sc_shelf_l, sc_shelf_r;

    SimpleBiquad sat_tone_l, sat_tone_r;
    SimpleBiquad harm_pre_l, harm_pre_r;
    SimpleBiquad harm_post_l, harm_post_r;

    // transformer voicing (fixed, mode-dependent)
    SimpleBiquad iron_voicing_l, iron_voicing_r;
    SimpleBiquad steel_low_l, steel_low_r;
    SimpleBiquad steel_high_l, steel_high_r;

    // comp state
    double fb_prev_l = 0.0, fb_prev_r = 0.0;
    double det_env = 0.0;
    double env = 0.0; // GR in dB (negative)
    double env_fast = 0.0;
    double env_slow = 0.0;

    // compressor derived
    double att_coeff = 0.999;
    double rel_coeff_manual = 0.999;
    double rel_coeff = 0.999;
    double auto_rel_slow = 0.999;
    double auto_rel_fast = 0.90;

    bool use_rms = false;
    int rms_window = 1;
    std::vector<double> rms_ring;
    int rms_pos = 0;
    double rms_sum = 0.0;

    double stereo_link = 1.0;
    double fb_blend = 0.0;

    // crest-follow
    double crest_target_db = 12.0;
    double crest_speed_ms = 400.0;
    double crest_coeff = 0.999;
    double cf_peak_env = 0.0;
    double cf_rms_sum = 0.0;
    double cf_amt = 0.0;
    double cf_ratio_mix = 0.0;

    // TP / Flux
    bool tp_enabled = false;
    double tp_amt = 0.5;
    double tp_raise_db = 12.0;

    bool flux_enabled = false;
    double flux_amt = 0.3;
    double flux_env = 0.0;

    // sat state
    double steel_phi_l = 0.0, steel_phi_r = 0.0;
    double steel_prev_x_l = 0.0, steel_prev_x_r = 0.0; // used as previous y for steel differentiator

    // saturation derived (updated in updateParameters)
    double steel_dt = 0.0;
    double steel_dy_gain = 1.0;
    double steel_leak_coeff = 1.0;

    // saturation AGC (post-nonlinearity, pre user-trim/tone)
    double sat_agc_gain_sm = 1.0;

    double thrust_gain_db = 0.0;

    // gains (targets + smoothed)
    double makeup_lin_target = 1.0;
    double makeup_lin_sm = 1.0;

    double out_lin_target = 1.0;
    double out_lin_sm = 1.0;

    double sat_drive_lin_target = 1.0;
    double sat_drive_lin_sm = 1.0;
    double sat_drive_lin_sm_os = 1.0;

    double sat_trim_lin = 1.0;
    double sat_trim_lin_sm = 1.0; // <--- FIXED: Added this variable

    double sat_mix_target = 1.0;
    double sat_mix_sm = 1.0;

    double drywet_sm = 1.0;

    // param smoothing
    double smooth_alpha = 0.999;
    double smooth_alpha_os = 0.999;

    double thresh_sm = -20.0;
    double ratio_sm = 4.0;
    double knee_sm = 6.0;

    int last_sat_mode = -1;
    int last_ctrl_mode = -1;

    // scratch
    juce::AudioBuffer<float> dry_buf;
    juce::AudioBuffer<float> wet_buf;
    juce::AudioBuffer<float> sat_clean_buf;
};