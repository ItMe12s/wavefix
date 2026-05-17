#include <Geode/Geode.hpp>
#include <Geode/binding/HardStreak.hpp>
#include <Geode/modify/PlayerObject.hpp>

#include <cmath>

using namespace geode::prelude;

namespace {
    constexpr char const* kEnabledSetting = "enabled";
    constexpr char const* kLoggingSetting = "logging";
    constexpr double kEpsilon = 0.000001;
    constexpr double kSlopeTolerance = 0.03;
    constexpr double kAnchorDriftTolerance = 0.125;
    constexpr double kLargeJumpDistance = 60.0; // 2 grid spaces in the editor.

    bool fixEnabled() {
        return Mod::get()->getSettingValue<bool>(kEnabledSetting);
    }

    bool loggingEnabled() {
        return Mod::get()->getSettingValue<bool>(kLoggingSetting);
    }

    double signOf(double value) {
        if (value > 0.0) {
            return 1.0;
        }

        if (value < 0.0) {
            return -1.0;
        }

        return 0.0;
    }

    double waveRatio(float vehicleSize) {
        return vehicleSize < 1.0f ? 2.0 : 1.0;
    }

    bool isLargeJump(double deltaX, double deltaY) {
        return std::abs(deltaX) > kLargeJumpDistance || std::abs(deltaY) > kLargeJumpDistance;
    }

    bool isZeroDelta(double deltaX, double deltaY) {
        return std::abs(deltaX) <= kEpsilon && std::abs(deltaY) <= kEpsilon;
    }

    bool isWaveMovementCandidate(double deltaX, double deltaY, double ratio) {
        if (std::abs(deltaX) <= kEpsilon || std::abs(deltaY) <= kEpsilon) {
            return false;
        }

        auto actualRatio = std::abs(deltaY / deltaX);
        return std::abs(actualRatio - ratio) <= kSlopeTolerance;
    }

    double projectedY(double anchorX, double anchorY, double positionX, double direction, double xDirection, double ratio) {
        auto signedXTravel = (positionX - anchorX) * xDirection;
        return anchorY + direction * signedXTravel * ratio;
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

    bool shouldFixWave() {
        return fixEnabled() && m_isDart && !m_isDead;
    }

    void clearAnchor() {
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
            m_currentSlope2 != nullptr ||
            m_currentPotentialSlope != nullptr
        );
    }

    void resetWaveTrail() {
        if (m_waveTrail != nullptr) {
            m_waveTrail->reset();
        }
    }

    bool anchorMatchesCurrent(cocos2d::CCPoint const& current, double ratio, double xDirection) {
        if (m_fields->direction == 0.0) {
            return true;
        }

        auto expectedY = projectedY(
            m_fields->anchorX,
            m_fields->anchorY,
            static_cast<double>(current.x),
            m_fields->direction,
            xDirection,
            ratio
        );

        return std::abs(static_cast<double>(current.y) - expectedY) <= kAnchorDriftTolerance;
    }

    bool anchorSupportsCurrentX(cocos2d::CCPoint const& current, double xDirection) {
        if (!m_fields->anchorValid) {
            return true;
        }

        auto signedXTravel = (static_cast<double>(current.x) - m_fields->anchorX) * xDirection;
        return signedXTravel >= -kAnchorDriftTolerance;
    }

    void seedAnchor(cocos2d::CCPoint const& position, double ratio, double direction, double xDirection) {
        m_fields->anchorValid = true;
        m_fields->anchorX = static_cast<double>(position.x);
        m_fields->anchorY = static_cast<double>(position.y);
        m_fields->direction = direction;
        m_fields->xDirection = xDirection;
        m_fields->ratio = ratio;
    }

    void logCorrection(
        cocos2d::CCPoint const& current,
        cocos2d::CCPoint const& requested,
        cocos2d::CCPoint const& corrected
    ) {
        if (!loggingEnabled()) {
            return;
        }

        log::info(
            "corrected current=({}, {}) requested=({}, {}) corrected=({}, {}) anchor=({}, {}) ratio={} direction={} xDirection={}",
            current.x,
            current.y,
            requested.x,
            requested.y,
            corrected.x,
            corrected.y,
            m_fields->anchorX,
            m_fields->anchorY,
            m_fields->ratio,
            m_fields->direction,
            m_fields->xDirection
        );
    }

    void setPosition(cocos2d::CCPoint const& position) {
        if (!shouldFixWave()) {
            clearAnchor();
            PlayerObject::setPosition(position);
            return;
        }

        auto current = m_position;
        auto deltaX = static_cast<double>(position.x) - static_cast<double>(current.x);
        auto deltaY = static_cast<double>(position.y) - static_cast<double>(current.y);
        auto ratio = waveRatio(m_vehicleSize);
        auto xDirection = currentXDirection();

        // Ignore idle frame, probably doesn't happen in normal mode but maybe in platformer mode.
        if (isZeroDelta(deltaX, deltaY)) {
            PlayerObject::setPosition(position);
            return;
        }

        auto currentSlopeState = isSlopeStateActive();

        if (currentSlopeState && !m_fields->slopeStateActive) {
            resetWaveTrail();
        }

        m_fields->slopeStateActive = currentSlopeState;

        if (isLargeJump(deltaX, deltaY) || m_wasTeleported) {
            PlayerObject::setPosition(position);
            seedAnchor(m_position, ratio, 0.0, xDirection);
            resetWaveTrail();
            return;
        }

        if (!isWaveMovementCandidate(deltaX, deltaY, ratio)) {
            PlayerObject::setPosition(position);
            seedAnchor(m_position, ratio, 0.0, xDirection);
            return;
        }

        auto direction = signOf(deltaY);

        if (
            !m_fields->anchorValid ||
            m_fields->ratio != ratio ||
            m_fields->xDirection != xDirection ||
            !anchorSupportsCurrentX(current, xDirection) ||
            !anchorMatchesCurrent(current, ratio, xDirection) ||
            (m_fields->direction != 0.0 && m_fields->direction != direction)
        ) {
            seedAnchor(current, ratio, direction, xDirection);
        } else if (m_fields->direction == 0.0) {
            m_fields->direction = direction;
        }

        auto correctedY = projectedY(
            m_fields->anchorX,
            m_fields->anchorY,
            static_cast<double>(position.x),
            direction,
            xDirection,
            ratio
        );
        auto corrected = cocos2d::CCPoint(position.x, static_cast<float>(correctedY));

        logCorrection(current, position, corrected);
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

        auto adjustedVelocity = std::copysign(std::abs(xVelocity) * waveRatio(m_vehicleSize), velocity);

        if (loggingEnabled()) {
            log::info(
                "yVelocity requested={} fixed={} xVelocity={} ratio={} type={}",
                velocity,
                adjustedVelocity,
                xVelocity,
                waveRatio(m_vehicleSize),
                type
            );
        }

        PlayerObject::setYVelocity(adjustedVelocity, type);
    }

    void toggleDartMode(bool enable, bool noEffects) {
        PlayerObject::toggleDartMode(enable, noEffects);

        clearAnchor();

        if (enable && m_isDart && !m_isDead) {
            seedAnchor(m_position, waveRatio(m_vehicleSize), 0.0, currentXDirection());
        }
    }
};
