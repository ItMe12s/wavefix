#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>

#include <cmath>

using namespace geode::prelude;

namespace {
    constexpr char const* kLoggingSetting = "logging";
    constexpr int kMaxMovementLogs = 240;
    constexpr int kMaxCollisionLogs = 80;

    bool loggingEnabled() {
        return Mod::get()->getSettingValue<bool>(kLoggingSetting);
    }

    char const* boolText(bool value) {
        return value ? "true" : "false";
    }

    double safeRatio(double deltaX, double deltaY) {
        if (std::abs(deltaX) <= 0.000001) {
            return 0.0;
        }

        return deltaY / deltaX;
    }

    int objectID(GameObject* object) {
        return object ? object->m_objectID : 0;
    }
}

class $modify(WaveFixPlayerObject, PlayerObject) {
    struct Fields {
        int movementLogs = 0;
        int collisionLogs = 0;
    };

    bool shouldLogMovement() {
        return loggingEnabled() && m_isDart && m_fields->movementLogs++ < kMaxMovementLogs;
    }

    bool shouldLogCollisionBlock(int lines) {
        if (!loggingEnabled() || !m_isDart || m_fields->collisionLogs + lines > kMaxCollisionLogs) {
            return false;
        }

        m_fields->collisionLogs += lines;
        return true;
    }

    void logPlayerState(char const* tag, float dt = 0.0f) {
        log::info(
            "[{}] dt={} dart={} dead={} upsideDown={} second={} vehicleSize={} playerSpeed={} speedMultiplier={} yVel={} xVel={} posDouble=({}, {}) posFloat=({}, {})",
            tag,
            dt,
            boolText(m_isDart),
            boolText(m_isDead),
            boolText(m_isUpsideDown),
            boolText(m_isSecondPlayer),
            m_vehicleSize,
            m_playerSpeed,
            m_speedMultiplier,
            m_yVelocity,
            getCurrentXVelocity(),
            m_positionX,
            m_positionY,
            m_position.x,
            m_position.y
        );
    }

    void update(float dt) {
        auto wasDart = m_isDart;

        PlayerObject::update(dt);

        if ((wasDart || m_isDart) && shouldLogMovement()) {
            logPlayerState("update", dt);
        }
    }

    void setPosition(cocos2d::CCPoint const& position) {
        if (shouldLogMovement()) {
            auto deltaX = static_cast<double>(position.x) - m_positionX;
            auto deltaY = static_cast<double>(position.y) - m_positionY;

            log::info(
                "[setPosition] requested=({}, {}) delta=({}, {}) ratio={} currentDouble=({}, {}) currentFloat=({}, {})",
                position.x,
                position.y,
                deltaX,
                deltaY,
                safeRatio(deltaX, deltaY),
                m_positionX,
                m_positionY,
                m_position.x,
                m_position.y
            );
        }

        PlayerObject::setPosition(position);
    }

    void setYVelocity(double velocity, int type) {
        if (shouldLogMovement()) {
            log::info(
                "[setYVelocity] requested={} type={} previousYVel={} xVel={} vehicleSize={}",
                velocity,
                type,
                m_yVelocity,
                getCurrentXVelocity(),
                m_vehicleSize
            );
        }

        PlayerObject::setYVelocity(velocity, type);
    }

    void toggleDartMode(bool enable, bool noEffects) {
        if (loggingEnabled()) {
            log::info(
                "[toggleDartMode:before] enable={} noEffects={} dart={} yVel={} xVel={} vehicleSize={} posDouble=({}, {})",
                boolText(enable),
                boolText(noEffects),
                boolText(m_isDart),
                m_yVelocity,
                getCurrentXVelocity(),
                m_vehicleSize,
                m_positionX,
                m_positionY
            );
        }

        PlayerObject::toggleDartMode(enable, noEffects);

        m_fields->movementLogs = 0;
        m_fields->collisionLogs = 0;

        if (loggingEnabled()) {
            log::info(
                "[toggleDartMode:after] enable={} dart={} dead={} upsideDown={} second={} yVel={} xVel={} vehicleSize={} posDouble=({}, {})",
                boolText(enable),
                boolText(m_isDart),
                boolText(m_isDead),
                boolText(m_isUpsideDown),
                boolText(m_isSecondPlayer),
                m_yVelocity,
                getCurrentXVelocity(),
                m_vehicleSize,
                m_positionX,
                m_positionY
            );
        }
    }

    bool collidedWithObject(float dt, GameObject* object, cocos2d::CCRect rect, bool skipCheck) {
        auto logBefore = shouldLogCollisionBlock(4);

        if (logBefore) {
            log::info(
                "[collidedWithObject:before] dt={} objectID={} skipCheck={} rect=({}, {}, {}, {})",
                dt,
                objectID(object),
                boolText(skipCheck),
                rect.origin.x,
                rect.origin.y,
                rect.size.width,
                rect.size.height
            );
            logPlayerState("collidedWithObject:before", dt);
        }

        auto result = PlayerObject::collidedWithObject(dt, object, rect, skipCheck);

        if (logBefore) {
            log::info("[collidedWithObject:after] result={}", boolText(result));
            logPlayerState("collidedWithObject:after", dt);
        }

        return result;
    }

    bool collidedWithObjectInternal(float dt, GameObject* object, cocos2d::CCRect rect, bool skipCheck) {
        auto logBefore = shouldLogCollisionBlock(4);

        if (logBefore) {
            log::info(
                "[collidedWithObjectInternal:before] dt={} objectID={} skipCheck={} rect=({}, {}, {}, {})",
                dt,
                objectID(object),
                boolText(skipCheck),
                rect.origin.x,
                rect.origin.y,
                rect.size.width,
                rect.size.height
            );
            logPlayerState("collidedWithObjectInternal:before", dt);
        }

        auto result = PlayerObject::collidedWithObjectInternal(dt, object, rect, skipCheck);

        if (logBefore) {
            log::info("[collidedWithObjectInternal:after] result={}", boolText(result));
            logPlayerState("collidedWithObjectInternal:after", dt);
        }

        return result;
    }
};
