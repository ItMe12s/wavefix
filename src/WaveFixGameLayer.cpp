#include "WaveFixPlayerApi.hpp"

#include <Geode/binding/TeleportPortalObject.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

class $modify(WaveFixGameLayer, GJBaseGameLayer) {
    void teleportPlayer(TeleportPortalObject* object, PlayerObject* player) {
        auto wrapped = wavefix::onTeleportPlayerBegin(player);
        GJBaseGameLayer::teleportPlayer(object, player);
        wavefix::onTeleportPlayerEnd(player, wrapped);
    }

    void update(float dt) {
        wavefix::tickTeleportBypass(this);
        GJBaseGameLayer::update(dt);
        wavefix::tickWaveTrailRestart(this);
    }
};
