#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>

#include <cmath>

using namespace geode::prelude;

namespace {
    constexpr double kMiniWaveScale = 1.0;
    constexpr double kZeroEpsilon = 1e-12;
    constexpr char const* kEnabledSetting = "enabled";

    bool waveFixEnabled() {
        return Mod::get()->getSettingValue<bool>(kEnabledSetting);
    }

    bool fixedWaveYVelocity(PlayerObject* player, double requestedVelocity, double& fixedVelocity) {
        if (!waveFixEnabled()) {
            return false;
        }

        if (!player->m_isDart || player->m_isDead || std::abs(requestedVelocity) <= kZeroEpsilon) {
            return false;
        }

        auto xVelocity = std::abs(player->getCurrentXVelocity());
        if (xVelocity <= kZeroEpsilon) {
            return false;
        }

        auto ratio = player->m_vehicleSize < kMiniWaveScale ? 2.0 : 1.0;
        fixedVelocity = std::copysign(xVelocity * ratio, requestedVelocity);
        return true;
    }
}

class $modify(WaveFixPlayerObject, PlayerObject) {
    void setYVelocity(double velocity, int type) {
        double fixedVelocity = 0.0;
        if (fixedWaveYVelocity(this, velocity, fixedVelocity)) {
            m_yVelocity = fixedVelocity;
            return;
        }

        PlayerObject::setYVelocity(velocity, type);
    }

    void toggleDartMode(bool enable, bool noEffects) {
        PlayerObject::toggleDartMode(enable, noEffects);

        if (!enable) {
            return;
        }

        double fixedVelocity = 0.0;
        if (fixedWaveYVelocity(this, m_yVelocity, fixedVelocity)) {
            m_yVelocity = fixedVelocity;
        }
    }
};
