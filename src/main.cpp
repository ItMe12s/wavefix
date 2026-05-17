#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>

#include <cmath>

using namespace geode::prelude;

namespace {
    constexpr char const* kEnabledSetting = "enabled";
    constexpr char const* kLoggingSetting = "logging";
    constexpr double kEpsilon = 0.000001;
    constexpr double kSlopeTolerance = 0.03;
    constexpr double kLargeJumpDistance = 64.0;

    bool fixEnabled() {
        return Mod::get()->getSettingValue<bool>(kEnabledSetting);
    }

    bool loggingEnabled() {
        return Mod::get()->getSettingValue<bool>(kLoggingSetting);
    }

    double signOf(double value) {
        return value < 0.0 ? -1.0 : 1.0;
    }

    double waveRatio(float vehicleSize) {
        return vehicleSize < 1.0f ? 2.0 : 1.0;
    }

    bool nearlyEqual(double left, double right) {
        return std::abs(left - right) <= kEpsilon;
    }

    bool isLargeJump(double deltaX, double deltaY) {
        return std::abs(deltaX) > kLargeJumpDistance || std::abs(deltaY) > kLargeJumpDistance;
    }

    bool isZeroDelta(double deltaX, double deltaY) {
        return std::abs(deltaX) <= kEpsilon && std::abs(deltaY) <= kEpsilon;
    }

    bool isWaveMovementCandidate(double deltaX, double deltaY, double ratio) {
        if (std::abs(deltaX) <= kEpsilon || std::abs(deltaY) <= kEpsilon || isLargeJump(deltaX, deltaY)) {
            return false;
        }

        auto actualRatio = std::abs(deltaY / deltaX);
        return std::abs(actualRatio - ratio) <= kSlopeTolerance;
    }
}

class $modify(WaveFixPlayerObject, PlayerObject) {
    struct Fields {
        bool anchorValid = false;
        double anchorX = 0.0;
        double anchorY = 0.0;
        double direction = 0.0;
        double ratio = 1.0;
    };

    bool shouldFixWave() {
        return fixEnabled() && m_isDart && !m_isDead;
    }

    void clearAnchor() {
        m_fields->anchorValid = false;
        m_fields->anchorX = 0.0;
        m_fields->anchorY = 0.0;
        m_fields->direction = 0.0;
        m_fields->ratio = waveRatio(m_vehicleSize);
    }

    void seedAnchor(cocos2d::CCPoint const& position, double ratio, double direction) {
        m_fields->anchorValid = true;
        m_fields->anchorX = static_cast<double>(position.x);
        m_fields->anchorY = static_cast<double>(position.y);
        m_fields->direction = direction;
        m_fields->ratio = ratio;
    }

    void logCorrection(
        cocos2d::CCPoint const& current,
        cocos2d::CCPoint const& requested,
        cocos2d::CCPoint const& corrected,
        double ratio,
        double direction
    ) {
        if (!loggingEnabled()) {
            return;
        }

        log::info(
            "corrected current=({}, {}) requested=({}, {}) corrected=({}, {}) anchor=({}, {}) ratio={} direction={}",
            current.x,
            current.y,
            requested.x,
            requested.y,
            corrected.x,
            corrected.y,
            m_fields->anchorX,
            m_fields->anchorY,
            ratio,
            direction
        );
    }

    void setPosition(cocos2d::CCPoint const& position) {
        if (!shouldFixWave()) {
            clearAnchor();
            PlayerObject::setPosition(position);
            return;
        }

        auto current = m_position;
        auto deltaX = static_cast<double>(position.x) - current.x;
        auto deltaY = static_cast<double>(position.y) - current.y;
        auto ratio = waveRatio(m_vehicleSize);

        if (isZeroDelta(deltaX, deltaY)) {
            PlayerObject::setPosition(position);
            return;
        }

        if (!isWaveMovementCandidate(deltaX, deltaY, ratio)) {
            PlayerObject::setPosition(position);
            seedAnchor(position, ratio, 0.0);
            return;
        }

        auto direction = signOf(deltaY);

        if (
            !m_fields->anchorValid ||
            !nearlyEqual(m_fields->ratio, ratio) ||
            (std::abs(m_fields->direction) > kEpsilon && !nearlyEqual(m_fields->direction, direction))
        ) {
            seedAnchor(current, ratio, direction);
        }

        m_fields->direction = direction;
        m_fields->ratio = ratio;

        auto correctedY = m_fields->anchorY + direction * std::abs(static_cast<double>(position.x) - m_fields->anchorX) * ratio;
        auto corrected = cocos2d::CCPoint(position.x, static_cast<float>(correctedY));

        logCorrection(current, position, corrected, ratio, direction);
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

        m_yVelocity = std::copysign(std::abs(xVelocity) * waveRatio(m_vehicleSize), velocity);

        if (loggingEnabled()) {
            log::info(
                "yVelocity requested={} fixed={} xVelocity={} ratio={} type={}",
                velocity,
                m_yVelocity,
                xVelocity,
                waveRatio(m_vehicleSize),
                type
            );
        }
    }

    void toggleDartMode(bool enable, bool noEffects) {
        PlayerObject::toggleDartMode(enable, noEffects);

        clearAnchor();

        if (enable && m_isDart && !m_isDead) {
            seedAnchor(m_position, waveRatio(m_vehicleSize), 0.0);
        }
    }
};
