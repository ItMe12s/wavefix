#include "WaveFixConfig.hpp"
#include "WaveFixPlayerApi.hpp"

#include <Geode/binding/GJBaseGameLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/Geode.hpp>

using namespace geode::prelude;

namespace wavefix {
    namespace {
        constexpr char const* kNodeId         = "wavefix-trajectory";
        constexpr char const* kFont           = "chatFont.fnt";
        constexpr float kLabelScale           = 0.5f;
        constexpr float kOffsetX              = 50.0f;
        constexpr float kVanillaOffsetY       = 25.0f;
        constexpr float kIdealOffsetY         = 15.0f;
        constexpr float kOffsetOffsetY        = 35.0f;
        constexpr double kRayLength           = 300.0;
        constexpr float kRayThickness         = 0.3f;
        constexpr cocos2d::ccColor3B kVanilla = {255, 60, 60};
        constexpr cocos2d::ccColor3B kIdeal   = {60, 255, 120};
        constexpr cocos2d::ccColor3B kOffset  = {60, 255, 255};
        constexpr int kAnglePrecision         = 14;
        constexpr int kNodeZOrder             = 6767;
        inline const std::string kAngleFmt    = fmt::format("{{:.{}f}}", kAnglePrecision);
        inline const std::string kOffsetFmt   = fmt::format("{{:+.{}f}}", kAnglePrecision);

        inline double slopeToDegrees(double absSlope) {
            return std::atan(absSlope) * 180.0 / M_PI;
        }

        class WaveFixTrajectoryNode;

        WaveFixTrajectoryNode* g_node = nullptr;

        struct PlayerLabels {
            cocos2d::CCLabelBMFont* vanilla = nullptr;
            cocos2d::CCLabelBMFont* ideal   = nullptr;
            cocos2d::CCLabelBMFont* offset  = nullptr;
        };

        class WaveFixTrajectoryNode : public cocos2d::CCNode {
        public:
            static WaveFixTrajectoryNode* create() {
                auto* ret = new WaveFixTrajectoryNode();
                if (ret && ret->init()) {
                    ret->autorelease();
                    return ret;
                }
                delete ret;
                return nullptr;
            }

            ~WaveFixTrajectoryNode() {
                g_node = nullptr;
            }

            void rebuild(GJBaseGameLayer* gl) {
                ensureChildren();
                if (m_draw) m_draw->clear();
                updatePlayer(gl->m_player1, m_p1);
                updatePlayer(gl->m_player2, m_p2);
            }

        private:
            PlayerLabels m_p1;
            PlayerLabels m_p2;
            cocos2d::CCDrawNode* m_draw = nullptr;

            void ensureChildren() {
                if (m_draw == nullptr) {
                    m_draw = cocos2d::CCDrawNode::create();
                    m_draw->m_bUseArea = false;
                    addChild(m_draw);
                }
                if (m_p1.vanilla == nullptr) m_p1 = makePair();
                if (m_p2.vanilla == nullptr) m_p2 = makePair();
            }

            PlayerLabels makePair() {
                PlayerLabels pair;
                pair.vanilla = cocos2d::CCLabelBMFont::create("", kFont);
                pair.ideal   = cocos2d::CCLabelBMFont::create("", kFont);
                pair.offset  = cocos2d::CCLabelBMFont::create("", kFont);
                pair.vanilla->setScale(kLabelScale);
                pair.ideal->setScale(kLabelScale);
                pair.offset->setScale(kLabelScale);
                pair.vanilla->setColor(kVanilla);
                pair.ideal->setColor(kIdeal);
                pair.offset->setColor(kOffset);
                addChild(pair.vanilla);
                addChild(pair.ideal);
                addChild(pair.offset);
                return pair;
            }

            static void hide(PlayerLabels& pair) {
                if (pair.vanilla) pair.vanilla->setVisible(false);
                if (pair.ideal)   pair.ideal->setVisible(false);
                if (pair.offset)  pair.offset->setVisible(false);
            }

            void updatePlayer(PlayerObject* player, PlayerLabels& pair) {
                if (pair.vanilla == nullptr) {
                    return;
                }

                double reqDx = 0.0;
                double reqDy = 0.0;
                if (player == nullptr
                    || !getVisualizerDelta(player, reqDx, reqDy)
                    || !player->m_isDart
                    || player->m_isSideways
                    || player->m_isDead) {
                    hide(pair);
                    return;
                }

                auto absDx = std::abs(reqDx);
                auto absDy = std::abs(reqDy);
                auto ratio = waveRatio(player->m_vehicleSize);
                if (!isWaveMovementCandidate(absDx, absDy, ratio)) {
                    hide(pair);
                    return;
                }

                auto pos   = player->getPosition();
                auto xDir  = player->m_isGoingLeft ? -1.0 : 1.0;
                auto dir   = std::copysign(1.0, reqDy);
                auto fwdX  = xDir * kRayLength;

                pair.vanilla->setString(fmt::format(fmt::runtime(kAngleFmt), slopeToDegrees(absDy / absDx)).c_str());
                pair.vanilla->setPosition(pos.x + kOffsetX, pos.y + kVanillaOffsetY);
                pair.vanilla->setVisible(true);

                pair.ideal->setString(fmt::format(fmt::runtime(kAngleFmt), slopeToDegrees(ratio)).c_str());
                pair.ideal->setPosition(pos.x + kOffsetX, pos.y + kIdealOffsetY);
                pair.ideal->setVisible(true);

                pair.offset->setString(fmt::format(fmt::runtime(kOffsetFmt), slopeToDegrees(absDy / absDx) - slopeToDegrees(ratio)).c_str());
                pair.offset->setPosition(pos.x + kOffsetX, pos.y + kOffsetOffsetY);
                pair.offset->setVisible(true);

                if (m_draw) {
                    cocos2d::CCPoint vanillaEnd(
                        pos.x + static_cast<float>(fwdX),
                        pos.y + static_cast<float>((reqDy / reqDx) * fwdX)
                    );
                    cocos2d::CCPoint idealEnd(
                        pos.x + static_cast<float>(fwdX),
                        pos.y + static_cast<float>(dir * ratio * fwdX)
                    );
                    m_draw->drawSegment(pos, vanillaEnd, kRayThickness, ccc4FFromccc3B(kVanilla));
                    m_draw->drawSegment(pos, idealEnd, kRayThickness, ccc4FFromccc3B(kIdeal));
                }
            }
        };

        WaveFixTrajectoryNode* getNode(GJBaseGameLayer* gl) {
            if (g_node != nullptr) {
                if (g_node->getParent() == nullptr) {
                    gl->m_objectLayer->addChild(g_node, kNodeZOrder);
                }
                return g_node;
            }
            auto* node = WaveFixTrajectoryNode::create();
            if (node == nullptr) {
                return nullptr;
            }
            node->setID(kNodeId);
            gl->m_objectLayer->addChild(node, kNodeZOrder);
            g_node = node;
            return node;
        }
    }

    void updateVisualizer(GJBaseGameLayer* gl) {
        if (!g_visualizerEnabled || gl == nullptr || gl->m_objectLayer == nullptr) {
            return;
        }
        if (auto* node = getNode(gl)) {
            node->rebuild(gl);
        }
    }
}
