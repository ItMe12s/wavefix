#include "WaveFixConfig.hpp"
#include "WaveFixPlayerApi.hpp"

#include <Geode/binding/GJBaseGameLayer.hpp>
#include <Geode/binding/HardStreak.hpp>
#include <Geode/modify/PlayerObject.hpp>

using namespace geode::prelude;

class $modify(WaveFixPlayerObject, PlayerObject) {
    struct Fields {
        bool anchorValid           = false;
        int specialMoveDepth       = 0;
        int teleportBypassFrames   = 0;
        int waveTrailRestartFrames = 0;
        char const* waveTrailRestartReason = "unspecified";

        double anchorX     = 0.0;
        double anchorY     = 0.0;
        double direction   = 0.0;
        double xDirection  = 0.0;
        double ratio       = 1.0;
        double visReqDx    = 0.0;
        double visReqDy    = 0.0;
        bool   visReqValid = false;
    };

    bool isLocalGameplayPlayer() {
        auto* gl = GJBaseGameLayer::get();
        if (gl == nullptr) {
            return false;
        }
        return this == gl->m_player1 || this == gl->m_player2;
    }

    bool shouldFixWave() {
        return wavefix::g_fixEnabled && m_isDart && !m_isDead && !m_isSideways && isLocalGameplayPlayer();
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
        m_fields->anchorValid      = false;
        m_fields->anchorX          = 0.0;
        m_fields->anchorY          = 0.0;
        m_fields->direction        = 0.0;
        m_fields->xDirection       = 0.0;
        m_fields->ratio            = 1.0;
        m_fields->visReqValid      = false;
    }

    double currentXDirection() {
        return m_isGoingLeft ? -1.0 : 1.0;
    }

    void armWaveTrailRestart(int frames, char const* reason) {
        auto previousFrames = m_fields->waveTrailRestartFrames;
        if (frames > previousFrames) {
            m_fields->waveTrailRestartFrames = frames;
            m_fields->waveTrailRestartReason = reason;
        }

        WAVEFIX_LOG_STATE(
            "waveTrail restart armed reason={} prevFrames={} frames={} nextFrames={}",
            reason,
            previousFrames,
            frames,
            m_fields->waveTrailRestartFrames
        );
    }

    void armWaveTrailRestart(char const* reason) {
        armWaveTrailRestart(wavefix::kWaveTrailRestartFrames, reason);
    }

