// Interfaz tactil (estilo QOLMod / Eclipse) para el Predictive Input Buffer.
//
//  * Boton flotante "IB" dentro del nivel: un toque cambia la etapa
//    (sustituye a la tecla F6 de PC) y se puede arrastrar a cualquier sitio.
//  * Boton "IB" en el menu de pausa y en el menu principal: abre el menu completo.
//
// Solo usa cocos2d + clases basicas de GD (CCLayer, CCMenu, ButtonSprite...),
// sin depender de las clases Popup de Geode, para que compile igual en todas las versiones.

#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>

#include <algorithm>
#include <string>

#include "state.hpp"

using namespace geode::prelude;

namespace {

    // =======================================================================
    //  Boton flotante dentro del nivel
    // =======================================================================
    class IBHud : public CCLayer {
    public:
        static IBHud* s_inst;

        static constexpr float kW = 70.f;
        static constexpr float kH = 30.f;

        static IBHud* create() {
            auto ret = new IBHud();
            if (ret && ret->setup()) {
                ret->autorelease();
                return ret;
            }
            delete ret;
            return nullptr;
        }

        ~IBHud() override {
            if (s_inst == this) s_inst = nullptr;
        }

        bool setup() {
            if (!CCLayer::init()) return false;

            this->setContentSize({kW, kH});

            m_bg = CCLayerColor::create(ccc4(0, 0, 0, 160), kW, kH);
            this->addChild(m_bg);

            m_label = CCLabelBMFont::create("IB", "bigFont.fnt");
            m_label->setScale(0.45f);
            m_label->setPosition({kW / 2.f, kH / 2.f});
            this->addChild(m_label);

            auto win = CCDirector::sharedDirector()->getWinSize();
            const float x = Mod::get()->getSavedValue<float>("hud-x", 12.f);
            const float y = Mod::get()->getSavedValue<float>("hud-y", win.height - kH - 12.f);
            this->setPosition(clampPos({x, y}));

            this->setTouchEnabled(true);   // (sin update por fotograma: solo se programa durante el parpadeo)

            s_inst = this;
            refresh();
            return true;
        }

        CCPoint clampPos(CCPoint p) const {
            auto win = CCDirector::sharedDirector()->getWinSize();
            p.x = std::clamp(p.x, 0.f, win.width - kW);
            p.y = std::clamp(p.y, 0.f, win.height - kH);
            return p;
        }

        // actualiza texto / visibilidad / opacidad
        void refresh() {
            const int a = ib::cfgOpacity() * 255 / 100;
            this->setVisible(ib::cfgShowButton());
            m_bg->setOpacity(static_cast<unsigned char>(a * 3 / 4));
            m_label->setOpacity(static_cast<unsigned char>(a));
            std::string text = std::string("IB ") + ib::kStages[ib::S().stage].label;
            m_label->setString(text.c_str());

            // que el texto siempre quepa dentro del boton
            const float cw = m_label->getContentSize().width;
            float scale = 0.45f;
            if (cw > 0.f && cw * scale > kW - 10.f) scale = (kW - 10.f) / cw;
            m_label->setScale(scale);
        }

        // parpadeo verde cuando el buffer retrasa un input
        // (el update por fotograma solo existe mientras dura el parpadeo: cero coste en reposo)
        void flash() {
            m_flash = 0.2f;
            m_bg->setColor({30, 150, 60});
            if (!m_updating) {
                m_updating = true;
                this->scheduleUpdate();
            }
        }

        void update(float dt) override {
            m_flash -= dt;
            if (m_flash <= 0.f) {
                m_bg->setColor({0, 0, 0});
                m_updating = false;
                this->unscheduleUpdate();
            }
        }

        // ----- tactil: arrastrar = mover, tocar = cambiar etapa -----
        void registerWithTouchDispatcher() override {
            CCDirector::sharedDirector()->getTouchDispatcher()->addTargetedDelegate(this, -128, true);
        }

        bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
            if (!this->isVisible() || !ib::cfgShowButton()) return false;
            const CCPoint local = this->convertToNodeSpace(touch->getLocation());
            if (local.x < 0.f || local.y < 0.f || local.x > kW || local.y > kH) return false;
            m_dragging = false;
            m_start = touch->getLocation();
            const CCPoint pos = this->getPosition();
            m_offset = {pos.x - m_start.x, pos.y - m_start.y};
            return true;
        }

