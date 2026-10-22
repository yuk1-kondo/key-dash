#pragma once
#include "base_screen.h"
#include "typing_viewmodel.h"
#include <array>
#include <string>
namespace screen {
class TypingScreen final : public BaseScreen {
public:
    TypingScreen(viewmodel::BaseViewModel& shell, app::AssetManager& assets, viewmodel::TypingViewModel& typing);
    ~TypingScreen() override;
    void refresh(uint64_t now);
protected:
    void build_content(lv_obj_t* content) override;
private:
    void rebuild();
    void home();
    void game();
    void results();
    void help();
    void footer(const char* first, const char* second, const char* third);
    lv_obj_t* rectangle(lv_obj_t* parent, int x, int y, int width, int height, uint32_t color, int radius = 0);
    lv_obj_t* text(lv_obj_t* parent, int x, int y, const char* value, const lv_font_t* font, uint32_t color);
    static void set_text(lv_obj_t* label, const char* value);
    viewmodel::TypingViewModel& typing_;
    lv_obj_t* content_{nullptr};
    lv_obj_t* timer_{nullptr};
    lv_obj_t* speed_{nullptr};
    lv_obj_t* words_{nullptr};
    lv_obj_t* accuracy_{nullptr};
    lv_obj_t* typed_{nullptr};
    lv_obj_t* message_{nullptr};
    lv_obj_t* progress_{nullptr};
    lv_obj_t* cursor_{nullptr};
    std::array<lv_obj_t*, 8> tiles_{};
    std::array<lv_obj_t*, 8> letters_{};
    lv_observer_t* observer_{nullptr};
    model::TypingPage visible_page_{model::TypingPage::Help};
    model::WordPack visible_pack_{model::WordPack::Code};
    model::TypingMode visible_mode_{model::TypingMode::Practice};
    bool daylight_{true}, storage_warning_{false};
    uint32_t background_{0}, surface_{0}, ink_{0}, muted_{0}, accent_{0}, danger_{0};
};
}