    bool restartWaveTrail(char const* reason = "unspecified") {
        WAVEFIX_LOG_STATE(
            "waveTrail restart reason={} pos=({}, {}) hasTrail={} draw={}",
            reason,
            m_position.x,
            m_position.y,
            m_waveTrail != nullptr,
            m_waveTrail != nullptr ? m_waveTrail->m_drawStreak : false
        );
        if (m_waveTrail == nullptr) {
            return false;
        }

        m_waveTrail->reset();
        m_waveTrail->resumeStroke();
        placeStreakPoint();
        return true;
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
        m_fields->anchorX     = static_cast<double>(position.x);
        m_fields->anchorY     = static_cast<double>(position.y);
        m_fields->direction   = direction;
        m_fields->xDirection  = xDirection;
        m_fields->ratio       = ratio;
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

    bool isInSpecialMove() {
        return m_fields->specialMoveDepth > 0;
    }

    bool shouldBypassWaveFix() {
        return isInSpecialMove() || m_wasTeleported || m_fields->teleportBypassFrames > 0;
    }

public:
    void beginSpecialMove() {
        m_fields->specialMoveDepth += 1;
    }

    void endSpecialMove(char const* reason) {
        if (m_fields->specialMoveDepth > 0) {
            m_fields->specialMoveDepth -= 1;
        }

        if (m_fields->specialMoveDepth == 0 && shouldFixWave()) {
            seedAnchor(m_position, wavefix::waveRatio(m_vehicleSize), 0.0, currentXDirection(), reason);
            if (!m_isDashing) {
                armWaveTrailRestart(reason);
            }
        }
    }

    void armTeleportBypass(int frames, char const* reason) {
        auto previousFrames = m_fields->teleportBypassFrames;
        if (frames > previousFrames) {
            m_fields->teleportBypassFrames = frames;
        }

        WAVEFIX_LOG_STATE(
            "teleport bypass armed reason={} prevFrames={} frames={} nextFrames={}",
            reason,
            previousFrames,
            frames,
            m_fields->teleportBypassFrames
        );
    }

    void tickTeleportBypass() {
        if (m_fields->teleportBypassFrames <= 0) {
            return;
        }

        m_fields->teleportBypassFrames -= 1;
        WAVEFIX_LOG_STATE("teleport bypass tick remaining={}", m_fields->teleportBypassFrames);
    }

    void tickWaveTrailRestart() {
        if (m_fields->waveTrailRestartFrames <= 0) {
            return;
        }

        if (!shouldFixWave()) {
            m_fields->waveTrailRestartFrames = 0;
            m_fields->waveTrailRestartReason = "unspecified";
            return;
        }

        m_fields->waveTrailRestartFrames -= 1;
        if (m_fields->waveTrailRestartFrames > 0) {
            WAVEFIX_LOG_STATE("waveTrail restart tick remaining={}", m_fields->waveTrailRestartFrames);
            return;
        }

        restartWaveTrail(m_fields->waveTrailRestartReason);
        m_fields->waveTrailRestartReason = "unspecified";
    }

    bool isTeleportFixTarget() {
        return shouldFixWave();
    }

    bool visDelta(double& outDx, double& outDy) {
        if (!m_fields->visReqValid) {
            return false;
        }
        outDx = m_fields->visReqDx;
        outDy = m_fields->visReqDy;
        return true;
    }

    void setPosition(cocos2d::CCPoint const& position) {
        if (m_isDart && !m_isSideways && !m_isDead && isLocalGameplayPlayer()) {
            auto dx = static_cast<double>(position.x) - static_cast<double>(m_position.x);
            auto dy = static_cast<double>(position.y) - static_cast<double>(m_position.y);
            if (!wavefix::isZeroDelta(std::abs(dx), std::abs(dy))) {
                m_fields->visReqDx    = dx;
                m_fields->visReqDy    = dy;
                m_fields->visReqValid = true;
            }
        }

        if (!shouldFixWave()) {
            clearAnchor("shouldFixWave=false");
            PlayerObject::setPosition(position);
            return;
        }

        if (shouldBypassWaveFix()) {
            PlayerObject::setPosition(position);
            if (!isInSpecialMove() && !m_isDashing) {
                seedAnchor(m_position, wavefix::waveRatio(m_vehicleSize), 0.0, currentXDirection(), "teleport-bypass");
                armWaveTrailRestart("teleport-bypass");
            }
            return;
        }

        if (m_isDashing) {
            PlayerObject::setPosition(position);
            return;
        }

        auto* f = m_fields.operator->();
        auto current = m_position;
        auto deltaX  = static_cast<double>(position.x) - static_cast<double>(current.x);
        auto deltaY  = static_cast<double>(position.y) - static_cast<double>(current.y);
        auto absDx   = std::abs(deltaX);
        auto absDy   = std::abs(deltaY);

        if (wavefix::isZeroDelta(absDx, absDy)) {
            PlayerObject::setPosition(position);
            return;
        }

        auto ratio = wavefix::waveRatio(m_vehicleSize);
        auto xDirection = currentXDirection();

        if (wavefix::isLargeJump(absDx, absDy)) {
            char const* reason = "large-jump";
            PlayerObject::setPosition(position);
            seedAnchor(m_position, ratio, 0.0, xDirection, reason);
            return;
        }

        if (absDx <= wavefix::kEpsilon && absDy > wavefix::kEpsilon) {
            PlayerObject::setPosition(position);
            seedAnchor(m_position, ratio, 0.0, xDirection, "vertical-reseed");
            return;
        }

        if (absDy <= wavefix::kEpsilon) {
            PlayerObject::setPosition(position);
            return;
        }

        if (!wavefix::isWaveMovementCandidate(absDx, absDy, ratio)) {
            PlayerObject::setPosition(position);
            if (!f->anchorValid) {
                seedAnchor(m_position, ratio, 0.0, xDirection, "non-wave-move");
            }
            return;
        }

        if (deltaX * xDirection <= wavefix::kEpsilon) {
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
            if (signedXTravel < -wavefix::kAnchorDriftTolerance) {
                reseedReason = "reseed-xback";
            } else if (f->direction != 0.0) {
                auto slope = f->direction * xDirection * ratio;
                auto expectedY = f->anchorY + slope * (currentX - f->anchorX);
                if (std::abs(currentY - expectedY) > wavefix::kAnchorDriftTolerance) {
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
        if (!wavefix::g_fixMomentumEnabled || shouldBypassWaveFix() || m_isDashing
            || !shouldFixWave() || std::abs(velocity) <= wavefix::kEpsilon) {
            PlayerObject::setYVelocity(velocity, type);
            return;
        }

        auto xVelocity = getCurrentXVelocity();

        if (std::abs(xVelocity) <= wavefix::kEpsilon) {
            PlayerObject::setYVelocity(velocity, type);
            return;
        }

        auto ratio = wavefix::waveRatio(m_vehicleSize);
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
        m_fields->visReqValid = false;

        if (enable && m_isDart && !m_isDead && !m_isSideways && isLocalGameplayPlayer()) {
            seedAnchor(m_position, wavefix::waveRatio(m_vehicleSize), 0.0, currentXDirection(), "toggle-dart");
        }
    }

    void stopDashing() {
        PlayerObject::stopDashing();

        if (shouldFixWave()) {
            seedAnchor(m_position, wavefix::waveRatio(m_vehicleSize), 0.0, currentXDirection(), "dash-release");
        }
    }

    // void ringJump(RingObject* object, bool skipCheck) {
    //     beginSpecialMove();
    //     PlayerObject::ringJump(object, skipCheck);
    //     endSpecialMove("ring-jump");
    // }

    void spiderTestJump(bool dynamic) {
        beginSpecialMove();
        PlayerObject::spiderTestJump(dynamic);
        endSpecialMove("spider-test-jump");
    }

    void spiderTestJumpInternal(bool dynamic) {
        beginSpecialMove();
        PlayerObject::spiderTestJumpInternal(dynamic);
        endSpecialMove("spider-test-jump-internal");
    }

    // void bumpPlayer(float bumpMod, int objectType, bool noEffects, GameObject* object) {
    //     beginSpecialMove();
    //     PlayerObject::bumpPlayer(bumpMod, objectType, noEffects, object);
    //     endSpecialMove("bump-player");
    // }

    // void propellPlayer(float yVelocity, bool noEffects, int objectType) {
    //     beginSpecialMove();
    //     PlayerObject::propellPlayer(yVelocity, noEffects, objectType);
    //     endSpecialMove("propell-player");
    // }
};

namespace wavefix {
    static WaveFixPlayerObject* asWaveFixPlayer(PlayerObject* player) {
        return player != nullptr ? geode::cast::modify_cast<WaveFixPlayerObject*>(player) : nullptr;
    }

    bool onTeleportPlayerBegin(PlayerObject* player) {
        auto* wavePlayer = asWaveFixPlayer(player);
        auto wrapped = wavePlayer != nullptr && wavePlayer->isTeleportFixTarget();
        if (wrapped) {
            wavePlayer->beginSpecialMove();
        }
        return wrapped;
    }

    void onTeleportPlayerEnd(PlayerObject* player, bool wrapped) {
        if (!wrapped) {
            return;
        }

        auto* wavePlayer = asWaveFixPlayer(player);
        if (wavePlayer == nullptr) {
            return;
        }

        wavePlayer->endSpecialMove("teleport-player");
        wavePlayer->armTeleportBypass(2, "teleport-player");
    }

    static void tickPlayerTeleportBypass(PlayerObject* player) {
        if (auto* wavePlayer = asWaveFixPlayer(player)) {
            wavePlayer->tickTeleportBypass();
        }
    }

    static void tickPlayerWaveTrailRestart(PlayerObject* player) {
        if (auto* wavePlayer = asWaveFixPlayer(player)) {
            wavePlayer->tickWaveTrailRestart();
        }
    }

    void tickTeleportBypass(GJBaseGameLayer* layer) {
        if (layer == nullptr) {
            return;
        }
        tickPlayerTeleportBypass(layer->m_player1);
        tickPlayerTeleportBypass(layer->m_player2);
    }

    void tickWaveTrailRestart(GJBaseGameLayer* layer) {
        if (layer == nullptr) {
            return;
        }
        tickPlayerWaveTrailRestart(layer->m_player1);
        tickPlayerWaveTrailRestart(layer->m_player2);
    }

    bool getVisualizerDelta(PlayerObject* player, double& outDx, double& outDy) {
        auto* wavePlayer = asWaveFixPlayer(player);
        if (wavePlayer == nullptr) {
            return false;
        }
        return wavePlayer->visDelta(outDx, outDy);
    }
}