        void ccTouchMoved(CCTouch* touch, CCEvent*) override {
            const CCPoint loc = touch->getLocation();
            const float dx = loc.x - m_start.x;
            const float dy = loc.y - m_start.y;
            if (!m_dragging && (dx * dx + dy * dy) > 100.f) m_dragging = true;   // > 10 px
            if (m_dragging) {
                this->setPosition(clampPos({loc.x + m_offset.x, loc.y + m_offset.y}));
            }
        }

        void ccTouchEnded(CCTouch*, CCEvent*) override {
            if (m_dragging) {
                const CCPoint pos = this->getPosition();
                Mod::get()->setSavedValue<float>("hud-x", pos.x);
                Mod::get()->setSavedValue<float>("hud-y", pos.y);
            } else {
                ib::cycleStage();
            }
            m_dragging = false;
        }

        void ccTouchCancelled(CCTouch*, CCEvent*) override {
            m_dragging = false;
        }

    private:
        CCLayerColor* m_bg = nullptr;
        CCLabelBMFont* m_label = nullptr;
        CCPoint m_start = {0.f, 0.f};
        CCPoint m_offset = {0.f, 0.f};
        bool m_dragging = false;
        float m_flash = 0.f;
        bool m_updating = false;
    };

    IBHud* IBHud::s_inst = nullptr;

    // =======================================================================
    //  Menu completo (popup)
    // =======================================================================
    class IBMenu : public CCLayerColor {
    public:
        static IBMenu* create() {
            auto ret = new IBMenu();
            if (ret && ret->setup()) {
                ret->autorelease();
                return ret;
            }
            delete ret;
            return nullptr;
        }

        bool setup() {
            if (!CCLayerColor::initWithColor(ccc4(0, 0, 0, 140))) return false;
            this->setTouchEnabled(true);
            this->setKeypadEnabled(true);

            auto win = CCDirector::sharedDirector()->getWinSize();
            m_panel = CCNode::create();
            m_panel->setPosition({(win.width - kW) / 2.f, (win.height - kH) / 2.f});
            this->addChild(m_panel);

            auto border = CCLayerColor::create(ccc4(90, 200, 120, 255), kW + 4.f, kH + 4.f);
            border->setPosition({-2.f, -2.f});
            m_panel->addChild(border);

            auto bg = CCLayerColor::create(ccc4(18, 22, 30, 245), kW, kH);
            m_panel->addChild(bg);

            addLabel("Predictive Input Buffer", "goldFont.fnt", 0.62f, {kW / 2.f, kH - 22.f});
            addLabel("Stage  (ticks: All / Wave)", "chatFont.fnt", 0.85f, {kW / 2.f, kH - 48.f});

            // resaltado de la etapa activa (va detras de los botones)
            m_sel = CCLayerColor::create(ccc4(80, 230, 110, 255), 60.f, 36.f);
            m_panel->addChild(m_sel);

            m_menu = CCMenu::create();
            m_menu->setPosition({0.f, 0.f});
            m_menu->setTouchPriority(-701);
            m_panel->addChild(m_menu);

            // --- botones de etapa ---
            for (int i = 0; i < ib::kStageCount; ++i) {
                auto spr = ButtonSprite::create(ib::kStages[i].label, 50, true, "bigFont.fnt",
                                                "GJ_button_04.png", 28.f, 0.5f);
                auto btn = CCMenuItemSpriteExtra::create(spr, nullptr, this, menu_selector(IBMenu::onStage));
                btn->setTag(i);
                btn->setPosition({stageX(i), kStageY});
                m_menu->addChild(btn);
            }

            m_desc = addLabel("", "chatFont.fnt", 0.8f, {kW / 2.f, kStageY - 30.f});

            // --- ajustes ---
            addToggle("Floating button (in level)", 140.f, ib::cfgShowButton(),
                      menu_selector(IBMenu::onToggleHud));
            addToggle("Log saves to Geode log", 114.f, ib::cfgLogSaves(),
                      menu_selector(IBMenu::onToggleLog));

            m_opLabel = addLabel("", "bigFont.fnt", 0.38f, {26.f, 88.f}, {0.f, 0.5f});
            auto minus = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("-", 28, true, "bigFont.fnt", "GJ_button_01.png", 26.f, 0.7f),
                nullptr, this, menu_selector(IBMenu::onOpacity));
            minus->setTag(-1);
            minus->setPosition({kW - 80.f, 88.f});
            m_menu->addChild(minus);
            auto plus = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("+", 28, true, "bigFont.fnt", "GJ_button_01.png", 26.f, 0.7f),
                nullptr, this, menu_selector(IBMenu::onOpacity));
            plus->setTag(1);
            plus->setPosition({kW - 40.f, 88.f});
            m_menu->addChild(plus);

            // --- rendimiento: presupuesto de CPU por clic ---
            m_budLabel = addLabel("", "bigFont.fnt", 0.38f, {26.f, 62.f}, {0.f, 0.5f});
            auto budMinus = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("-", 28, true, "bigFont.fnt", "GJ_button_01.png", 26.f, 0.7f),
                nullptr, this, menu_selector(IBMenu::onBudget));
            budMinus->setTag(-1);
            budMinus->setPosition({kW - 80.f, 62.f});
            m_menu->addChild(budMinus);
            auto budPlus = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("+", 28, true, "bigFont.fnt", "GJ_button_01.png", 26.f, 0.7f),
                nullptr, this, menu_selector(IBMenu::onBudget));
            budPlus->setTag(1);
            budPlus->setPosition({kW - 40.f, 62.f});
            m_menu->addChild(budPlus);

            // --- cerrar ---
            auto ok = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("OK", 80, true, "goldFont.fnt", "GJ_button_01.png", 30.f, 0.8f),
                nullptr, this, menu_selector(IBMenu::onClose));
            ok->setPosition({kW / 2.f, 24.f});
            m_menu->addChild(ok);

            refresh();
            return true;
        }

        void refresh() {
            const int s = ib::S().stage;
            m_sel->setPosition({stageX(s) - 30.f, kStageY - 18.f});
            m_desc->setString(ib::kStages[s].desc);
            std::string op = "Button opacity: " + std::to_string(ib::cfgOpacity()) + "%";
            m_opLabel->setString(op.c_str());
            std::string bud = "CPU budget / click: " + std::to_string(ib::cfgBudgetMs()) + " ms";
            m_budLabel->setString(bud.c_str());
        }

        // ----- callbacks -----
        void onStage(CCObject* sender) {
            ib::setStage(static_cast<CCNode*>(sender)->getTag());
            this->refresh();
        }

        void onToggleHud(CCObject* sender) {
            const bool now = !static_cast<CCMenuItemToggler*>(sender)->isToggled();
            ib::cfgSetShowButton(now);
            ib::onStageChanged();
        }

        void onToggleLog(CCObject* sender) {
            const bool now = !static_cast<CCMenuItemToggler*>(sender)->isToggled();
            ib::cfgSetLogSaves(now);
        }

        void onBudget(CCObject* sender) {
            const int dir = static_cast<CCNode*>(sender)->getTag();
            ib::cfgSetBudgetMs(ib::cfgBudgetMs() + dir);
            this->refresh();
        }

        void onOpacity(CCObject* sender) {
            const int dir = static_cast<CCNode*>(sender)->getTag();
            ib::cfgSetOpacity(ib::cfgOpacity() + dir * 10);
            ib::onStageChanged();
            this->refresh();
        }

        void onClose(CCObject*) {
            this->removeFromParentAndCleanup(true);
        }

        // ----- tactil / tecla atras de Android -----
        void registerWithTouchDispatcher() override {
            CCDirector::sharedDirector()->getTouchDispatcher()->addTargetedDelegate(this, -700, true);
        }

        bool ccTouchBegan(CCTouch*, CCEvent*) override { return true; }   // bloquea lo de detras

        void keyBackClicked() override { this->onClose(nullptr); }

    private:
        static constexpr float kW = 340.f;
        static constexpr float kH = 282.f;
        static constexpr float kStageY = 198.f;

        static float stageX(int i) { return kW / 2.f + (i - 2) * 62.f; }

        CCLabelBMFont* addLabel(const char* text, const char* font, float scale, CCPoint pos,
                                CCPoint anchor = {0.5f, 0.5f}) {
            auto lbl = CCLabelBMFont::create(text, font);
            lbl->setScale(scale);
            lbl->setAnchorPoint(anchor);
            lbl->setPosition(pos);
            m_panel->addChild(lbl);
            return lbl;
        }

        void addToggle(const char* text, float y, bool on, SEL_MenuHandler cb) {
            addLabel(text, "bigFont.fnt", 0.38f, {26.f, y}, {0.f, 0.5f});
            auto toggler = CCMenuItemToggler::createWithStandardSprites(this, cb, 0.6f);
            toggler->toggle(on);
            toggler->setPosition({kW - 40.f, y});
            m_menu->addChild(toggler);
        }

        CCNode* m_panel = nullptr;
        CCMenu* m_menu = nullptr;
        CCLayerColor* m_sel = nullptr;
        CCLabelBMFont* m_desc = nullptr;
        CCLabelBMFont* m_opLabel = nullptr;
        CCLabelBMFont* m_budLabel = nullptr;
    };

    // crea el boton "IB" que se mete en los menus del juego
    CCMenuItemSpriteExtra* makeIBButton(CCObject* target, SEL_MenuHandler cb) {
        auto spr = ButtonSprite::create("IB", 36, true, "bigFont.fnt", "GJ_button_01.png", 30.f, 0.7f);
        auto btn = CCMenuItemSpriteExtra::create(spr, nullptr, target, cb);
        btn->setID("ai.predictivebuffer/ib-button");
        return btn;
    }

}

