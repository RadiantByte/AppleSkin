#pragma once

#include "config/Settings.hpp"
#include <chrono>
#include <cmath>

namespace appleskin {
inline float previewOpacity(float opacity, const Options& options) {
    const auto peak = opacity * options.previewOpacity * 0.01f;
    if (!options.animatePreviews) return peak;
    const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    const auto phase = std::fmod(seconds * options.pulseSpeed, 1.0) * 6.283185307179586;
    return static_cast<float>(0.15 + 0.85 * (0.5 + 0.5 * std::sin(phase))) * peak;
}
}
