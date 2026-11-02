// Predictive Input Buffer  (puerto Windows + Android)   v1.2.1
//
// Logica reconstruida a partir del binario original (v4.7 "Zero-Lag + Up Arrow Fix"):
//   * Al pulsar/soltar se simula la trayectoria con un jugador "fantasma".
//   * Si el input inmediato es peligroso, se prueban retrasos de 1..N ticks
//     (1 tick = 1/240 s); wave compara candidatos y elige la mayor supervivencia.
//   * Etapas: 1/1 -> 2/2 -> 4/4 -> 8/8 -> 16/8 -> OFF  (todos los modos / wave).
//   * En PC se cambia con F6; en Android con el boton flotante tactil (ui.cpp).
//
// Optimizaciones v1.2.0 (pensadas para gama media, p. ej. Helio G85 + 4 GB):
//   * Presupuesto de CPU por clic: si la prediccion no cabe, se deja pasar el input crudo
//     (nunca hay un tiron de mas de N ms). Ajustable en el menu (por defecto 6 ms).
//   * Horizonte adaptativo: se mide cuanto cuesta simular 1 tick en ESTE dispositivo y el
//     horizonte se acorta para que la primera simulacion use <= 40 % del presupuesto.
//   * Retrasos de 1,2,3,4,6,8,10... (en vez de 1,2,3,4,5,6...): ~40 % menos simulaciones.
//   * Los nodos de los fantasmas (particulas, animaciones) se ponen en pausa: cero trabajo en reposo.
//   * Ajustes en cache y estado global (sin consultas ni guardas de inicializacion por tick).

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/HardStreak.hpp>
#include <Geode/modify/GameObject.hpp>
#include <Geode/modify/EffectGameObject.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>

#include "state.hpp"

// reloj en microsegundos (los tests lo sustituyen por uno falso)
#ifndef IB_NOW_US
#define IB_NOW_US() static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::microseconds>( \
    std::chrono::steady_clock::now().time_since_epoch()).count())
#endif

#ifdef GEODE_IS_WINDOWS
extern "C" __declspec(dllimport) short __stdcall GetAsyncKeyState(int vKey);
#endif

using namespace geode::prelude;

// ---------------------------------------------------------------------------
//  Ajustes / etapas
// ---------------------------------------------------------------------------
namespace ib {

    void applyStage(int s) {
        auto& st = S();
        st.stage = ((s % kStageCount) + kStageCount) % kStageCount;
        auto const& cfg = kStages[st.stage];
        st.enabled = cfg.all > 0;
        st.limitAll = cfg.all;
        st.limitWave = cfg.wave;
        st.pending = -1;
        st.flagD = false;
    }

    void setStage(int s) {
        applyStage(s);
        Mod::get()->setSavedValue<int>("stage", S().stage);
        log::info("[InputBuffer] {}", kStages[S().stage].desc);
        onStageChanged();
    }

    void cycleStage() {
        setStage(S().stage + 1);
    }

    void loadSettings() {
        auto* mod = Mod::get();
        auto& st = S();
        st.budgetMs = std::clamp(mod->getSavedValue<int>("budget-ms", 6), 1, 30);
        st.showButton = mod->getSavedValue<bool>("show-button", true);
        st.opacity = std::clamp(mod->getSavedValue<int>("button-opacity", 55), 10, 100);
        st.logSaves = mod->getSavedValue<bool>("log-saves", false);

        // Ajustes visuales usados por la interfaz tactil (ui.cpp).
        st.showBg = mod->getSavedValue<bool>("show-bg", true);
        st.colorBg = mod->getSavedValue<int>("color-bg", 0x202040);
        st.colorBorder = mod->getSavedValue<int>("color-border", 0xFFFFFF);
        st.colorText = mod->getSavedValue<int>("color-text", 0xFFFFFF);
        st.colorBtn = mod->getSavedValue<int>("color-btn", 0x3399FF);

        applyStage(mod->getSavedValue<int>("stage", 0));
        log::info("[InputBuffer] v4.7 LOADED (touch port, perf build) - {} | budget {} ms",
                  kStages[st.stage].desc, st.budgetMs);
    }

