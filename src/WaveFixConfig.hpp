#pragma once

#include <Geode/Geode.hpp>

#include <cmath>

using namespace geode::prelude;

namespace wavefix {
    constexpr char const* kEnabledSetting      = "enabled";
    constexpr char const* kLoggingSetting      = "logging";
    constexpr char const* kStateLoggingSetting = "stateLogging";
    constexpr char const* kVisualizerSetting   = "visualizer";
    
    constexpr double kEpsilon              = 0.000001;
    constexpr double kSlopeTolerance       = 0.03;
    constexpr double kAnchorDriftTolerance = 0.125;
    constexpr double kLargeJumpDistance    = 60.0;
    constexpr int kWaveTrailRestartFrames  = 2;

    inline bool g_fixEnabled          = true;
    inline bool g_loggingEnabled      = false;
    inline bool g_stateLoggingEnabled = false;
    inline bool g_visualizerEnabled   = false;

#define WAVEFIX_LOG_FIX(...)   do { if (wavefix::g_loggingEnabled)      [[unlikely]] ::geode::log::info(__VA_ARGS__); } while (0)
#define WAVEFIX_LOG_STATE(...) do { if (wavefix::g_stateLoggingEnabled) [[unlikely]] ::geode::log::info(__VA_ARGS__); } while (0)

    inline double waveRatio(float vehicleSize) {
        return vehicleSize < 1.0f ? 2.0 : 1.0;
    }

    inline bool isLargeJump(double absDx, double absDy) {
        return absDx > kLargeJumpDistance || absDy > kLargeJumpDistance;
    }

    inline bool isZeroDelta(double absDx, double absDy) {
        return absDx <= kEpsilon && absDy <= kEpsilon;
    }

    inline bool isWaveMovementCandidate(double absDx, double absDy, double ratio) {
        if (absDx <= kEpsilon || absDy <= kEpsilon) {
            return false;
        }

        return std::abs(absDy - ratio * absDx) <= kSlopeTolerance * absDx;
    }
}

$on_mod(Loaded) {
    wavefix::g_fixEnabled = Mod::get()->getSettingValue<bool>(wavefix::kEnabledSetting);
    wavefix::g_loggingEnabled = Mod::get()->getSettingValue<bool>(wavefix::kLoggingSetting);
    wavefix::g_stateLoggingEnabled = Mod::get()->getSettingValue<bool>(wavefix::kStateLoggingSetting);
    wavefix::g_visualizerEnabled = Mod::get()->getSettingValue<bool>(wavefix::kVisualizerSetting);

    listenForSettingChanges<bool>(wavefix::kEnabledSetting, [](bool value) {
        wavefix::g_fixEnabled = value;
    });
    listenForSettingChanges<bool>(wavefix::kLoggingSetting, [](bool value) {
        wavefix::g_loggingEnabled = value;
    });
    listenForSettingChanges<bool>(wavefix::kStateLoggingSetting, [](bool value) {
        wavefix::g_stateLoggingEnabled = value;
    });
    listenForSettingChanges<bool>(wavefix::kVisualizerSetting, [](bool value) {
        wavefix::g_visualizerEnabled = value;
    });
}
