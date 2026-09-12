#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/ui/Label.hpp>
#include <deque>
#include <vector>
#include <map>
#include <algorithm>

using namespace geode::prelude;

constexpr int OVERLAY_TAG = 0x46504301;

struct InputEvent {
    bool isPress = true;
    bool isPlayer2 = false;
    int64_t frame = 0;
    bool perfect = false;
    int64_t early = 0;
    int64_t late = 0;
};

class ModSettings {
public:
    static ModSettings& get() {
        static ModSettings instance;
        return instance;
    }

    bool enabled = true;
    bool showFrameCounter = true;
    bool showInputLogging = false;
    float overlayX = 0.02f;
    float overlayY = 0.95f;
    float overlayScale = 0.5f;
    int perfectWindow = 1;
    int targetFrame = 0;
    bool showPlayer1 = true;
    bool showPlayer2 = true;
    cocos2d::ccColor3B textColor = { 255, 255, 255 };
    cocos2d::ccColor3B perfectColor = { 0, 255, 0 };
    cocos2d::ccColor3B earlyColor = { 255, 255, 0 };
    cocos2d::ccColor3B lateColor = { 255, 68, 68 };

    void reload() {
        auto mod = Mod::get();
        enabled = mod->getSettingValue<bool>("enabled");
        showFrameCounter = mod->getSettingValue<bool>("show-frame-counter");
        showInputLogging = mod->getSettingValue<bool>("show-input-logging");
        overlayX = static_cast<float>(mod->getSettingValue<double>("overlay-x"));
        overlayY = static_cast<float>(mod->getSettingValue<double>("overlay-y"));
        overlayScale = static_cast<float>(mod->getSettingValue<double>("overlay-scale"));
        perfectWindow = static_cast<int>(mod->getSettingValue<int64_t>("perfect-window"));
        targetFrame = static_cast<int>(mod->getSettingValue<int64_t>("target-frame"));
        showPlayer1 = mod->getSettingValue<bool>("show-player-1");
        showPlayer2 = mod->getSettingValue<bool>("show-player-2");
        textColor = mod->getSettingValue<cocos2d::ccColor3B>("text-color");
        perfectColor = mod->getSettingValue<cocos2d::ccColor3B>("perfect-color");
        earlyColor = mod->getSettingValue<cocos2d::ccColor3B>("early-color");
        lateColor = mod->getSettingValue<cocos2d::ccColor3B>("late-color");
    }

private:
    ModSettings() = default;
};
class CounterOverlay : public cocos2d::CCNode {
public:
    static CounterOverlay* create(PlayLayer* layer) {
        auto ret = new CounterOverlay();
        if (ret && ret->init(layer)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

    bool init(PlayLayer* layer) {
        if (!CCNode::init()) return false;
        m_playLayer = layer;
        this->setTag(OVERLAY_TAG);
        this->setZOrder(9999);
        buildLabels();
        return true;
    }

    void onPhysicsFrame() {
        if (!m_playLayer) return;
        m_frameCounter++;
        if (ModSettings::get().enabled && ModSettings::get().showFrameCounter) {
            updateLabels();
        }
    }

    void onInput(bool down, bool isPlayer2) {
        auto& s = ModSettings::get();
        if (!s.enabled) return;

        bool& held = isPlayer2 ? m_player2Held : m_player1Held;
        if (down && !held) {
            held = true;
            recordEvent(true, isPlayer2);
        } else if (!down && held) {
            held = false;
            recordEvent(false, isPlayer2);
        }
        updateLabels();
    }

    void reset() {
        m_frameCounter = 0;
        m_player1Held = false;
        m_player2Held = false;
        m_history.clear();
        m_frameWindows.clear();
        m_frameWindowsSorted.clear();
        auto& s = ModSettings::get();
        m_targetFrame = s.targetFrame;
        m_perfectWindow = s.perfectWindow;
        updateLabels();
    }

    void registerSuccessfulInput(int windowSize) {
        m_frameWindows[windowSize]++;
        m_frameWindowsSorted.clear();
        for (auto& [k, v] : m_frameWindows) {
            m_frameWindowsSorted.push_back({k, v});
        }
        std::sort(m_frameWindowsSorted.begin(), m_frameWindowsSorted.end());
    }

private:
    PlayLayer* m_playLayer = nullptr;
    int64_t m_frameCounter = 0;
    int64_t m_targetFrame = 0;
    int m_perfectWindow = 1;
    bool m_player1Held = false;
    bool m_player2Held = false;

    std::deque<InputEvent> m_history;
    static constexpr size_t MAX_HISTORY = 20;

    std::map<int, int> m_frameWindows;
    std::vector<std::pair<int, int>> m_frameWindowsSorted;

    geode::Label* m_frameLabel = nullptr;
    geode::Label* m_lastInputLabel = nullptr;
    geode::Label* m_resultLabel = nullptr;
    geode::Label* m_historyLabel = nullptr;
    geode::Label* m_windowLabel = nullptr;

    void buildLabels() {
        clearLabels();
        auto& s = ModSettings::get();
        auto winSize = cocos2d::CCDirector::sharedDirector()->getWinSize();
        float x = winSize.width * s.overlayX;
        float y = winSize.height * s.overlayY;
        float scale = s.overlayScale;

        m_frameLabel = Label::create("", "chatFont.fnt");
        m_frameLabel->setScale(scale);
        m_frameLabel->setAnchorPoint({ 0.f, 1.f });
        m_frameLabel->setPosition({ x, y });
        m_frameLabel->setColor(s.textColor);
        this->addChild(m_frameLabel);

        m_lastInputLabel = Label::create("", "chatFont.fnt");
        m_lastInputLabel->setScale(scale);
        m_lastInputLabel->setAnchorPoint({ 0.f, 1.f });
        m_lastInputLabel->setPosition({ x, y - (20.f * scale) });
        m_lastInputLabel->setColor(s.textColor);
        this->addChild(m_lastInputLabel);

        m_resultLabel = Label::create("", "chatFont.fnt");
        m_resultLabel->setScale(scale);
        m_resultLabel->setAnchorPoint({ 0.f, 1.f });
        m_resultLabel->setPosition({ x, y - (40.f * scale) });
        m_resultLabel->setColor(s.textColor);
        this->addChild(m_resultLabel);

        m_windowLabel = Label::create("", "chatFont.fnt");
        m_windowLabel->setScale(scale * 0.9f);
        m_windowLabel->setAnchorPoint({ 0.f, 1.f });
        m_windowLabel->setPosition({ x, y - (60.f * scale) });
        m_windowLabel->setColor(s.textColor);
        m_windowLabel->setAlignment(cocos2d::kCCTextAlignmentLeft);
        this->addChild(m_windowLabel);

        m_historyLabel = Label::create("", "chatFont.fnt");
        m_historyLabel->setScale(scale * 0.8f);
        m_historyLabel->setAnchorPoint({ 0.f, 1.f });
        m_historyLabel->setPosition({ x, y - (140.f * scale) });
        m_historyLabel->setColor(s.textColor);
        m_historyLabel->setAlignment(cocos2d::kCCTextAlignmentLeft);
        this->addChild(m_historyLabel);

        updateLabels();
    }

    void clearLabels() {
        if (m_frameLabel) { m_frameLabel->removeFromParent(); m_frameLabel = nullptr; }
        if (m_lastInputLabel) { m_lastInputLabel->removeFromParent(); m_lastInputLabel = nullptr; }
        if (m_resultLabel) { m_resultLabel->removeFromParent(); m_resultLabel = nullptr; }
        if (m_historyLabel) { m_historyLabel->removeFromParent(); m_historyLabel = nullptr; }
        if (m_windowLabel) { m_windowLabel->removeFromParent(); m_windowLabel = nullptr; }
    }

    void recordEvent(bool isPress, bool isPlayer2) {
        InputEvent ev;
        ev.isPress = isPress;
        ev.isPlayer2 = isPlayer2;
        ev.frame = m_frameCounter;
        int64_t diff = ev.frame - m_targetFrame;
        if (diff >= -m_perfectWindow && diff <= m_perfectWindow) {
            ev.perfect = true;
            registerSuccessfulInput(1);
        } else if (diff < 0) {
            ev.early = -diff;
        } else {
            ev.late = diff;
        }
        m_history.push_back(ev);
        if (m_history.size() > MAX_HISTORY) {
            m_history.pop_front();
        }
    }

    void updateLabels() {
        auto& s = ModSettings::get();
        if (m_frameLabel) {
            if (s.showFrameCounter) {
                m_frameLabel->setVisible(true);
                m_frameLabel->setString(fmt::format("Frame: {}", m_frameCounter).c_str());
            } else {
                m_frameLabel->setVisible(false);
            }
        }
        if (m_lastInputLabel) {
            if (!m_history.empty()) {
                const auto& last = m_history.back();
                const char* type = last.isPress ? "PRESS" : "RELEASE";
                const char* player = last.isPlayer2 ? "P2" : "P1";
                m_lastInputLabel->setString(fmt::format("Last: {} {} @ F{}", player, type, last.frame).c_str());
                m_lastInputLabel->setVisible(true);
            } else {
                m_lastInputLabel->setVisible(false);
            }
        }
        if (m_resultLabel) {
            if (!m_history.empty()) {
                const auto& last = m_history.back();
                if (last.perfect) {
                    m_resultLabel->setString("PERFECT");
                    m_resultLabel->setColor(s.perfectColor);
                } else if (last.early > 0) {
                    m_resultLabel->setString(fmt::format("EARLY: {} FRAMES", last.early).c_str());
                    m_resultLabel->setColor(s.earlyColor);
                } else {
                    m_resultLabel->setString(fmt::format("LATE: {} FRAMES", last.late).c_str());
                    m_resultLabel->setColor(s.lateColor);
                }
                m_resultLabel->setVisible(true);
            } else {
                m_resultLabel->setVisible(false);
            }
        }
        if (m_windowLabel) {
            if (!m_frameWindowsSorted.empty()) {
                std::string text = "Frame Windows:\n";
                for (const auto& [size, count] : m_frameWindowsSorted) {
                    text += fmt::format("{} frame: {}\n", size, count);
                }
                m_windowLabel->setString(text.c_str());
                m_windowLabel->setVisible(true);
            } else {
                m_windowLabel->setVisible(false);
            }
        }
        if (m_historyLabel) {
            if (s.showInputLogging) {
                std::string text;
                for (auto it = m_history.rbegin(); it != m_history.rend(); ++it) {
                    const auto& ev = *it;
                    std::string line;
                    if (ev.perfect) {
                        line = fmt::format("P{} {} F{} PERFECT", ev.isPlayer2 ? 2 : 1, ev.isPress ? "D" : "U", ev.frame);
                    } else if (ev.early > 0) {
                        line = fmt::format("P{} {} F{} EARLY {}", ev.isPlayer2 ? 2 : 1, ev.isPress ? "D" : "U", ev.frame, ev.early);
                    } else {
                        line = fmt::format("P{} {} F{} LATE {}", ev.isPlayer2 ? 2 : 1, ev.isPress ? "D" : "U", ev.frame, ev.late);
                    }
                    text += line + "\n";
                }
                m_historyLabel->setString(text.c_str());
                m_historyLabel->setVisible(true);
            } else {
                m_historyLabel->setVisible(false);
            }
        }
    }
};
class $modify(FPCPlayLayer, PlayLayer) {
    struct Fields {
        CounterOverlay* overlay = nullptr;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        if (this->getChildByTag(OVERLAY_TAG)) return true;
        auto overlay = CounterOverlay::create(this);
        if (overlay) {
            m_fields->overlay = overlay;
            this->addChild(overlay);
        }
        return true;
    }

    void destroyPlayer(PlayerObject* player, GameObject* gameObject) {
        if (m_fields->overlay) {
            m_fields->overlay->reset();
        }
        PlayLayer::destroyPlayer(player, gameObject);
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        if (m_fields->overlay) {
            m_fields->overlay->reset();
        }
    }

    void loadFromCheckpoint(CheckpointObject* checkpoint) {
        PlayLayer::loadFromCheckpoint(checkpoint);
        if (m_fields->overlay) {
            m_fields->overlay->reset();
        }
    }
};

class $modify(FPCGJBaseGameLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        if (!self.setHookPriorityPre("GJBaseGameLayer::handleButton", Priority::VeryEarly)) {
            geode::log::warn("[FPC] Failed to set handleButton priority");
        }
    }

    void handleButton(bool down, int button, bool isPlayer2) {
        auto* playLayer = typeinfo_cast<PlayLayer*>(this);
        if (playLayer) {
            auto* overlay = typeinfo_cast<CounterOverlay*>(playLayer->getChildByTag(OVERLAY_TAG));
            if (overlay) {
                overlay->onInput(down, isPlayer2);
            }
        }
        GJBaseGameLayer::handleButton(down, button, isPlayer2);
    }
};

class $modify(FPCPlayerObject, PlayerObject) {
    void update(float dt) {
        if (!this->m_isSecondPlayer) {
            auto* scene = cocos2d::CCDirector::sharedDirector()->getRunningScene();
            if (scene) {
                auto* playLayer = typeinfo_cast<PlayLayer*>(scene->getChildByType<PlayLayer>(0));
                if (playLayer) {
                    auto* overlay = typeinfo_cast<CounterOverlay*>(playLayer->getChildByTag(OVERLAY_TAG));
                    if (overlay) {
                        overlay->onPhysicsFrame();
                    }
                }
            }
        }
        PlayerObject::update(dt);
    }
};

$on_mod(Loaded) {
    ModSettings::get().reload();
    listenForSettingChanges<bool>("enabled", [](bool) { ModSettings::get().reload(); });
    listenForSettingChanges<bool>("show-frame-counter", [](bool) { ModSettings::get().reload(); });
    listenForSettingChanges<bool>("show-input-logging", [](bool) { ModSettings::get().reload(); });
    listenForSettingChanges<double>("overlay-x", [](double) { ModSettings::get().reload(); });
    listenForSettingChanges<double>("overlay-y", [](double) { ModSettings::get().reload(); });
    listenForSettingChanges<double>("overlay-scale", [](double) { ModSettings::get().reload(); });
    listenForSettingChanges<int64_t>("perfect-window", [](int64_t) { ModSettings::get().reload(); });
    listenForSettingChanges<int64_t>("target-frame", [](int64_t) { ModSettings::get().reload(); });
    listenForSettingChanges<bool>("show-player-1", [](bool) { ModSettings::get().reload(); });
    listenForSettingChanges<bool>("show-player-2", [](bool) { ModSettings::get().reload(); });
    listenForSettingChanges<cocos2d::ccColor3B>("text-color", [](cocos2d::ccColor3B) { ModSettings::get().reload(); });
    listenForSettingChanges<cocos2d::ccColor3B>("perfect-color", [](cocos2d::ccColor3B) { ModSettings::get().reload(); });
    listenForSettingChanges<cocos2d::ccColor3B>("early-color", [](cocos2d::ccColor3B) { ModSettings::get().reload(); });
    listenForSettingChanges<cocos2d::ccColor3B>("late-color", [](cocos2d::ccColor3B) { ModSettings::get().reload(); });
    geode::log::info("[Frame Perfect Counter] Loaded");
}