    // Los ajustes se leen una vez (cache) y se escriben al guardado solo al cambiarlos.
    bool cfgShowButton() { return S().showButton; }
    void cfgSetShowButton(bool v) { S().showButton = v; Mod::get()->setSavedValue<bool>("show-button", v); }

    // Fondo del HUD flotante.
    bool cfgShowBg() { return S().showBg; }
    void cfgSetShowBg(bool v) {
        S().showBg = v;
        Mod::get()->setSavedValue<bool>("show-bg", v);
    }

    // Colores personalizados del menu y del HUD.
    int cfgColorBg() { return S().colorBg; }
    void cfgSetColorBg(int c) {
        S().colorBg = c & 0xFFFFFF;
        Mod::get()->setSavedValue<int>("color-bg", S().colorBg);
    }

    int cfgColorBorder() { return S().colorBorder; }
    void cfgSetColorBorder(int c) {
        S().colorBorder = c & 0xFFFFFF;
        Mod::get()->setSavedValue<int>("color-border", S().colorBorder);
    }

    int cfgColorText() { return S().colorText; }
    void cfgSetColorText(int c) {
        S().colorText = c & 0xFFFFFF;
        Mod::get()->setSavedValue<int>("color-text", S().colorText);
    }

    int cfgColorBtn() { return S().colorBtn; }
    void cfgSetColorBtn(int c) {
        S().colorBtn = c & 0xFFFFFF;
        Mod::get()->setSavedValue<int>("color-btn", S().colorBtn);
    }

    int cfgOpacity() { return S().opacity; }
    void cfgSetOpacity(int v) {
        S().opacity = std::clamp(v, 10, 100);
        Mod::get()->setSavedValue<int>("button-opacity", S().opacity);
    }

    bool cfgLogSaves() { return S().logSaves; }
    void cfgSetLogSaves(bool v) { S().logSaves = v; Mod::get()->setSavedValue<bool>("log-saves", v); }

    int cfgBudgetMs() { return S().budgetMs; }
    void cfgSetBudgetMs(int v) {
        S().budgetMs = std::clamp(v, 1, 30);
        Mod::get()->setSavedValue<int>("budget-ms", S().budgetMs);
    }

    void resetRuntime() {
        auto& st = S();
        st.simulating = false;
        st.injecting = false;
        st.simDied = false;
        st.lastDown = false;
        st.prevDown = false;
        st.lastIsP1 = true;
        st.pending = -1;
        st.pendingDown = false;
        st.flagD = false;
        st.sinceInput = 999;
        st.rapidCount = 0;
    }

}

$on_mod(Loaded) {
    ib::loadSettings();
}

// ---------------------------------------------------------------------------
//  Utilidades (toleran diferencias entre versiones de los bindings)
// ---------------------------------------------------------------------------
namespace ibx {
namespace {

    // Tipos de objeto que se conservan durante la simulacion:
    // 0 = solido, 2 = peligro, 25 = pendiente, 39 y 47 (igual que el original)
    constexpr std::uint64_t kCollMask = 0x808002000005ull;

#define IB_FLAG(fn, member)                                  \
    template <class T>                                       \
    bool fn(T* p) {                                          \
        if constexpr (requires { p->member; }) {             \
            return static_cast<bool>(p->member);             \
        } else {                                             \
            return false;                                    \
        }                                                    \
    }

    IB_FLAG(flagShip, m_isShip)
    IB_FLAG(flagBird, m_isBird)      // UFO
    IB_FLAG(flagDart, m_isDart)      // wave
    IB_FLAG(flagRobot, m_isRobot)
    IB_FLAG(flagSwing, m_isSwing)
    IB_FLAG(flagDead, m_isDead)
    IB_FLAG(flagGround, m_isOnGround)
#undef IB_FLAG

    template <class T>
    float vehicleSize(T* p) {
        if constexpr (requires { p->m_vehicleSize; }) {
            return static_cast<float>(p->m_vehicleSize);
        } else {
            return 1.f;
        }
    }

