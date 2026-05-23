#include <Geode/Geode.hpp>
#include <Geode/binding/GJBaseGameLayer.hpp>
#include <Geode/binding/HardStreak.hpp>
#include <Geode/modify/PlayerObject.hpp>

#include <cmath>

using namespace geode::prelude;

namespace {
    constexpr char const* kEnabledSetting = "enabled";
    constexpr char const* kLoggingSetting = "logging";
    constexpr char const* kStateLoggingSetting = "stateLogging";
    constexpr double kEpsilon = 0.000001;
    constexpr double kSlopeTolerance = 0.03;
    constexpr double kAnchorDriftTolerance = 0.125;
    constexpr double kLargeJumpDistance = 60.0; // 2 grid spaces in the editor.

    bool g_fixEnabled = true;
    bool g_loggingEnabled = false;
    bool g_stateLoggingEnabled = false;

    // Probably not needed but it's like 1 cpu cycle faster.
#define WAVEFIX_LOG_FIX(...)   do { if (g_loggingEnabled)      [[unlikely]] ::geode::log::info(__VA_ARGS__); } while (0)
#define WAVEFIX_LOG_STATE(...) do { if (g_stateLoggingEnabled) [[unlikely]] ::geode::log::info(__VA_ARGS__); } while (0)

    $on_mod(Loaded) {
        g_fixEnabled = Mod::get()->getSettingValue<bool>(kEnabledSetting);
        g_loggingEnabled = Mod::get()->getSettingValue<bool>(kLoggingSetting);
        g_stateLoggingEnabled = Mod::get()->getSettingValue<bool>(kStateLoggingSetting);

        listenForSettingChanges<bool>(kEnabledSetting, [](bool value) {
            g_fixEnabled = value;
        });
        listenForSettingChanges<bool>(kLoggingSetting, [](bool value) {
            g_loggingEnabled = value;
        });
        listenForSettingChanges<bool>(kStateLoggingSetting, [](bool value) {
            g_stateLoggingEnabled = value;
        });
    }

    double waveRatio(float vehicleSize) {
        return vehicleSize < 1.0f ? 2.0 : 1.0;
    }

    bool isLargeJump(double absDx, double absDy) {
        return absDx > kLargeJumpDistance || absDy > kLargeJumpDistance;
    }

    bool isZeroDelta(double absDx, double absDy) {
        return absDx <= kEpsilon && absDy <= kEpsilon;
    }

    bool isWaveMovementCandidate(double absDx, double absDy, double ratio) {
        if (absDx <= kEpsilon || absDy <= kEpsilon) {
            return false;
        }

        return std::abs(absDy - ratio * absDx) <= kSlopeTolerance * absDx;
    }
}

class $modify(WaveFixPlayerObject, PlayerObject) {
    struct Fields {
        bool anchorValid = false;
        bool slopeStateActive = false;
        double anchorX = 0.0;
        double anchorY = 0.0;
        double direction = 0.0;
        double xDirection = 0.0;
        double ratio = 1.0;
    };

    bool isLocalGameplayPlayer() {
        auto* gl = GJBaseGameLayer::get();
        if (gl == nullptr) {
            return false;
        }
        return this == gl->m_player1 || this == gl->m_player2;
    }

    // The bindings for sideways are too goofy and I'm not rewriting RobTop's code.
    bool shouldFixWave() {
        return g_fixEnabled && m_isDart && !m_isDead && !m_isSideways && isLocalGameplayPlayer();
    }

    void clearAnchor(char const* reason = "unspecified") {
        if (m_fields->anchorValid) {
            WAVEFIX_LOG_STATE(
                "anchor cleared reason={} prevAnchor=({}, {}) ratio={} direction={} xDirection={}",
                reason,
                m_fields->anchorX,
                m_fields->anchorY,
                m_fields->ratio,
                m_fields->direction,
                m_fields->xDirection
            );
        }
        m_fields->anchorValid = false;
        m_fields->slopeStateActive = false;
        m_fields->anchorX = 0.0;
        m_fields->anchorY = 0.0;
        m_fields->direction = 0.0;
        m_fields->xDirection = 0.0;
        m_fields->ratio = 1.0;
    }

    double currentXDirection() {
        return m_isGoingLeft ? -1.0 : 1.0;
    }

    bool isSlopeStateActive() {
        return (
            m_isCollidingWithSlope ||
            m_isOnSlope ||
            m_slopeSlidingMaybeRotated ||
            m_currentSlope != nullptr ||
            m_currentSlope2 != nullptr
        );
    }

    void resetWaveTrail(char const* reason = "unspecified") {
        WAVEFIX_LOG_STATE(
            "waveTrail reset reason={} pos=({}, {}) hasTrail={}",
            reason,
            m_position.x,
            m_position.y,
            m_waveTrail != nullptr
        );
        if (m_waveTrail != nullptr) {
            m_waveTrail->reset();
        }
    }

    void seedAnchor(cocos2d::CCPoint const& position, double ratio, double direction, double xDirection, char const* reason = "unspecified") {
        WAVEFIX_LOG_STATE(
            "anchor seed reason={} pos=({}, {}) ratio={} direction={} xDirection={} reseed={}",
            reason,
            position.x,
            position.y,
            ratio,
            direction,
            xDirection,
            m_fields->anchorValid
        );
        m_fields->anchorValid = true;
        m_fields->anchorX = static_cast<double>(position.x);
        m_fields->anchorY = static_cast<double>(position.y);
        m_fields->direction = direction;
        m_fields->xDirection = xDirection;
        m_fields->ratio = ratio;
    }

