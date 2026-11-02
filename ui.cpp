// Interfaz tactil para Predictive Input Buffer.

#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/ui/ColorPickPopup.hpp>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

#include "state.hpp"

using namespace geode::prelude;

namespace {

    ccColor3B intToColor(int hex) {
        return {
            static_cast<GLubyte>((hex >> 16) & 0xFF),
            static_cast<GLubyte>((hex >> 8) & 0xFF),
            static_cast<GLubyte>(hex & 0xFF)
        };
    }

    int colorToInt(ccColor3B c) {
        return (c.r << 16) | (c.g << 8) | c.b;
    }

    void updateBtnText(CCMenuItemSpriteExtra* btn, ccColor3B col) {
        if (!btn) return;

        if (auto spr = btn->getChildByType<ButtonSprite>(0)) {
            if (auto lbl = spr->getChildByType<CCLabelBMFont>(0)) {
                if (col.r > 200 && col.g > 200 && col.b > 200) {
                    lbl->setColor(ccc3(0, 0, 0));
                } else {
                    lbl->setColor(ccc3(255, 255, 255));
                }
            }
        }
    }

    CCDrawNode* createPanelFill(float width, float height, ccColor3B color) {
        auto node = CCDrawNode::create();
        if (!node) return nullptr;

        ccColor4F fill = {
            color.r / 255.f,
            color.g / 255.f,
            color.b / 255.f,
            1.f
        };

        node->drawRect({0, 0}, {width, height}, fill, 0.f, fill);
        return node;
    }

    class ColorTargetLayer : public CCLayer {
    protected:
        CCLayerColor* m_dimBg = nullptr;
        CCLayer* m_panel = nullptr;
        CCDrawNode* m_bgFill = nullptr;
        bool m_closing = false;

        bool init() {
            if (!CCLayer::init()) return false;

            this->setTouchEnabled(true);
            this->setKeypadEnabled(true);

            auto winSize = CCDirector::sharedDirector()->getWinSize();

            m_dimBg = CCLayerColor::create(ccc4(0, 0, 0, 150), winSize.width, winSize.height);
            if (!m_dimBg) return false;
            this->addChild(m_dimBg, -1);

            const float kW = 220.f;
            const float kH = 300.f;

            m_panel = CCLayer::create();
            if (!m_panel) return false;

            m_panel->setContentSize({kW, kH});
            m_panel->setAnchorPoint({0.f, 0.f});
            m_panel->setPosition({(winSize.width - kW) / 2.f, (winSize.height - kH) / 2.f});
            this->addChild(m_panel);

            auto border = CCScale9Sprite::create("GJ_square01.png");
            if (border) {
                border->setContentSize({kW, kH});
                border->setColor(intToColor(ib::cfgColorBorder()));
                border->setPosition({kW / 2.f, kH / 2.f});
                m_panel->addChild(border, 0);
            }

            m_bgFill = createPanelFill(kW - 6.f, kH - 6.f, intToColor(ib::cfgColorBg()));
            if (m_bgFill) {
                m_bgFill->setPosition({3.f, 3.f});
                m_panel->addChild(m_bgFill, 1);
            }

            auto title = CCLabelBMFont::create("Select Target", "bigFont.fnt");
            if (title) {
                title->setScale(0.7f);
                title->setColor(intToColor(ib::cfgColorText()));
                title->setPosition({kW / 2.f, 260.f});
                m_panel->addChild(title, 2);
            }

            auto menu = CCMenu::create();
            if (!menu) return false;
            menu->setPosition({kW / 2.f, 130.f});
            menu->setTouchPriority(-701);
            m_panel->addChild(menu, 3);

            auto makeBtn = [&](const char* text, SEL_MenuHandler cb, float y) {
                auto spr = ButtonSprite::create(text, "bigFont.fnt", "GJ_button_04.png", 0.5f);
                if (!spr) return;
                spr->setCascadeColorEnabled(true);
                auto btn = CCMenuItemSpriteExtra::create(spr, this, cb);
                if (!btn) return;
                btn->setColor(intToColor(ib::cfgColorBtn()));
                updateBtnText(btn, intToColor(ib::cfgColorBtn()));
                btn->setPosition({0.f, y});
                menu->addChild(btn);
            };

            makeBtn("Background", menu_selector(ColorTargetLayer::onBg), 80.f);
            makeBtn("Border", menu_selector(ColorTargetLayer::onBorder), 20.f);
            makeBtn("Texts", menu_selector(ColorTargetLayer::onText), -40.f);
            makeBtn("Buttons", menu_selector(ColorTargetLayer::onBtn), -100.f);

            auto sprClose = ButtonSprite::create("X", "bigFont.fnt", "GJ_button_04.png", 0.5f);
            if (sprClose) {
                sprClose->setCascadeColorEnabled(true);
                auto closeBtn = CCMenuItemSpriteExtra::create(sprClose, this, menu_selector(ColorTargetLayer::onClose));
                if (closeBtn) {
                    closeBtn->setColor(intToColor(ib::cfgColorBtn()));
                    updateBtnText(closeBtn, intToColor(ib::cfgColorBtn()));
                    closeBtn->setPosition({100.f, 135.f});
                    menu->addChild(closeBtn);
                }
            }

            m_panel->setScale(0.5f);
            m_dimBg->setOpacity(0);
            m_dimBg->runAction(CCFadeTo::create(0.15f, 150));
            m_panel->runAction(CCEaseBackOut::create(CCScaleTo::create(0.2f, 1.f)));

            return true;
        }