    template <class L>
    bool dualOn(L* l) {
        if constexpr (requires { l->m_gameState.m_isDualMode; }) {
            return static_cast<bool>(l->m_gameState.m_isDualMode);
        } else {
            return l->m_player2 != nullptr;
        }
    }

    template <class L>
    bool platformerLevel(L* l) {
        if constexpr (requires { l->m_levelSettings->m_platformerMode; }) {
            return l->m_levelSettings && l->m_levelSettings->m_platformerMode;
        } else {
            return false;
        }
    }

    template <class L>
    float timeWarp(L* l) {
        if constexpr (requires { l->m_gameState.m_timeWarp; }) {
            return static_cast<float>(l->m_gameState.m_timeWarp);
        } else {
            return 1.f;
        }
    }

    // Retrasos que se prueban: 1,2,3,4 y luego de 2 en 2 (6,8,10,...).
    // A partir de 4 ticks (1 frame @240) la diferencia de 1 tick no se nota y se ahorran simulaciones.
    constexpr int nextDelay(int d) {
        return d < 4 ? d + 1 : d + 2;
    }

    // Pone en pausa el scheduler y las acciones de todo el arbol: las particulas y animaciones
    // del fantasma no hacen trabajo en reposo (el fisica se avanza a mano con update()).
    void pauseTree(CCNode* n) {
        if (!n) return;
        n->pauseSchedulerAndActions();
        auto* kids = n->getChildren();
        if (!kids) return;
        for (unsigned i = 0, c = kids->count(); i < c; ++i) {
            pauseTree(static_cast<CCNode*>(kids->objectAtIndex(i)));
        }
    }

    // Copia el estado fisico del jugador real al fantasma.
    template <class T>
    void copyState(T* g, T* p) {
        if constexpr (requires { g->copyAttributes(p); }) {
            g->copyAttributes(p);
        }
#define IB_COPY(f)                                   \
    if constexpr (requires { g->f = p->f; }) {       \
        g->f = p->f;                                 \
    }
        IB_COPY(m_isShip)
        IB_COPY(m_isBird)
        IB_COPY(m_isBall)
        IB_COPY(m_isDart)
        IB_COPY(m_isRobot)
        IB_COPY(m_isSpider)
        IB_COPY(m_isSwing)
        IB_COPY(m_isUpsideDown)
        IB_COPY(m_isSideways)
        IB_COPY(m_isOnGround)
        IB_COPY(m_isOnSlope)
        IB_COPY(m_wasOnSlope)
        IB_COPY(m_isGoingLeft)
        IB_COPY(m_playerSpeed)
        IB_COPY(m_vehicleSize)
        IB_COPY(m_gravityMod)
        IB_COPY(m_yVelocity)
        IB_COPY(m_xVelocity)
        IB_COPY(m_platformerXVelocity)
        IB_COPY(m_lastPosition)
        IB_COPY(m_isPlatformer)
#undef IB_COPY
        g->setPosition(p->getPosition());
        g->setRotation(p->getRotation());
        g->setScaleX(p->getScaleX());
        g->setScaleY(p->getScaleY());
        if constexpr (requires { g->m_isDead = false; }) {
            g->m_isDead = false;
        }
    }

    // Vacia los registros de colision del fantasma entre ticks.
    template <class T>
    void clearLogs(T* p) {
        if constexpr (requires { p->m_collisionLogTop; }) {
            if (p->m_collisionLogTop) p->m_collisionLogTop->removeAllObjects();
        }
        if constexpr (requires { p->m_collisionLogBottom; }) {
            if (p->m_collisionLogBottom) p->m_collisionLogBottom->removeAllObjects();
        }
        if constexpr (requires { p->m_collisionLogLeft; }) {
            if (p->m_collisionLogLeft) p->m_collisionLogLeft->removeAllObjects();
        }
        if constexpr (requires { p->m_collisionLogRight; }) {
            if (p->m_collisionLogRight) p->m_collisionLogRight->removeAllObjects();
        }
    }