    void logCorrection(
        Fields const* f,
        cocos2d::CCPoint const& current,
        cocos2d::CCPoint const& requested,
        cocos2d::CCPoint const& corrected
    ) {
        WAVEFIX_LOG_FIX(
            "corrected current=({}, {}) requested=({}, {}) corrected=({}, {}) anchor=({}, {}) ratio={} direction={} xDirection={}",
            current.x,
            current.y,
            requested.x,
            requested.y,
            corrected.x,
            corrected.y,
            f->anchorX,
            f->anchorY,
            f->ratio,
            f->direction,
            f->xDirection
        );
    }

    void setPosition(cocos2d::CCPoint const& position) {
        if (!shouldFixWave()) {
            clearAnchor("shouldFixWave=false");
            PlayerObject::setPosition(position);
            return;
        }

        auto* f = m_fields.operator->();
        auto current = m_position;
        auto deltaX = static_cast<double>(position.x) - static_cast<double>(current.x);
        auto deltaY = static_cast<double>(position.y) - static_cast<double>(current.y);
        auto absDx = std::abs(deltaX);
        auto absDy = std::abs(deltaY);

        // Ignore idle frame, probably doesn't happen in normal mode but maybe in platformer mode.
        if (isZeroDelta(absDx, absDy)) {
            PlayerObject::setPosition(position);
            return;
        }

        auto currentSlopeState = isSlopeStateActive();
        if (currentSlopeState && !f->slopeStateActive) {
            resetWaveTrail("slope-enter");
        }
        f->slopeStateActive = currentSlopeState;

        auto ratio = waveRatio(m_vehicleSize);
        auto xDirection = currentXDirection();

        if (isLargeJump(absDx, absDy) || m_wasTeleported) {
            char const* reason = m_wasTeleported ? "teleport" : "large-jump";
            PlayerObject::setPosition(position);
            seedAnchor(m_position, ratio, 0.0, xDirection, reason);
            resetWaveTrail(reason);
            return;
        }

        if (absDy <= kEpsilon) {
            PlayerObject::setPosition(position);
            return;
        }

        if (!isWaveMovementCandidate(absDx, absDy, ratio)) {
            PlayerObject::setPosition(position);
            if (!f->anchorValid) {
                seedAnchor(m_position, ratio, 0.0, xDirection, "non-wave-move");
            }
            return;
        }

        // Btw this is for like frame extrapolation mods and stuff.
        if (deltaX * xDirection <= kEpsilon) {
            PlayerObject::setPosition(position);
            return;
        }

        auto direction = std::copysign(1.0, deltaY);
        auto currentX = static_cast<double>(current.x);
        auto currentY = static_cast<double>(current.y);

        char const* reseedReason = nullptr;
        if (!f->anchorValid)                  reseedReason = "reseed-invalid";
        else if (f->ratio != ratio)           reseedReason = "reseed-ratio";
        else if (f->xDirection != xDirection) reseedReason = "reseed-xdir";
        else {
            auto signedXTravel = (currentX - f->anchorX) * xDirection;
            if (signedXTravel < -kAnchorDriftTolerance) {
                reseedReason = "reseed-xback";
            } else if (f->direction != 0.0) {
                auto slope = f->direction * xDirection * ratio;
                auto expectedY = f->anchorY + slope * (currentX - f->anchorX);
                if (std::abs(currentY - expectedY) > kAnchorDriftTolerance) {
                    reseedReason = "reseed-drift";
                } else if (f->direction != direction) {
                    reseedReason = "reseed-flip";
                }
            }
        }

        if (reseedReason != nullptr) {
            seedAnchor(current, ratio, direction, xDirection, reseedReason);
        } else if (f->direction == 0.0) {
            f->direction = direction;
        }

        auto slope = direction * xDirection * ratio;
        auto correctedY = f->anchorY + slope * (static_cast<double>(position.x) - f->anchorX);
        auto corrected = cocos2d::CCPoint(position.x, static_cast<float>(correctedY));

        logCorrection(f, current, position, corrected);
        PlayerObject::setPosition(corrected);
    }

    void setYVelocity(double velocity, int type) {
        if (!shouldFixWave() || std::abs(velocity) <= kEpsilon) {
            PlayerObject::setYVelocity(velocity, type);
            return;
        }

        auto xVelocity = getCurrentXVelocity();

        if (std::abs(xVelocity) <= kEpsilon) {
            PlayerObject::setYVelocity(velocity, type);
            return;
        }

        auto ratio = waveRatio(m_vehicleSize);
        auto adjustedVelocity = std::copysign(std::abs(xVelocity) * ratio, velocity);

        WAVEFIX_LOG_FIX(
            "yVelocity requested={} fixed={} xVelocity={} ratio={} type={}",
            velocity,
            adjustedVelocity,
            xVelocity,
            ratio,
            type
        );

        PlayerObject::setYVelocity(adjustedVelocity, type);
    }

    void toggleDartMode(bool enable, bool noEffects) {
        WAVEFIX_LOG_STATE("toggleDartMode enable={} noEffects={} isDart={} isDead={}", enable, noEffects, m_isDart, m_isDead);

        PlayerObject::toggleDartMode(enable, noEffects);

        clearAnchor("toggle-dart");

        if (enable && m_isDart && !m_isDead && !m_isSideways && isLocalGameplayPlayer()) {
            seedAnchor(m_position, waveRatio(m_vehicleSize), 0.0, currentXDirection(), "toggle-dart");
        }
    }
};