    public:
        static ColorTargetLayer* create() {
            auto ret = new ColorTargetLayer();
            if (ret && ret->init()) {
                ret->autorelease();
                return ret;
            }
            delete ret;
            return nullptr;
        }

        void openColorPicker(int currentColor, std::function<void(ccColor3B)> callback) {
            auto popup = ColorPickPopup::create(intToColor(currentColor));
            if (!popup) return;
            popup->setCallback(geode::Function<void(cocos2d::ccColor4B const&)>([callback](cocos2d::ccColor4B const& c) {
                callback({c.r, c.g, c.b});
            }));
            popup->show();
            this->onClose(nullptr);
        }

        void onBg(CCObject*) {
            openColorPicker(ib::cfgColorBg(), [](ccColor3B c) {
                ib::cfgSetColorBg(colorToInt(c));
                ib::onStageChanged();
            });
        }

        void onBorder(CCObject*) {
            openColorPicker(ib::cfgColorBorder(), [](ccColor3B c) {
                ib::cfgSetColorBorder(colorToInt(c));
                ib::onStageChanged();
            });
        }

        void onText(CCObject*) {
            openColorPicker(ib::cfgColorText(), [](ccColor3B c) {
                ib::cfgSetColorText(colorToInt(c));
                ib::onStageChanged();
            });
        }

        void onBtn(CCObject*) {
            openColorPicker(ib::cfgColorBtn(), [](ccColor3B c) {
                ib::cfgSetColorBtn(colorToInt(c));
                ib::onStageChanged();
            });
        }

        void onClose(CCObject*) {
            if (m_closing) return;
            m_closing = true;
            this->setTouchEnabled(false);

            auto cleanup = CCCallFunc::create(this, callfunc_selector(ColorTargetLayer::doCleanup));
            m_dimBg->runAction(CCFadeTo::create(0.15f, 0));
            m_panel->runAction(CCSequence::create(CCEaseBackIn::create(CCScaleTo::create(0.15f, 0.7f)), cleanup, nullptr));
        }

        void doCleanup() {
            if (auto scene = CCDirector::sharedDirector()->getRunningScene()) {
                if (auto menu = scene->getChildByID("ai.predictivebuffer/menu")) {
                    menu->setVisible(true);
                }
            }
            this->removeFromParentAndCleanup(true);
        }

        void keyBackClicked() override {
            this->onClose(nullptr);
        }

        void registerWithTouchDispatcher() override {
            CCDirector::sharedDirector()->getTouchDispatcher()->addTargetedDelegate(this, -700, true);
        }