    template <class P>
    void applyButton(P* p, bool down) {
        if (down) {
            p->pushButton(PlayerButton::Jump);
        } else {
            p->releaseButton(PlayerButton::Jump);
        }
    }

    template <class L, class P>
    bool runCollisions(L* layer, P* player, float dt) {
        if constexpr (requires { layer->checkCollisions(player, dt, false); }) {
            layer->checkCollisions(player, dt, false);
            return true;
        } else {
            return false;
        }
    }

    template <class P, class L, class H>
    P* makePlayer(L* layer, H* parent) {
        if constexpr (requires { P::create(1, 1, layer, parent, true); }) {
            return P::create(1, 1, layer, parent, true);
        } else {
            return nullptr;
        }
    }

    // Envia un input "real" al juego saltandose el buffer.
    void inject(GJBaseGameLayer* pl, bool down) {
        auto& st = ib::S();
        st.injecting = true;
        st.prevDown = down;
        pl->handleButton(down, 1, st.lastIsP1);
        st.injecting = false;
    }

    // Simula `horizon` ticks despues de aplicar el input (`down`) en el tick `delay`.
    // Devuelve cuantos ticks sobrevive (horizon = sobrevive todo).
    //   info.abort    = murio ANTES de que el input se aplique (no tiene sentido retrasar)
    //   info.timedOut = se paso de `deadlineUs` (reloj en microsegundos): el resultado no vale
    int simulate(GJBaseGameLayer* pl, PlayerObject* p1, PlayerObject* p2, bool dual,
                 int delay, bool down, int horizon, std::int64_t deadlineUs, ib::SimInfo& info) {
        auto& st = ib::S();
        info = ib::SimInfo{};

        PlayerObject* g1 = st.ghost1;
        if (!g1 || st.simBroken || horizon <= 0) return horizon;
        PlayerObject* g2 = (dual && p2) ? st.ghost2 : nullptr;

        const std::int64_t t0 = IB_NOW_US();

        st.simulating = true;
        st.simDied = false;

        copyState(g1, p1);
        if (g2) copyState(g2, p2);

        // estado previo al input que se evalua (soltado si es un press, pulsado si es un release)
        applyButton(g1, !down);
        if (g2) applyButton(g2, !down);

        float dt = st.stepDt;
        if (!(dt > 0.001f && dt < 2.0f)) dt = 0.25f;

        // modos "hold" (wave / ufo / swing): no se usa el atajo de aterrizaje seguro
        bool holdMode = true;
        if (!flagDart(p1) && !flagBird(p1) && !flagSwing(p1) && !flagShip(p1)) {
            if (!dual || !p2) {
                holdMode = false;
            } else if (flagDart(p2) || flagBird(p2) || flagShip(p2)) {
                holdMode = true;
            } else {
                holdMode = flagSwing(p2) || flagShip(p2);
            }
        }

        const int total = horizon + delay;
        int after = 0;
        int airTicks = 0;
        int groundStreak = 0;
        bool died = false;
        int deathTick = 0;

        for (int i = 0; i < total; ++i) {
            // Comprueba antes de hacer trabajo caro; como maximo se excede ~1 tick.
            if ((i & 1) == 0 && IB_NOW_US() >= deadlineUs) {
                info.timedOut = true;
                break;
            }
            if (i == delay) {
                applyButton(g1, down);
                if (g2) applyButton(g2, down);
            }

            g1->update(dt);
            if (g2) g2->update(dt);
            ++info.ticks;

            if (st.simDied) { died = true; deathTick = i; break; }

            clearLogs(g1);
            if (!runCollisions(pl, g1, dt)) { st.simBroken = true; break; }
            if (st.simDied) { died = true; deathTick = i; break; }

            if (g2) {
                clearLogs(g2);
                runCollisions(pl, g2, dt);
                if (st.simDied) { died = true; deathTick = i; break; }
            }

            if (i < delay) continue;
            ++after;
            if (holdMode) continue;

            const bool ground2 = g2 ? flagGround(g2) : true;
            if (!flagGround(g1) || !ground2) {
                ++airTicks;
                groundStreak = 0;
            } else if (airTicks >= 6) {
                if (groundStreak > 2) { after = horizon; break; }   // salto completado a salvo
                ++groundStreak;
            }
        }

        if (died && deathTick < delay) info.abort = true;
        st.simulating = false;

        // coste medido por tick (media movil): alimenta el horizonte adaptativo
        if (info.ticks >= 8) {
            const double perTick = static_cast<double>(IB_NOW_US() - t0) / info.ticks;
            st.tickCostUs = std::clamp(0.75 * st.tickCostUs + 0.25 * perTick, 1.0, 5000.0);
        }

        // tras la primera simulacion ya existen todos los nodos del fantasma: a pausa
        if (!st.ghostsPaused && st.holder) {
            pauseTree(st.holder);
            st.ghostsPaused = true;
        }

        if (st.simBroken) {
            log::warn("[InputBuffer] GJBaseGameLayer::checkCollisions no disponible: simulacion desactivada");
            return horizon;
        }
        return after;
    }

}
}

