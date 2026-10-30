// Interfaz tactil (estilo QOLMod / Eclipse) para el Predictive Input Buffer.

#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/ui/ColorPickPopup.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "state.hpp"

using namespace geode::prelude;

namespace {

    ccColor3B intToColor(int hex) {
        return { 
            (GLubyte)((hex >> 16) & 0xFF), 
            (GLubyte)((hex >> 8) & 0xFF), 
            (GLubyte)(hex & 0xFF) 
        };
    }
    int colorToInt(ccColor3B c) {
        return (c.r << 16) | (c.g << 8) | c.b;
    }

    // Popup pequeño para elegir qué parte pintar
    class ColorTargetPopup : public Popup {
    protected:
        bool setup() override {
            this->setTitle("Select Target");
            auto winSize = m_mainLayer->getContentSize();
            
            auto menu = CCMenu::create();
            menu->setPosition({winSize.width / 2.f, winSize.height / 2.f - 10.f});
            m_mainLayer->addChild(menu);

            auto makeBtn = [&](const char* text, SEL_MenuHandler cb, float y) {
                auto btn = CCMenuItemSpriteExtra::create(
                    ButtonSprite::create(text, "bigFont.fnt", "GJ_button_04.png", 0.5f),
                    this, cb);
                btn->setPosition({0, y});
                menu->addChild(btn);
            };

            makeBtn("Background", menu_selector(ColorTargetPopup::onBg), 60.f);
            makeBtn("Border", menu_selector(ColorTargetPopup::onBorder), 20.f);
            makeBtn("Texts", menu_selector(ColorTargetPopup::onText), -20.f);
            makeBtn("Buttons", menu_selector(ColorTargetPopup::onBtn), -60.f);

            return true;
        }

    public:
        static ColorTargetPopup* create() {
            auto ret = new ColorTargetPopup();
            if (ret && ret->initAnchored(200.f, 220.f, "GJ_square01.png")) {
                ret->autorelease();
                return ret;
            }
            CC_SAFE_DELETE(ret);
            return nullptr;
        }

        void onBg(CCObject*) {
            auto popup = ColorPickPopup::create(intToColor(ib::cfgColorBg()), [](ccColor3B const& c) {
                ib::cfgSetColorBg(colorToInt(c));
                ib::onStageChanged();
            });
            popup->show();
            this->onClose(nullptr);
        }
        void onBorder(CCObject*) {
            auto popup = ColorPickPopup::create(intToColor(ib::cfgColorBorder()), [](ccColor3B const& c) {
                ib::cfgSetColorBorder(colorToInt(c));
                ib::onStageChanged();
            });
            popup->show();
            this->onClose(nullptr);
        }
        void onText(CCObject*) {
            auto popup = ColorPickPopup::create(intToColor(ib::cfgColorText()), [](ccColor3B const& c) {
                ib::cfgSetColorText(colorToInt(c));
                ib::onStageChanged();
            });
            popup->show();
            this->onClose(nullptr);
        }
        void onBtn(CCObject*) {
            auto popup = ColorPickPopup::create(intToColor(ib::cfgColorBtn()), [](ccColor3B const& c) {
                ib::cfgSetColorBtn(colorToInt(c));
                ib::onStageChanged();
            });
            popup->show();
            this->onClose(nullptr);
        }
    };

    class IBHud : public CCLayer {
    public:
        static IBHud* s_inst;

        static constexpr float kW = 62.f;
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

            m_label = CCLabelBMFont::create("PB", "bigFont.fnt");
            m_label->setScale(0.45f);
            m_label->setPosition({kW / 2.f, kH / 2.f});
            this->addChild(m_label);

            auto win = CCDirector::sharedDirector()->getWinSize();
            const float x = Mod::get()->getSavedValue<float>("hud-x", 12.f);
            const float y = Mod::get()->getSavedValue<float>("hud-y", win.height - kH - 12.f);
            this->setPosition(clampPos({x, y}));

            this->setTouchEnabled(true);
            this->scheduleUpdate();

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

