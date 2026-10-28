#pragma once

// Predictive Input Buffer - estado compartido (logica <-> interfaz tactil)

#include <Geode/Geode.hpp>

namespace ib {

    struct Stage {
        int all;          // ticks maximos de retraso (todos los modos)
        int wave;         // ticks maximos de retraso (wave)
        const char* label;
        const char* desc;
    };

    inline constexpr int kStageCount = 6;

    // 1 tick = 1/240 s  ->  0.25 frames @ 60fps
    inline constexpr Stage kStages[kStageCount] = {
        {1, 1,  "1/1",  "STAGE 0: 1 tick All | 1 tick Wave (0.25f @ 60fps)"},
        {2, 2,  "2/2",  "STAGE 1: 2 ticks All | 2 ticks Wave (0.5f @ 60fps)"},
        {4, 4,  "4/4",  "STAGE 2: 4 ticks All | 4 ticks Wave (1.0f @ 60fps)"},
        {8, 8,  "8/8",  "STAGE 3: 8 ticks All | 8 ticks Wave (2.0f @ 60fps)"},
        {16, 8, "16/8", "STAGE 4: 16 ticks All | 8 ticks Wave (4.0f / 2.0f @ 60fps)"},
        {0, 0,  "OFF",  "STAGE 5: OFF (100% raw input)"},
    };

    struct State {
        // jugadores "fantasma" que se usan para simular la trayectoria
        PlayerObject* ghost1 = nullptr;
        PlayerObject* ghost2 = nullptr;
        cocos2d::CCNode* holder = nullptr;   // contenedor invisible de los fantasmas

        bool simulating = false;   // true mientras corre una simulacion
        bool injecting = false;    // true mientras se re-inyecta un input retrasado
        bool simDied = false;      // el fantasma murio durante la simulacion
        bool lastDown = false;     // ultimo estado pedido por el jugador
        bool prevDown = false;     // ultimo estado realmente enviado al juego
        bool lastIsP1 = true;
        bool enabled = true;

        int stage = 0;
        int limitAll = 2;
        int limitWave = 2;

        int pending = -1;          // >=0: ticks que faltan | -1: nada | -2: soltar tras inyectar
        bool pendingDown = false;  // estado del input retrasado
        bool flagD = false;        // llego un input contrario mientras habia uno retrasado

        float stepDt = 0.25f;      // dt de un tick (en unidades de frame @60fps)
        int sinceInput = 999;      // ticks desde el ultimo input
        int rapidCount = 0;        // inputs rapidos consecutivos
        bool simBroken = false;    // la simulacion no esta disponible en esta version
    };

    State& S();

    // ----- etapas -----
    void applyStage(int stage);   // aplica (sin guardar ni avisar)
    void setStage(int stage);     // aplica + guarda + actualiza la interfaz
    void cycleStage();            // F6 en PC / toque en el boton flotante
    void loadSettings();

    // ----- ajustes guardados -----
    bool cfgShowButton();
    void cfgSetShowButton(bool v);
    int cfgOpacity();             // 10..100
    void cfgSetOpacity(int v);
    bool cfgLogSaves();
    void cfgSetLogSaves(bool v);

    // ----- ciclo de vida (buffer.cpp) -----
    void createGhosts(PlayLayer* pl);
    void destroyGhosts();
    void resetRuntime();

    // ----- interfaz tactil (ui.cpp) -----
    void onStageChanged();                  // refresca el boton flotante
    void onSaved(int ticks);                // parpadeo cuando el buffer actua
    void attachHud(cocos2d::CCNode* parent);
    void openMenu();

}