// ---------------------------------------------------------------------------
//  Ciclo de vida de los fantasmas
// ---------------------------------------------------------------------------
namespace ib {

    void destroyGhosts() {
        auto& st = S();
        PlayerObject** slots[2] = {&st.ghost1, &st.ghost2};
        for (auto** slot : slots) {
            if (*slot) {
                (*slot)->removeFromParent();
                (*slot)->release();
                *slot = nullptr;
            }
        }
        if (st.holder) {
            st.holder->removeFromParent();
            st.holder->release();
            st.holder = nullptr;
        }
        st.ghostsPaused = false;
        resetRuntime();
    }

    void createGhosts(PlayLayer* pl) {
        destroyGhosts();
        auto& st = S();
        if (!pl || !pl->m_objectLayer) return;

        // Contenedor invisible: asi estelas/particulas del fantasma tampoco se ven.
        auto* holder = CCLayer::create();
        if (!holder) return;
        holder->setVisible(false);
        holder->retain();
        pl->m_objectLayer->addChild(holder);
        st.holder = holder;

        auto make = [&](PlayerObject*& out) {
            out = ibx::makePlayer<PlayerObject>(pl, holder);
            if (!out) return;
            out->retain();
            out->setPosition({0.f, 105.f});
            out->setVisible(false);
            holder->addChild(out);
        };
        make(st.ghost1);
        make(st.ghost2);

        // sin trabajo en reposo: particulas / animaciones de los fantasmas en pausa
        ibx::pauseTree(holder);

        if (!st.ghost1) {
            log::warn("[InputBuffer] no se pudo crear el jugador fantasma: el buffer queda inactivo");
        }
    }

}