        bool ccTouchBegan(CCTouch*, CCEvent*) override {
            return true;
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
            if (!m_bg) return false;
            this->addChild(m_bg);

            m_label = CCLabelBMFont::create("PB", "bigFont.fnt");
            if (!m_label) return false;
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

        // Mantiene el HUD dentro de los limites del padre.
        // Esta implementacion supone que el padre no tiene rotacion.
        CCPoint clampPos(CCPoint p) {
            auto winSize = CCDirector::sharedDirector()->getWinSize();
            float minX = 0.f;
            float maxX = winSize.width - kW;
            float minY = 0.f;
            float maxY = winSize.height - kH;

            if (auto parent = this->getParent()) {
                CCPoint bl = parent->convertToNodeSpace({0.f, 0.f});
                CCPoint tr = parent->convertToNodeSpace({winSize.width, winSize.height});

                minX = std::min(bl.x, tr.x);
                maxX = std::max(bl.x, tr.x) - kW;
                minY = std::min(bl.y, tr.y);
                maxY = std::max(bl.y, tr.y) - kH;
            }

            if (maxX < minX) maxX = minX;
            if (maxY < minY) maxY = minY;

            p.x = std::clamp(p.x, minX, maxX);
            p.y = std::clamp(p.y, minY, maxY);
            return p;
        }

        void refresh() {
            const bool show = ib::cfgShowButton();
            this->setVisible(show);
            if (!show) return;

            int s = ib::S().stage;
            if (s < 0 || s >= ib::kStageCount) s = 0;

            const int a = ib::cfgOpacity() * 255 / 100;
            m_bg->setVisible(ib::cfgShowBg());
            m_bg->setOpacity(static_cast<unsigned char>(a * 3 / 4));
            m_label->setOpacity(static_cast<unsigned char>(a));

            std::string text = std::string("PB ") + ib::kStages[s].label;
            m_label->setString(text.c_str());
        }

        void flash() {
            if (!ib::cfgShowBg()) return;
            m_flash = 0.2f;
            m_bg->setColor({30, 150, 60});
        }

        void update(float dt) override {
            if (m_flash > 0.f) {
                m_flash -= dt;
                if (m_flash <= 0.f) {
                    m_flash = 0.f;
                    m_bg->setColor({0, 0, 0});
                }
            }
        }

        void registerWithTouchDispatcher() override {
            CCDirector::sharedDirector()->getTouchDispatcher()->addTargetedDelegate(this, -128, true);
        }

        bool ccTouchBegan(CCTouch* touch, CCEvent*) override {
            if (!this->isVisible() || !ib::cfgShowButton()) return false;

            auto parent = this->getParent();
            if (!parent) return false;

            CCPoint touchPos = parent->convertToNodeSpace(touch->getLocation());
            const CCPoint local = this->convertToNodeSpace(touch->getLocation());

            if (local.x < 0.f || local.y < 0.f || local.x > kW || local.y > kH) {
                return false;
            }

            m_dragging = false;
            m_start = touchPos;

            const CCPoint pos = this->getPosition();
            m_offset = CCPoint(pos.x - m_start.x, pos.y - m_start.y);

            return true;
        }

        void ccTouchMoved(CCTouch* touch, CCEvent*) override {
            auto parent = this->getParent();
            if (!parent) return;

            CCPoint loc = parent->convertToNodeSpace(touch->getLocation());

            const float dx = loc.x - m_start.x;
            const float dy = loc.y - m_start.y;

            if (!m_dragging && (dx * dx + dy * dy) > 100.f) {
                m_dragging = true;
            }

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

    class IBMenu : public CCLayer {
    public:
        static IBMenu* s_inst;

        static IBMenu* create() {
            auto ret = new IBMenu();
            if (ret && ret->setup()) {
                ret->autorelease();
                return ret;
            }
            delete ret;
            return nullptr;
        }

        ~IBMenu() override {
            if (s_inst == this) s_inst = nullptr;
        }

        bool setup() {
            if (!CCLayer::init()) return false;

            this->setTouchEnabled(true);
            this->setKeypadEnabled(true);

            auto win = CCDirector::sharedDirector()->getWinSize();

            m_dimBg = CCLayerColor::create(ccc4(0, 0, 0, 150), win.width, win.height);
            if (!m_dimBg) return false;
            this->addChild(m_dimBg, -1);

            m_panel = CCLayer::create();
            if (!m_panel) return false;

            m_panel->setContentSize({kW, kH});
            m_panel->setAnchorPoint({0.f, 0.f});
            m_panel->setPosition({(win.width - kW) / 2.f, (win.height - kH) / 2.f});
            this->addChild(m_panel);

            m_borderSpr = CCScale9Sprite::create("GJ_square01.png");
            if (m_borderSpr) {
                m_borderSpr->setContentSize({kW, kH});
                m_borderSpr->setColor(intToColor(ib::cfgColorBorder()));
                m_borderSpr->setPosition({kW / 2.f, kH / 2.f});
                m_panel->addChild(m_borderSpr, 0);
            }

            m_bgFill = createPanelFill(kW - 6.f, kH - 6.f, intToColor(ib::cfgColorBg()));
            if (m_bgFill) {
                m_bgFill->setPosition({3.f, 3.f});
                m_panel->addChild(m_bgFill, 1);
            }

            m_titleLabel = addLabel("Predictive Input Buffer", "bigFont.fnt", 0.62f, {kW / 2.f - 30.f, kH - 22.f});
            addLabel("Stage  (ticks: All / Wave)", "chatFont.fnt", 0.85f, {kW / 2.f, kH - 48.f});

            m_menu = CCMenu::create();
            if (!m_menu) return false;

            m_menu->setPosition({0.f, 0.f});
            m_menu->setTouchPriority(-701);
            m_panel->addChild(m_menu, 3);

            auto sprCol = ButtonSprite::create("COLORS", 60, true, "bigFont.fnt", "GJ_button_04.png", 24.f, 0.5f);
            if (sprCol) {
                sprCol->setCascadeColorEnabled(true);
                auto paletteBtn = CCMenuItemSpriteExtra::create(sprCol, this, menu_selector(IBMenu::onPalette));
                if (paletteBtn) {
                    paletteBtn->setPosition({kW - 45.f, kH - 22.f});
                    m_menu->addChild(paletteBtn);
                    m_allButtons.push_back(paletteBtn);
                }
            }

            for (int i = 0; i < ib::kStageCount; ++i) {
                auto spr = ButtonSprite::create(ib::kStages[i].label, 50, true, "bigFont.fnt", "GJ_button_04.png", 28.f, 0.5f);
                if (!spr) continue;

                spr->setCascadeColorEnabled(true);

                auto btn = CCMenuItemSpriteExtra::create(spr, nullptr, this, menu_selector(IBMenu::onStage));
                if (!btn) continue;

                btn->setTag(i);
                btn->setPosition({stageX(i), kStageY});
                m_menu->addChild(btn);

                m_stageBtns[i] = btn;
                m_allButtons.push_back(btn);
            }

            m_desc = addLabel("", "chatFont.fnt", 0.8f, {kW / 2.f, kStageY - 30.f});

            addToggle("Floating button (in level)", 160.f, ib::cfgShowButton(), menu_selector(IBMenu::onToggleHud));
            addToggle("Floating background", 134.f, ib::cfgShowBg(), menu_selector(IBMenu::onToggleBg));

            m_opLabel = addLabel("", "bigFont.fnt", 0.38f, {26.f, 88.f}, {0.f, 0.5f});

            auto sprMinus = ButtonSprite::create("-", 28, true, "bigFont.fnt", "GJ_button_04.png", 26.f, 0.7f);
            if (sprMinus) {
                sprMinus->setCascadeColorEnabled(true);

                auto minus = CCMenuItemSpriteExtra::create(sprMinus, nullptr, this, menu_selector(IBMenu::onOpacity));
                if (minus) {
                    minus->setTag(-1);
                    minus->setPosition({kW - 85.f, 88.f});
                    m_menu->addChild(minus);
                    m_allButtons.push_back(minus);
                }
            }

            auto sprPlus = ButtonSprite::create("+", 28, true, "bigFont.fnt", "GJ_button_04.png", 26.f, 0.7f);
            if (sprPlus) {
                sprPlus->setCascadeColorEnabled(true);

                auto plus = CCMenuItemSpriteExtra::create(sprPlus, nullptr, this, menu_selector(IBMenu::onOpacity));
                if (plus) {
                    plus->setTag(1);
                    plus->setPosition({kW - 35.f, 88.f});
                    m_menu->addChild(plus);
                    m_allButtons.push_back(plus);
                }
            }

            auto sprOk = ButtonSprite::create("OK", 80, true, "bigFont.fnt", "GJ_button_04.png", 30.f, 0.8f);
            if (sprOk) {
                sprOk->setCascadeColorEnabled(true);

                auto ok = CCMenuItemSpriteExtra::create(sprOk, nullptr, this, menu_selector(IBMenu::onClose));
                if (ok) {
                    ok->setPosition({kW / 2.f, 30.f});
                    m_menu->addChild(ok);
                    m_allButtons.push_back(ok);
                }
            }

            applyColors();
            refresh();

            m_panel->setScale(0.5f);
            m_dimBg->setOpacity(0);
            m_dimBg->runAction(CCFadeTo::create(0.15f, 150));
            m_panel->runAction(CCEaseBackOut::create(CCScaleTo::create(0.2f, 1.f)));

            s_inst = this;
            return true;
        }

        void applyColors() {
            if (m_bgFill) {
                m_bgFill->clear();

                auto col = intToColor(ib::cfgColorBg());

                ccColor4F fill = {
                    col.r / 255.f,
                    col.g / 255.f,
                    col.b / 255.f,
                    1.f
                };

                m_bgFill->drawRect({0, 0}, {kW - 6.f, kH - 6.f}, fill, 0.f, fill);
            }

            if (m_borderSpr) {
                m_borderSpr->setColor(intToColor(ib::cfgColorBorder()));
            }

            for (auto lbl : m_allLabels) {
                if (lbl) lbl->setColor(intToColor(ib::cfgColorText()));
            }

            for (auto btn : m_allButtons) {
                if (btn) {
                    btn->setColor(intToColor(ib::cfgColorBtn()));
                    updateBtnText(btn, intToColor(ib::cfgColorBtn()));
                }
            }
        }

        void refresh() {
            int s = ib::S().stage;
            if (s < 0 || s >= ib::kStageCount) s = 0;

            const ccColor3B btnCol = intToColor(ib::cfgColorBtn());

            for (int i = 0; i < ib::kStageCount; ++i) {
                if (!m_stageBtns[i]) continue;

                m_stageBtns[i]->setColor(btnCol);
                m_stageBtns[i]->setScale(i == s ? 1.1f : 1.f);
                updateBtnText(m_stageBtns[i], btnCol);
            }

            if (m_desc) {
                m_desc->setString(ib::kStages[s].desc);
            }

            if (m_opLabel) {
                std::string op = "Button opacity: " + std::to_string(ib::cfgOpacity()) + "%";
                m_opLabel->setString(op.c_str());
            }
        }

        void onPalette(CCObject*) {
            auto scene = CCDirector::sharedDirector()->getRunningScene();
            if (!scene) return;

            this->setVisible(false);

            auto popup = ColorTargetLayer::create();
            if (popup) {
                popup->setID("color-target-layer");
                scene->addChild(popup, 100000);
            } else {
                this->setVisible(true);
            }
        }

        void onStage(CCObject* sender) {
            ib::setStage(static_cast<CCNode*>(sender)->getTag());
            this->refresh();
        }

        void onToggleHud(CCObject* sender) {
            ib::cfgSetShowButton(static_cast<CCMenuItemToggler*>(sender)->isToggled());
            ib::onStageChanged();
        }

        void onToggleBg(CCObject* sender) {
            ib::cfgSetShowBg(static_cast<CCMenuItemToggler*>(sender)->isToggled());
            ib::onStageChanged();
        }

        void onOpacity(CCObject* sender) {
            ib::cfgSetOpacity(ib::cfgOpacity() + static_cast<CCNode*>(sender)->getTag() * 10);
            ib::onStageChanged();
            this->refresh();
        }

        void onClose(CCObject*) {
            if (m_closing) return;

            m_closing = true;
            this->setTouchEnabled(false);

            auto cleanup = CCCallFunc::create(this, callfunc_selector(IBMenu::doCleanup));

            m_dimBg->runAction(CCFadeTo::create(0.15f, 0));
            m_panel->runAction(CCSequence::create(CCEaseBackIn::create(CCScaleTo::create(0.15f, 0.7f)), cleanup, nullptr));
        }

        void doCleanup() {
            this->removeFromParentAndCleanup(true);
        }

        void registerWithTouchDispatcher() override {
            CCDirector::sharedDirector()->getTouchDispatcher()->addTargetedDelegate(this, -700, true);
        }

        bool ccTouchBegan(CCTouch*, CCEvent*) override {
            return true;
        }

        void keyBackClicked() override {
            this->onClose(nullptr);
        }

    private:
        static constexpr float kW = 440.f;
        static constexpr float kH = 300.f;
        static constexpr float kStageY = 218.f;

        static float stageX(int i) {
            return kW / 2.f + (i - 2.5f) * 72.f;
        }

        CCLabelBMFont* addLabel(
            const char* text,
            const char* font,
            float scale,
            CCPoint pos,
            CCPoint anchor = {0.5f, 0.5f}
        ) {
            auto lbl = CCLabelBMFont::create(text, font);
            if (!lbl) return nullptr;

            lbl->setScale(scale);
            lbl->setAnchorPoint(anchor);
            lbl->setPosition(pos);
            m_panel->addChild(lbl, 2);

            m_allLabels.push_back(lbl);
            return lbl;
        }

        void addToggle(const char* text, float y, bool on, SEL_MenuHandler cb) {
            addLabel(text, "bigFont.fnt", 0.38f, {26.f, y}, {0.f, 0.5f});

            auto toggler = CCMenuItemToggler::createWithStandardSprites(this, cb, 0.6f);
            if (!toggler) return;

            toggler->toggle(on);
            toggler->setPosition({kW - 40.f, y});
            m_menu->addChild(toggler);
        }

        CCLayerColor* m_dimBg = nullptr;
        CCLayer* m_panel = nullptr;
        CCMenu* m_menu = nullptr;
        CCDrawNode* m_bgFill = nullptr;
        CCScale9Sprite* m_borderSpr = nullptr;

        CCMenuItemSpriteExtra* m_stageBtns[ib::kStageCount] = {nullptr};

        CCLabelBMFont* m_titleLabel = nullptr;
        CCLabelBMFont* m_desc = nullptr;
        CCLabelBMFont* m_opLabel = nullptr;

        std::vector<CCLabelBMFont*> m_allLabels;
        std::vector<CCMenuItemSpriteExtra*> m_allButtons;

        bool m_closing = false;
    };

    IBMenu* IBMenu::s_inst = nullptr;

    CCMenuItemSpriteExtra* makeIBButton(CCObject* target, SEL_MenuHandler cb) {
        auto spr = ButtonSprite::create("PB", 36, true, "bigFont.fnt", "GJ_button_04.png", 30.f, 0.7f);
        if (!spr) return nullptr;

        spr->setCascadeColorEnabled(true);

        auto btn = CCMenuItemSpriteExtra::create(spr, nullptr, target, cb);
        if (!btn) return nullptr;

        btn->setID("ai.predictivebuffer/ib-button");
        return btn;
    }

}

namespace ib {

    void onStageChanged() {
        if (IBHud::s_inst) {
            IBHud::s_inst->refresh();
        }

        if (IBMenu::s_inst) {
            IBMenu::s_inst->applyColors();
            IBMenu::s_inst->refresh();
        }
    }

    void onSaved(int) {
        if (IBHud::s_inst) IBHud::s_inst->flash();
    }

    void attachHud(CCNode* parent) {
        if (!parent) return;
        if (parent->getChildByID("ai.predictivebuffer/hud")) return;

        auto hud = IBHud::create();
        if (hud) {
            hud->setID("ai.predictivebuffer/hud");
            parent->addChild(hud, 5000);
            hud->setPosition(hud->clampPos(hud->getPosition()));
        }
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
        if (!btn) return true;

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
            if (!menu) return true;

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
        if (!btn) return;

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
            if (!menu) return;

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
