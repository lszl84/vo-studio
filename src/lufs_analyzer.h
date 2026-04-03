#pragma once

#include <string>

// Structure to hold LUFS analysis results
struct LufsAnalysis
{
    double integratedLufs = -70.0;   // Integrated loudness (LUFS)
    double shortTermLufs = -70.0;    // Short-term loudness (3s window)
    double momentaryLufs = -70.0;    // Momentary loudness (400ms window)
    double peakDb = -96.0;           // True peak in dB
    double duration = 0.0;           // Duration in seconds
    bool valid = false;              // Whether analysis succeeded
};

// Analyze a WAV file and return LUFS measurements
// Uses FFmpeg/libav for audio decoding and EBU R128 standard for loudness measurement
LufsAnalysis AnalyzeLufs(const std::string& filepath);