        void refresh() {
            bool show = ib::cfgShowButton();
            this->setVisible(show);
            
            if (show) {
                const int a = ib::cfgOpacity() * 255 / 100;
                m_bg->setVisible(ib::cfgShowBg());
                m_bg->setOpacity(static_cast<unsigned char>(a * 3 / 4));
                m_label->setOpacity(static_cast<unsigned char>(a));
                std::string text = std::string("PB ") + ib::kStages[ib::S().stage].label;
                m_label->setString(text.c_str());
            }
        }

        void flash() {
            if (!ib::cfgShowBg()) return;
            m_flash = 0.2f;
            m_bg->setColor({30, 150, 60});
        }

        void update(float dt) override {
            if (m_flash > 0.f) {
                m_flash -= dt;
                if (m_flash <= 0.f) m_bg->setColor({0, 0, 0});
            }
        }

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
            m_offset = CCPoint(pos.x - m_start.x, pos.y - m_start.y);
            return true;
        }

        void ccTouchMoved(CCTouch* touch, CCEvent*) override {
            const CCPoint loc = touch->getLocation();
            const float dx = loc.x - m_start.x;
            const float dy = loc.y - m_start.y;
            if (!m_dragging && (dx * dx + dy * dy) > 100.f) m_dragging = true;
            if (m_dragging) {
                this->setPosition(clampPos(CCPoint(loc.x + m_offset.x, loc.y + m_offset.y)));
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
    };

    IBHud* IBHud::s_inst = nullptr;

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

            m_borderSpr = CCScale9Sprite::create("GJ_square01.png");
            m_borderSpr->setContentSize({kW + 6.f, kH + 6.f});
            m_borderSpr->setPosition({kW / 2.f, kH / 2.f});
            m_panel->addChild(m_borderSpr);

            m_bgSpr = CCScale9Sprite::create("GJ_square01.png");
            m_bgSpr->setContentSize({kW, kH});
            m_bgSpr->setPosition({kW / 2.f, kH / 2.f});
            m_panel->addChild(m_bgSpr);

            m_titleLabel = addLabel("Predictive Input Buffer", "bigFont.fnt", 0.62f, {kW / 2.f - 30.f, kH - 22.f});
            addLabel("Stage  (ticks: All / Wave)", "chatFont.fnt", 0.85f, {kW / 2.f, kH - 48.f});

            m_menu = CCMenu::create();
            m_menu->setPosition({0.f, 0.f});
            m_menu->setTouchPriority(-701);
            m_panel->addChild(m_menu);

            // --- BOTÓN PALETA (Dibujado en código) ---
            auto paletteDraw = CCDrawNode::create();
            // Base blanca de la paleta
            paletteDraw->drawDot({0, 0}, 14.f, {255, 255, 255, 255});
            // Pintitas de colores
            paletteDraw->drawDot({-6, 6}, 3.f, {255, 0, 0, 255});   // Rojo
            paletteDraw->drawDot({6, 6}, 3.f, {0, 255, 0, 255});    // Verde
            paletteDraw->drawDot({6, -6}, 3.f, {0, 0, 255, 255});   // Azul
            paletteDraw->drawDot({-6, -6}, 3.f, {255, 255, 0, 255}); // Amarillo
            // Agujero central
            paletteDraw->drawDot({0, 0}, 4.f, {0, 0, 0, 0}); 
            
            auto paletteBtn = CCMenuItemSpriteExtra::create(
                paletteDraw, this, menu_selector(IBMenu::onPalette));
            paletteBtn->setPosition({kW - 35.f, kH - 25.f});
            m_menu->addChild(paletteBtn);
            // ----------------------------------------

            for (int i = 0; i < ib::kStageCount; ++i) {
                auto spr = ButtonSprite::create(ib::kStages[i].label, 50, true, "bigFont.fnt",
                                                "GJ_button_04.png", 28.f, 0.5f);
                auto btn = CCMenuItemSpriteExtra::create(spr, nullptr, this, menu_selector(IBMenu::onStage));
                btn->setTag(i);
                btn->setPosition({stageX(i), kStageY});
                m_menu->addChild(btn);
                m_stageBtns[i] = btn; 
                m_allButtons.push_back(btn);
            }

            m_desc = addLabel("", "chatFont.fnt", 0.8f, {kW / 2.f, kStageY - 30.f});

            addToggle("Floating button (in level)", 160.f, ib::cfgShowButton(),
                      menu_selector(IBMenu::onToggleHud));
            addToggle("Floating background", 134.f, ib::cfgShowBg(),
                      menu_selector(IBMenu::onToggleBg));

            m_opLabel = addLabel("", "bigFont.fnt", 0.38f, {26.f, 88.f}, {0.f, 0.5f});
            
            auto minus = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("-", 28, true, "bigFont.fnt", "GJ_button_04.png", 26.f, 0.7f),
                nullptr, this, menu_selector(IBMenu::onOpacity));
            minus->setTag(-1);
            minus->setPosition({kW - 85.f, 88.f}); 
            m_menu->addChild(minus);
            m_allButtons.push_back(minus);
            