// ---------------------------------------------------------------------------
//  API de interfaz usada por buffer.cpp
// ---------------------------------------------------------------------------
namespace ib {

    void onStageChanged() {
        if (IBHud::s_inst) IBHud::s_inst->refresh();
    }

    void onSaved(int) {
        if (IBHud::s_inst) IBHud::s_inst->flash();
    }

    void attachHud(CCNode* parent) {
        if (!parent) return;
        auto hud = IBHud::create();
        if (hud) parent->addChild(hud, 5000);
    }

    void openMenu() {
        auto scene = CCDirector::sharedDirector()->getRunningScene();
        if (!scene) return;
        if (scene->getChildByID("ai.predictivebuffer/menu")) return;
        auto menu = IBMenu::create();
        if (!menu) return;
        menu->setID("ai.predictivebuffer/menu");
        scene->addChild(menu, 100000);
    }

}

// ---------------------------------------------------------------------------
//  Puntos de entrada: menu principal y menu de pausa
// ---------------------------------------------------------------------------
class $modify(IBMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto btn = makeIBButton(this, menu_selector(IBMenuLayer::onIB));

        CCMenu* menu = nullptr;
        if (auto node = this->getChildByID("bottom-menu")) {
            menu = typeinfo_cast<CCMenu*>(node);
        }
        if (menu) {
            menu->addChild(btn);
            menu->updateLayout();
        } else {
            auto win = CCDirector::sharedDirector()->getWinSize();
            menu = CCMenu::create();
            menu->setPosition({0.f, 0.f});
            btn->setPosition({36.f, win.height - 30.f});
            menu->addChild(btn);
            this->addChild(menu, 100);
        }
        return true;
    }

    void onIB(CCObject*) {
        ib::openMenu();
    }
};

class $modify(IBPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto btn = makeIBButton(this, menu_selector(IBPauseLayer::onIB));

        CCMenu* menu = nullptr;
        if (auto node = this->getChildByID("right-button-menu")) {
            menu = typeinfo_cast<CCMenu*>(node);
        }
        if (menu) {
            menu->addChild(btn);
            menu->updateLayout();
        } else {
            auto win = CCDirector::sharedDirector()->getWinSize();
            menu = CCMenu::create();
            menu->setPosition({0.f, 0.f});
            btn->setPosition({win.width - 36.f, win.height - 30.f});
            menu->addChild(btn);
            this->addChild(menu, 100);
        }
    }

    void onIB(CCObject*) {
        ib::openMenu();
    }
};
