#pragma once

#include <Geode/Geode.hpp>

namespace ib {

    struct Stage {
        int all;
        int wave;
        const char* label;
        const char* desc;
    };

    inline constexpr int kStageCount = 6;

    inline constexpr Stage kStages[kStageCount] = {
        {1, 1,  "1/1",  "STAGE 0: 1 tick All | 1 tick Wave (0.25f @ 60fps)"},
        {2, 2,  "2/2",  "STAGE 1: 2 ticks All | 2 ticks Wave (0.5f @ 60fps)"},
        {4, 4,  "4/4",  "STAGE 2: 4 ticks All | 4 ticks Wave (1.0f @ 60fps)"},
        {8, 8,  "8/8",  "STAGE 3: 8 ticks All | 8 ticks Wave (2.0f @ 60fps)"},
        {16, 8, "16/8", "STAGE 4: 16 ticks All | 8 ticks Wave (4.0f / 2.0f @ 60fps)"},
        {0, 0,  "OFF",  "STAGE 5: OFF (100% raw input)"},
    };

    struct State {
        PlayerObject* ghost1 = nullptr;
        PlayerObject* ghost2 = nullptr;
        cocos2d::CCNode* holder = nullptr;

        bool simulating = false;
        bool injecting = false;
        bool simDied = false;
        bool lastDown = false;
        bool prevDown = false;
        bool lastIsP1 = true;
        bool enabled = true;

        int stage = 0;
        int limitAll = 2;
        int limitWave = 2;

        int pending = -1;
        bool pendingDown = false;
        bool flagD = false;

        float stepDt = 0.25f;
        int sinceInput = 999;
        int rapidCount = 0;
        bool simBroken = false;
    };

    State& S();

    void applyStage(int stage);
    void setStage(int stage);
    void cycleStage();
    void loadSettings();

    bool cfgShowButton();
    void cfgSetShowButton(bool v);
    bool cfgShowBg();
    void cfgSetShowBg(bool v);
    int cfgOpacity();
    void cfgSetOpacity(int v);

    void createGhosts(PlayLayer* pl);
    void destroyGhosts();
    void resetRuntime();

    void onStageChanged();
    void onSaved(int ticks);
    void attachHud(cocos2d::CCNode* parent);
    void openMenu();

}