            auto plus = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("+", 28, true, "bigFont.fnt", "GJ_button_04.png", 26.f, 0.7f),
                nullptr, this, menu_selector(IBMenu::onOpacity));
            plus->setTag(1);
            plus->setPosition({kW - 35.f, 88.f}); 
            m_menu->addChild(plus);
            m_allButtons.push_back(plus);

            auto ok = CCMenuItemSpriteExtra::create(
                ButtonSprite::create("OK", 80, true, "bigFont.fnt", "GJ_button_04.png", 30.f, 0.8f),
                nullptr, this, menu_selector(IBMenu::onClose));
            ok->setPosition({kW / 2.f, 30.f});
            m_menu->addChild(ok);
            m_allButtons.push_back(ok);

            refresh();
            applyColors();

            // --- ANIMACIÓN DE APERTURA ---
            this->setOpacity(0);
            m_panel->setScale(0.5f);
            m_panel->setOpacity(0);
            
            auto fadeIn = CCFadeTo::create(0.15f, 255);
            auto panelFadeIn = CCFadeTo::create(0.15f, 255);
            auto scaleIn = CCScaleTo::create(0.2f, 1.0f);
            auto easeScaleIn = CCEaseBackOut::create(scaleIn);
            
            this->runAction(fadeIn);
            m_panel->runAction(CCSpawn::create(panelFadeIn, easeScaleIn, nullptr));

            return true;
        }

        void applyColors() {
            m_bgSpr->setColor(intToColor(ib::cfgColorBg()));
            m_borderSpr->setColor(intToColor(ib::cfgColorBorder()));
            
            for (auto lbl : m_allLabels) {
                lbl->setColor(intToColor(ib::cfgColorText()));
            }

            for (size_t i = 0; i < m_allButtons.size(); ++i) {
                // Si no es el botón de la etapa activa, lo pintamos
                if (i < ib::kStageCount && i == ib::S().stage) continue;
                m_allButtons[i]->setColor(intToColor(ib::cfgColorBtn()));
            }
        }

        void refresh() {
            const int s = ib::S().stage;

            for (int i = 0; i < ib::kStageCount; ++i) {
                if (m_stageBtns[i]) {
                    if (i == s) {
                        m_stageBtns[i]->setColor(ccc3(100, 255, 100)); 
                        m_stageBtns[i]->setScale(1.1f);                
                    } else {
                        m_stageBtns[i]->setColor(intToColor(ib::cfgColorBtn())); 
                        m_stageBtns[i]->setScale(1.0f);                
                    }
                }
            }

            m_desc->setString(ib::kStages[s].desc);
            std::string op = "Button opacity: " + std::to_string(ib::cfgOpacity()) + "%";
            m_opLabel->setString(op.c_str());
        }

        void onPalette(CCObject*) {
            auto popup = ColorTargetPopup::create();
            popup->show();
        }

        void onStage(CCObject* sender) {
            ib::setStage(static_cast<CCNode*>(sender)->getTag());
            this->refresh();
        }

        void onToggleHud(CCObject* sender) {
            const bool now = !static_cast<CCMenuItemToggler*>(sender)->isToggled();
            ib::cfgSetShowButton(now);
            ib::onStageChanged();
        }

        void onToggleBg(CCObject* sender) {
            const bool now = !static_cast<CCMenuItemToggler*>(sender)->isToggled();
            ib::cfgSetShowBg(now);
            ib::onStageChanged();
        }

        void onOpacity(CCObject* sender) {
            const int dir = static_cast<CCNode*>(sender)->getTag();
            ib::cfgSetOpacity(ib::cfgOpacity() + dir * 10);
            ib::onStageChanged();
            this->refresh();
        }

        void onClose(CCObject*) {
            this->setTouchEnabled(false);
            auto fadeOut = CCFadeTo::create(0.15f, 0);
            auto panelFadeOut = CCFadeTo::create(0.15f, 0);
            auto scaleOut = CCScaleTo::create(0.15f, 0.7f);
            auto easeScaleOut = CCEaseBackIn::create(scaleOut);
            auto cleanup = CCCallFunc::create(this, callfunc_selector(IBMenu::doCleanup));
            this->runAction(CCSequence::create(fadeOut, cleanup, nullptr));
            m_panel->runAction(CCSpawn::create(panelFadeOut, easeScaleOut, nullptr));
        }

        void doCleanup() {
            this->removeFromParentAndCleanup(true);
        }

        void registerWithTouchDispatcher() override {
            CCDirector::sharedDirector()->getTouchDispatcher()->addTargetedDelegate(this, -700, true);
        }

        bool ccTouchBegan(CCTouch*, CCEvent*) override { return true; }
        void keyBackClicked() override { this->onClose(nullptr); }

    private:
        static constexpr float kW = 440.f;
        static constexpr float kH = 300.f;
        static constexpr float kStageY = 218.f;

        static float stageX(int i) { return kW / 2.f + (i - 2.5f) * 72.f; }

        CCLabelBMFont* addLabel(const char* text, const char* font, float scale, CCPoint pos,
                                CCPoint anchor = {0.5f, 0.5f}) {
            auto lbl = CCLabelBMFont::create(text, font);
            lbl->setScale(scale);
            lbl->setAnchorPoint(anchor);
            lbl->setPosition(pos);
            m_panel->addChild(lbl);
            m_allLabels.push_back(lbl);
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
        CCScale9Sprite* m_bgSpr = nullptr;
        CCScale9Sprite* m_borderSpr = nullptr;
        CCMenuItemSpriteExtra* m_stageBtns[6] = {nullptr}; 
        CCLabelBMFont* m_titleLabel = nullptr;
        CCLabelBMFont* m_desc = nullptr;
        CCLabelBMFont* m_opLabel = nullptr;
        std::vector<CCLabelBMFont*> m_allLabels;
        std::vector<CCMenuItemSpriteExtra*> m_allButtons;
    };

    CCMenuItemSpriteExtra* makeIBButton(CCObject* target, SEL_MenuHandler cb) {
        auto spr = ButtonSprite::create("PB", 36, true, "bigFont.fnt", "GJ_button_04.png", 30.f, 0.7f);
        auto btn = CCMenuItemSpriteExtra::create(spr, nullptr, target, cb);
        btn->setID("ai.predictivebuffer/ib-button");
        return btn;
    }

}

namespace ib {
    void onStageChanged() {
        if (IBHud::s_inst) IBHud::s_inst->refresh();
        if (auto scene = CCDirector::sharedDirector()->getRunningScene()) {
            if (auto menu = scene->getChildByID("ai.predictivebuffer/menu")) {
                static_cast<IBMenu*>(menu)->applyColors();
                static_cast<IBMenu*>(menu)->refresh();
            }
        }
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
    void onIB(CCObject*) { ib::openMenu(); }
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
    void onIB(CCObject*) { ib::openMenu(); }
};
