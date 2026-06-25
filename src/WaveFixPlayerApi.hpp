#pragma once

class PlayerObject;
class GJBaseGameLayer;

namespace wavefix {
    bool onTeleportPlayerBegin(PlayerObject* player);
    void onTeleportPlayerEnd(PlayerObject* player, bool wrapped);
    void tickTeleportBypass(GJBaseGameLayer* layer);
    void tickWaveTrailRestart(GJBaseGameLayer* layer);

    bool getVisualizerDelta(PlayerObject* player, double& outDx, double& outDy);
    void updateVisualizer(GJBaseGameLayer* layer);
}