// ---------------------------------------------------------------------------
//  Hooks: entrada (handleButton) y colisiones
// ---------------------------------------------------------------------------
class $modify(IBGameLayer, GJBaseGameLayer) {
    void handleButton(bool down, int button, bool isPlayer1) {
        auto& st = ib::S();

        if (st.simulating || st.injecting || button != 1) {
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        auto* pl = PlayLayer::get();
        if (!pl || static_cast<GJBaseGameLayer*>(pl) != static_cast<GJBaseGameLayer*>(this)) {
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        const bool dual = ibx::dualOn(pl) && pl->m_player2 != nullptr;

        if (!isPlayer1 && ibx::platformerLevel(pl)) {
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        st.lastIsP1 = isPlayer1;
        st.lastDown = down;

        if (!st.enabled) {
            st.prevDown = down;
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        PlayerObject* p1 = this->m_player1;
        PlayerObject* p2 = dual ? pl->m_player2 : nullptr;

        // La asistencia de nave solo existe en 1/1. En las demas etapas,
        // y en dual mixto, la nave conserva el input original sin alteraciones.
        const bool p1Ship = p1 && ibx::flagShip(p1);
        const bool p2Ship = p2 && ibx::flagShip(p2);
        const bool anyShip = p1Ship || p2Ship;
        const bool shipAssist = st.stage == 0 && p1Ship && (!dual || p2Ship);
        if (!p1 || ibx::flagDead(p1) || (anyShip && !shipAssist)) {
            st.pending = -1;
            st.flagD = false;
            st.prevDown = down;
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        // ya hay un input retrasado en camino: este evento solo se anota
        if (st.pending >= 0) {
            st.flagD = (st.pendingDown != down);
            st.sinceInput = 0;
            return;
        }
        if (st.pending == -2) {
            st.pending = -1;
            st.flagD = false;
            const bool d = !st.pendingDown;
            if (st.prevDown != d) ibx::inject(pl, d);
        }

        const bool p1Wave = ibx::flagDart(p1);
        const bool p1Robot = ibx::flagRobot(p1);
        const bool p2Wave = p2 && ibx::flagDart(p2);
        const bool anyWave = p1Wave || p2Wave;

        bool mini = false;
        if (p1Wave) {
            mini = 1.0f > ibx::vehicleSize(p1);
            if (!mini && p2) mini = p2Wave && 1.0f > ibx::vehicleSize(p2);
        } else if (p2) {
            mini = p2Wave && 1.0f > ibx::vehicleSize(p2);
        }

        // spam de clics en wave: se deja pasar el input crudo
        if (st.sinceInput < 8) {
            const int c = st.rapidCount++;
            st.sinceInput = 0;
            if (c >= 2 && anyWave) {
                st.prevDown = down;
                return GJBaseGameLayer::handleButton(down, button, isPlayer1);
            }
        } else {
            st.sinceInput = 0;
            st.rapidCount = 0;
        }

        // que eventos se bufferizan: todo en wave; solo "press" en el resto (y release en robot)
        if (!p1Wave) {
            const bool p2WaveOnly = p2 && !p1Robot && p2Wave;
            if (!p2WaveOnly) {
                const bool robotAny = p1Robot || (p2 && ibx::flagRobot(p2));
                if (!down && !robotAny && !shipAssist) {
                    st.prevDown = false;
                    return GJBaseGameLayer::handleButton(down, button, isPlayer1);
                }
            }
        }

        st.flagD = false;
        const char* name = down ? "PRESS" : "RELEASE";
        const int baseH = mini ? 28 : (anyWave ? 38 : 48);
        const int maxDelay = shipAssist ? 1 : (anyWave ? st.limitWave : st.limitAll);

        // --- presupuesto de CPU (gama media) ---
        // Todo el calculo de este clic debe caber en `budgetMs`. El horizonte se acorta segun lo
        // que cueste simular un tick en este dispositivo (la 1a simulacion usa <= 40 % del tiempo).
        const std::int64_t t0 = IB_NOW_US();
        const std::int64_t budgetUs = static_cast<std::int64_t>(st.budgetMs) * 1000;
        const std::int64_t deadline = t0 + budgetUs;
        // Reparte el presupuesto entre la simulacion base y todos los candidatos.
        // Incluye el coste medio del retraso para no dimensionar el horizonte solo
        // con la primera simulacion (el fallo de la formula anterior).
        const int candidateCount = maxDelay + 1;
        const double averageDelay = static_cast<double>(maxDelay) * 0.5;
        const double usableUs = static_cast<double>(budgetUs) * 0.72;
        const double estimatedTicksPerCandidate = usableUs /
            (std::max(1, candidateCount) * std::max(1.0, st.tickCostUs));
        const int plannedH = static_cast<int>(estimatedTicksPerCandidate - averageDelay);
        const int minH = std::min(baseH, 1);
        const int horizon = std::clamp(plannedH, minH, baseH);

        ib::SimInfo info;
        const int f0 = ibx::simulate(pl, p1, p2, dual, 0, down, horizon, deadline, info);

        // sin tiempo para decidir, o el input inmediato ya sobrevive todo el horizonte
        if (info.timedOut || f0 >= horizon || maxDelay <= 0) {
            st.prevDown = down;
            return GJBaseGameLayer::handleButton(down, button, isPlayer1);
        }

        int found = 0;
        int foundRes = 0;
        bool giveUp = false;

        if (anyWave) {
            // Wave: no te quedes con el primer retraso que apenas cruza un umbral.
            // Evalua todos los ticks disponibles y elige la trayectoria con mayor
            // supervivencia; al empatar conserva el retraso menor (menos latencia).
            int bestDelay = 0;
            int bestResult = f0;
            for (int d = 1; d <= maxDelay; ++d) {
                if (IB_NOW_US() >= deadline) { giveUp = true; break; }
                const int r = ibx::simulate(pl, p1, p2, dual, d, down, horizon, deadline, info);
                if (info.abort || info.timedOut) { giveUp = true; break; }
                if (r > bestResult) {
                    bestResult = r;
                    bestDelay = d;
                }
                // El primer retraso que sobrevive todo el horizonte es el mas corto
                // de los resultados perfectos: no hay razon para probar mas tarde.
                if (r >= horizon) {
                    bestResult = r;
                    bestDelay = d;
                    break;
                }
            }
            const int required = std::min(horizon, f0 + 8);
            if (!giveUp && bestDelay > 0 && bestResult >= required) {
                found = bestDelay;
                foundRes = bestResult;
            }
        } else if (shipAssist) {
            // Asistencia de nave muy conservadora: solo desplaza un tick si la
            // simulacion predice una mejora clara; nunca inventa pulsaciones.
            const int r = ibx::simulate(pl, p1, p2, dual, 1, down, horizon, deadline, info);
            if (info.abort || info.timedOut) giveUp = true;
            else if (r > f0 && r >= std::min(horizon, f0 + 4)) {
                found = 1;
                foundRes = r;
            }
        } else {
            const int target = std::min(horizon, f0 + 12);
            for (int d = 1; d <= maxDelay; d = ibx::nextDelay(d)) {
                if (IB_NOW_US() >= deadline) { giveUp = true; break; }
                const int r = ibx::simulate(pl, p1, p2, dual, d, down, horizon, deadline, info);
                if (info.abort || info.timedOut) { giveUp = true; break; }
                if (r >= target) { found = d; foundRes = r; break; }
            }
        }

        if (!giveUp && found > 0) {
            if (st.logSaves) {
                log::info("[InputBuffer] *** SAVED +{} TICKS ({:.2f} frames @ 60fps) {}! (Frame0: {} -> Delayed: {}) "
                          "[Limit: {} | horizon {} | {} us, ~{:.0f} us/tick] ***",
                          found, found * 0.25f, name, f0, foundRes, maxDelay, horizon,
                          static_cast<long long>(IB_NOW_US() - t0), st.tickCostUs);
            }
            ib::onSaved(found);
            st.pendingDown = down;
            st.pending = found;
            return;   // el input se inyectara dentro de `found` ticks
        }

        st.prevDown = down;
        return GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    // Durante la simulacion el fantasma solo choca con solidos / peligros / pendientes.
    void collisionCheckObjects(PlayerObject* player, gd::vector<GameObject*>* objects, int count, float dt) {
        if (!ib::S().simulating || !objects) {
            return GJBaseGameLayer::collisionCheckObjects(player, objects, count, dt);
        }

        static gd::vector<GameObject*> filtered;
        filtered.clear();

        const int n = std::min<int>(count, static_cast<int>(objects->size()));
        for (int i = 0; i < n; ++i) {
            GameObject* obj = (*objects)[i];
            if (!obj) continue;
            const int t = static_cast<int>(obj->m_objectType);
            if (t >= 0 && t <= 47 && ((ibx::kCollMask >> t) & 1ull)) {
                filtered.push_back(obj);
            }
        }

        GJBaseGameLayer::collisionCheckObjects(player, &filtered, static_cast<int>(filtered.size()), dt);
    }
};

// ---------------------------------------------------------------------------
//  Hooks: jugador
// ---------------------------------------------------------------------------
class $modify(IBPlayer, PlayerObject) {
    void update(float dt) {
        auto& st = ib::S();
        PlayerObject* self = this;

        if (!st.simulating) {
            auto* pl = PlayLayer::get();
            if (pl && self == pl->m_player1) {

#ifdef GEODE_IS_WINDOWS
                // F6 = cambiar de etapa (en Android se usa el boton tactil)
                if (GetAsyncKeyState(0x75) & 1) {
                    ib::cycleStage();
                }
#endif

                if (st.enabled) {
                    if (st.sinceInput <= 998) ++st.sinceInput;

                    const bool dual = ibx::dualOn(pl) && pl->m_player2 != nullptr;

                    const bool p1Ship = ibx::flagShip(self);
                    const bool p2Ship = dual && ibx::flagShip(pl->m_player2);
                    const bool shipAssist = st.stage == 0 && p1Ship && (!dual || p2Ship);
                    if ((p1Ship || p2Ship) && !shipAssist) {
                        // En etapas distintas de 1/1, nave = input crudo.
                        st.pending = -1;
                        st.flagD = false;
                        const bool d = st.lastDown;
                        if (st.prevDown != d) ibx::inject(pl, d);
                    } else {
                        const int pd = st.pending;
                        if (pd == 0) {
                            ibx::inject(pl, st.pendingDown);
                            if (st.flagD) {
                                st.pending = -2;
                                st.flagD = false;
                            } else {
                                st.pending = -1;
                            }
                        } else if (pd > 0) {
                            st.pending = pd - 1;
                        } else if (pd == -1) {
                            if (st.prevDown != st.lastDown) ibx::inject(pl, st.lastDown);
                        } else if (pd == -2) {
                            ibx::inject(pl, !st.pendingDown);
                            st.flagD = false;
                            st.pending = -1;
                        }
                    }
                }
            }
        }

        PlayerObject::update(dt);

        // dt de un tick: se muestrea 1 de cada 8 ticks y solo con el buffer activo
        if (st.enabled && !st.simulating && (st.dtCounter++ & 7u) == 0) {
            auto* pl = PlayLayer::get();
            if (pl) {
                const float tw = ibx::timeWarp(pl);
                st.stepDt = tw > 0.f ? dt / tw : dt;
            }
        }
    }

    // --- efectos secundarios que se anulan mientras se simula ---
    void incrementJumps() {
        if (ib::S().simulating) return;
        PlayerObject::incrementJumps();
    }

    void playSpiderDashEffect(cocos2d::CCPoint from, cocos2d::CCPoint to) {
        if (ib::S().simulating) return;
        PlayerObject::playSpiderDashEffect(from, to);
    }
};

class $modify(IBHardStreak, HardStreak) {
    void addPoint(cocos2d::CCPoint point) {
        if (ib::S().simulating) return;
        HardStreak::addPoint(point);
    }
};

class $modify(IBGameObject, GameObject) {
    void playShineEffect() {
        if (ib::S().simulating) return;
        GameObject::playShineEffect();
    }
};

class $modify(IBEffectObject, EffectGameObject) {
    void triggerObject(GJBaseGameLayer* layer, int p1, gd::vector<int> const* p2) {
        if (ib::S().simulating) return;
        EffectGameObject::triggerObject(layer, p1, p2);
    }
};

// ---------------------------------------------------------------------------
//  Hooks: PlayLayer
// ---------------------------------------------------------------------------
class $modify(IBPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        ib::createGhosts(this);
        ib::resetRuntime();
        ib::attachHud(this);
        return true;
    }

    void onQuit() {
        PlayLayer::onQuit();
        ib::destroyGhosts();
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        ib::resetRuntime();
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        auto& st = ib::S();
        if (st.simulating && (player == st.ghost1 || player == st.ghost2)) {
            st.simDied = true;   // murio el fantasma: no se mata al jugador real
            return;
        }
        st.pending = -1;
        st.flagD = false;
        st.prevDown = false;
        st.rapidCount = 0;
        PlayLayer::destroyPlayer(player, object);
    }

    void playEndAnimationToPos(cocos2d::CCPoint pos) {
        if (ib::S().simulating) return;   // el fantasma no debe terminar el nivel
        PlayLayer::playEndAnimationToPos(pos);
    }
};
