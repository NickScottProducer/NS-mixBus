/*
  ==============================================================================

    SimpleBiquad.h
    Exact replica of JSFX biquad behavior (Direct Form I)
    Updated with Peaking EQ for Iron/Steel voicing.

  ==============================================================================
*/

#pragma once
#include <cmath>
#include <algorithm>

struct SimpleBiquad {
    // Modern constant replacement for M_PI
    static constexpr double PI_CONST = 3.14159265358979323846;

    static inline double clampFreq(double freq, double sr)
    {
        const double nyq = 0.5 * sr;
        const double maxF = std::max(1.0, nyq * 0.49);
        return std::min(std::max(freq, 1.0), maxF);
    }

    static inline double clampQ(double Q)
    {
        if (!std::isfinite(Q)) return 0.707;
        return std::min(std::max(Q, 0.1), 20.0);
    }

    // Coefficients
    double b0 = 0.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0;

    // State
    double x1 = 0.0, x2 = 0.0;
    double y1 = 0.0, y2 = 0.0;

    void reset() {
        x1 = x2 = 0.0;
        y1 = y2 = 0.0;
        b0 = b1 = b2 = a1 = a2 = 0.0;
    }

    inline double process(double xn) {
        // Direct Form I difference equation
        double yn = b0 * xn + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;

        // Denormal protection (tiny offset logic or flush-to-zero)
        if (std::abs(yn) < 1e-24) yn = 0.0;

        // Shift state
        x2 = x1;
        x1 = xn;
        y2 = y1;
        y1 = yn;

        return yn;
    }

    // RBJ High Shelf
    void update_shelf(double freq, double gain_db, double Q, double sr) {
        if (sr <= 0.0) return;



        freq = clampFreq(freq, sr);
        Q = clampQ(Q);
        // Shelf formula here behaves like a slope control; keep it in a stable range.
        Q = std::min(Q, 1.0);
        if (!std::isfinite(gain_db)) gain_db = 0.0;
        double A = std::pow(10.0, gain_db / 40.0);
        double w0 = 2.0 * PI_CONST * freq / sr;
        double cos_w0 = std::cos(w0);
        double sin_w0 = std::sin(w0);
        // alpha = sin(w0)/2 * sqrt( (A + 1/A)*(1/Q - 1) + 2 )
        double alpha = (sin_w0 * 0.5) * std::sqrt((A + 1.0 / A) * (1.0 / Q - 1.0) + 2.0);

        double b0_t = A * ((A + 1.0) + (A - 1.0) * cos_w0 + 2.0 * std::sqrt(A) * alpha);
        double b1_t = -2.0 * A * ((A - 1.0) + (A + 1.0) * cos_w0);
        double b2_t = A * ((A + 1.0) + (A - 1.0) * cos_w0 - 2.0 * std::sqrt(A) * alpha);
        double a0_t = (A + 1.0) - (A - 1.0) * cos_w0 + 2.0 * std::sqrt(A) * alpha;
        double a1_t = 2.0 * ((A - 1.0) - (A + 1.0) * cos_w0);
        double a2_t = (A + 1.0) - (A - 1.0) * cos_w0 - 2.0 * std::sqrt(A) * alpha;

        double inv_a0 = 1.0 / a0_t;
        b0 = b0_t * inv_a0;
        b1 = b1_t * inv_a0;
        b2 = b2_t * inv_a0;
        a1 = a1_t * inv_a0;
        a2 = a2_t * inv_a0;
    }

    // RBJ Peaking EQ
    void update_peak(double freq, double gain_db, double Q, double sr) {
        if (sr <= 0.0) return;



        freq = clampFreq(freq, sr);
        Q = clampQ(Q);
        if (!std::isfinite(gain_db)) gain_db = 0.0;
        double A = std::pow(10.0, gain_db / 40.0);
        double w0 = 2.0 * PI_CONST * freq / sr;
        double cos_w0 = std::cos(w0);
        double alpha = std::sin(w0) / (2.0 * Q);

        double b0_t = 1.0 + alpha * A;
        double b1_t = -2.0 * cos_w0;
        double b2_t = 1.0 - alpha * A;
        double a0_t = 1.0 + alpha / A;
        double a1_t = -2.0 * cos_w0;
        double a2_t = 1.0 - alpha / A;

        double inv_a0 = 1.0 / a0_t;
        b0 = b0_t * inv_a0;
        b1 = b1_t * inv_a0;
        b2 = b2_t * inv_a0;
        a1 = a1_t * inv_a0;
        a2 = a2_t * inv_a0;
    }

    // RBJ HPF
    void update_hpf(double freq, double Q, double sr) {
        if (sr <= 0.0) return;



        freq = clampFreq(freq, sr);
        Q = clampQ(Q);
        double w0 = 2.0 * PI_CONST * freq / sr;
        double cos_w0 = std::cos(w0);
        double alpha = std::sin(w0) / (2.0 * Q);

        double b0_t = (1.0 + cos_w0) * 0.5;
        double b1_t = -(1.0 + cos_w0);
        double b2_t = (1.0 + cos_w0) * 0.5;
        double a0_t = 1.0 + alpha;
        double a1_t = -2.0 * cos_w0;
        double a2_t = 1.0 - alpha;

        double inv_a0 = 1.0 / a0_t;
        b0 = b0_t * inv_a0;
        b1 = b1_t * inv_a0;
        b2 = b2_t * inv_a0;
        a1 = a1_t * inv_a0;
        a2 = a2_t * inv_a0;
    }

    // RBJ LPF
    void update_lpf(double freq, double Q, double sr) {
        if (sr <= 0.0) return;



        freq = clampFreq(freq, sr);
        Q = clampQ(Q);
        double w0 = 2.0 * PI_CONST * freq / sr;
        double cos_w0 = std::cos(w0);
        double alpha = std::sin(w0) / (2.0 * Q);

        double b0_t = (1.0 - cos_w0) * 0.5;
        double b1_t = (1.0 - cos_w0);
        double b2_t = (1.0 - cos_w0) * 0.5;
        double a0_t = 1.0 + alpha;
        double a1_t = -2.0 * cos_w0;
        double a2_t = 1.0 - alpha;

        double inv_a0 = 1.0 / a0_t;
        b0 = b0_t * inv_a0;
        b1 = b1_t * inv_a0;
        b2 = b2_t * inv_a0;
        a1 = a1_t * inv_a0;
        a2 = a2_t * inv_a0;
    }
};