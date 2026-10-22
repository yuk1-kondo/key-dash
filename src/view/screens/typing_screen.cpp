#include "typing_screen.h"
#include "monotonic_clock.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
namespace screen {
namespace {
void show(lv_obj_t* object, bool visible) {
    if (visible) lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
}
bool is_game(model::TypingPage page) { return page == model::TypingPage::Ready || page == model::TypingPage::Running; }
}
TypingScreen::TypingScreen(viewmodel::BaseViewModel& shell, app::AssetManager& assets, viewmodel::TypingViewModel& typing)
    : BaseScreen(shell, assets, true), typing_(typing) {
    init();
    observer_ = lv_subject_add_observer(typing_.revision_subject(), [](lv_observer_t* observer, lv_subject_t*) {
        auto* self = static_cast<TypingScreen*>(lv_observer_get_user_data(observer));
        self->refresh(platform::monotonic_ms());
    }, this);
}
TypingScreen::~TypingScreen() { if (observer_) lv_observer_remove(observer_); }
lv_obj_t* TypingScreen::rectangle(lv_obj_t* parent, int x, int y, int width, int height, uint32_t color, int radius) {
    auto* object = lv_obj_create(parent); lv_obj_remove_style_all(object);
    lv_obj_remove_flag(object, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    lv_obj_set_pos(object, x, y); lv_obj_set_size(object, width, height);
    lv_obj_set_style_bg_color(object, lv_color_hex(color), 0); lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(object, radius, 0); return object;
}
lv_obj_t* TypingScreen::text(lv_obj_t* parent, int x, int y, const char* value, const lv_font_t* font, uint32_t color) {
    auto* label = lv_label_create(parent); lv_obj_set_pos(label, x, y);
    lv_obj_set_style_text_font(label, font, 0); lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_label_set_text(label, value); return label;
}
void TypingScreen::set_text(lv_obj_t* label, const char* value) {
    if (label && std::strcmp(lv_label_get_text(label), value) != 0) lv_label_set_text(label, value);
}
void TypingScreen::build_content(lv_obj_t* content) { content_ = content; rebuild(); }
void TypingScreen::rebuild() {
    lv_obj_clean(content_);
    timer_ = speed_ = words_ = accuracy_ = typed_ = message_ = progress_ = cursor_ = nullptr;
    tiles_.fill(nullptr); letters_.fill(nullptr);
    const auto& state = typing_.state(); visible_page_ = state.page();
    visible_pack_ = state.records().selected_pack; visible_mode_ = state.records().selected_mode;
    daylight_ = state.records().daylight; storage_warning_ = !state.storage_error().empty();
    background_ = daylight_ ? 0xf3eedf : 0x17232c;
    surface_ = daylight_ ? 0xe3dece : 0x23343d;
    ink_ = daylight_ ? 0x253b39 : 0xf0eedf;
    muted_ = daylight_ ? 0x69726b : 0x9baea9;
    accent_ = daylight_ ? 0x257b68 : 0xb5e5ad;
    danger_ = daylight_ ? 0xbe4f45 : 0xf29a85;
    lv_obj_set_style_bg_color(content_, lv_color_hex(background_), 0); lv_obj_set_style_bg_opa(content_, LV_OPA_COVER, 0);
    if (is_game(visible_page_)) game();
    else if (visible_page_ == model::TypingPage::Home) home();
    else if (visible_page_ == model::TypingPage::Results) results();
    else help();
    if (storage_warning_) {
        rectangle(content_, 0, 134, 320, 15, 0x743e43);
        text(content_, 10, 134, "Save unavailable - check state.txt", &lv_font_montserrat_12, 0xffffff);
    }
}
void TypingScreen::footer(const char* first, const char* second, const char* third) {
    rectangle(content_, 0, 149, 320, 21, surface_);
    text(content_, 10, 153, first, &lv_font_montserrat_12, ink_);
    text(content_, 116, 153, second, &lv_font_montserrat_12, ink_);
    text(content_, 232, 153, third, &lv_font_montserrat_12, ink_);
}
void TypingScreen::home() {
    const auto& state = typing_.state();
    rectangle(content_, 12, 9, 5, 10, accent_); rectangle(content_, 19, 9, 5, 5, accent_);
    text(content_, 32, 6, "KEY DASH", &lv_font_montserrat_14, ink_);
    auto* pack = text(content_, 230, 8, model::TypingModel::pack_name(visible_pack_), &lv_font_montserrat_12, accent_);
    lv_obj_set_width(pack, 78); lv_obj_set_style_text_align(pack, LV_TEXT_ALIGN_RIGHT, 0);
    rectangle(content_, 12, 28, 296, 1, surface_);
    text(content_, 12, 36, "tiny keys.", &lv_font_montserrat_28, ink_);
    text(content_, 13, 68, "quick fingers.", &lv_font_montserrat_20, accent_);
    char buffer[80]; std::snprintf(buffer, sizeof(buffer), "Best %u WPM  /  %s", state.best_wpm(), model::TypingModel::mode_name(visible_mode_));
    text(content_, 14, 94, buffer, &lv_font_montserrat_12, muted_);
    constexpr const char* modes[] = {"1  30s", "2  60s", "3  Practice"};
    for (unsigned index = 0; index < 3; ++index) {
        const bool selected = index == static_cast<unsigned>(visible_mode_);
        auto* chip = rectangle(content_, 12 + index * 100, 116, 96, 25, selected ? accent_ : surface_, 4);
        auto* label = text(chip, 0, 0, modes[index], &lv_font_montserrat_12, selected ? background_ : ink_);
        lv_obj_center(label);
    }
    footer("Tab  Words", "0  Theme", "4 Help / Esc");
}
void TypingScreen::game() {
    text(content_, 12, 7, model::TypingModel::mode_name(visible_mode_), &lv_font_montserrat_12, ink_);
    timer_ = text(content_, 142, 6, "30s", &lv_font_montserrat_14, ink_);
    speed_ = text(content_, 232, 7, "0 WPM", &lv_font_montserrat_12, accent_);
    lv_obj_set_width(speed_, 76); lv_obj_set_style_text_align(speed_, LV_TEXT_ALIGN_RIGHT, 0);
    rectangle(content_, 12, 28, 296, 3, surface_); progress_ = rectangle(content_, 12, 28, 296, 3, accent_);
    message_ = text(content_, 12, 40, "TYPE TO START", &lv_font_montserrat_12, muted_);
    lv_obj_set_width(message_, 296); lv_obj_set_style_text_align(message_, LV_TEXT_ALIGN_CENTER, 0);
    for (unsigned index = 0; index < tiles_.size(); ++index) {
        tiles_[index] = rectangle(content_, 0, 62, 25, 34, background_, 3);
        letters_[index] = text(tiles_[index], 0, 0, "", &lv_font_montserrat_28, muted_);
    }
    cursor_ = rectangle(content_, 0, 96, 21, 2, accent_);
    typed_ = text(content_, 12, 104, "", &lv_font_montserrat_12, muted_);
    lv_obj_set_width(typed_, 296); lv_obj_set_style_text_align(typed_, LV_TEXT_ALIGN_CENTER, 0);
    words_ = text(content_, 12, 127, "0 words", &lv_font_montserrat_12, muted_);
    accuracy_ = text(content_, 175, 127, "100% accuracy", &lv_font_montserrat_12, muted_);
    lv_obj_set_width(accuracy_, 133); lv_obj_set_style_text_align(accuracy_, LV_TEXT_ALIGN_RIGHT, 0);
    footer("Bksp  Fix", "Space  Next", "Esc  Stop");
}
void TypingScreen::results() {
    const auto& state = typing_.state();
    text(content_, 12, 8, state.new_best() ? "NEW PERSONAL BEST" : "ROUND COMPLETE", &lv_font_montserrat_12, accent_);
    rectangle(content_, 12, 29, 296, 1, surface_);
    char buffer[96]; std::snprintf(buffer, sizeof(buffer), "%u", state.wpm(0));
    text(content_, 12, 39, buffer, &lv_font_montserrat_36, ink_);
    text(content_, 14, 81, "WPM", &lv_font_montserrat_12, muted_);
    std::snprintf(buffer, sizeof(buffer), "%u%%", state.accuracy());
    text(content_, 187, 44, buffer, &lv_font_montserrat_28, ink_);
    text(content_, 190, 81, "ACCURACY", &lv_font_montserrat_12, muted_);
    std::snprintf(buffer, sizeof(buffer), "%u %s  /  %llus  /  %s", state.completed_words(), state.completed_words() == 1 ? "word" : "words",
        static_cast<unsigned long long>(state.elapsed_ms(0) / 1000), model::TypingModel::pack_name(visible_pack_));
    text(content_, 14, 106, buffer, &lv_font_montserrat_12, muted_);
    std::snprintf(buffer, sizeof(buffer), "Personal best: %u WPM", state.best_wpm());
    text(content_, 14, 126, buffer, &lv_font_montserrat_12, accent_);
    footer("Enter  Again", "Esc  Menu", "0  Theme");
}
void TypingScreen::help() {
    text(content_, 12, 8, "A TINY TYPING CHALLENGE", &lv_font_montserrat_12, ink_);
    rectangle(content_, 12, 28, 296, 1, surface_);
    text(content_, 12, 37, "1 30s / 2 60s / 3 Untimed practice", &lv_font_montserrat_12, ink_);
    text(content_, 12, 56, "Tab English or Code. 0 Day or night.", &lv_font_montserrat_12, muted_);
    text(content_, 12, 76, "Type a word, then Space or Enter.", &lv_font_montserrat_12, ink_);
    text(content_, 12, 96, "Backspace fixes typos. Esc stops.", &lv_font_montserrat_12, ink_);
    text(content_, 12, 117, "1 word = 5 characters. Autosaves.", &lv_font_montserrat_12, muted_);
    footer("Enter  Close", "Esc  Back", "OFFLINE");
}
void TypingScreen::refresh(uint64_t now) {
    const auto& state = typing_.state();
    const bool transitioned = state.page() != visible_page_ && !(is_game(state.page()) && is_game(visible_page_));
    if (transitioned || state.records().selected_pack != visible_pack_ || state.records().selected_mode != visible_mode_ ||
        state.records().daylight != daylight_ || storage_warning_ != !state.storage_error().empty()) rebuild();
    if (!timer_) return;
    char buffer[96];
    if (state.limit_ms()) std::snprintf(buffer, sizeof(buffer), "%llus", static_cast<unsigned long long>((state.remaining_ms(now) + 999) / 1000));
    else std::snprintf(buffer, sizeof(buffer), "%u/10", state.completed_words());
    set_text(timer_, buffer);
    std::snprintf(buffer, sizeof(buffer), "%u WPM", state.wpm(now)); set_text(speed_, buffer);
    std::snprintf(buffer, sizeof(buffer), "%u %s", state.completed_words(), state.completed_words() == 1 ? "word" : "words"); set_text(words_, buffer);
    std::snprintf(buffer, sizeof(buffer), "%u%% accuracy", state.accuracy()); set_text(accuracy_, buffer);
    const auto& word = state.word(); const auto& input = state.input();
    const int left = (320 - static_cast<int>(word.size()) * 26) / 2;
    for (unsigned index = 0; index < tiles_.size(); ++index) {
        const bool visible = index < word.size(); show(tiles_[index], visible);
        if (!visible) continue;
        lv_obj_set_x(tiles_[index], left + index * 26);
        const bool entered = index < input.size(); const bool correct = entered && input[index] == word[index];
        const char letter[] = {word[index], '\0'}; set_text(letters_[index], letter); lv_obj_center(letters_[index]);
        lv_obj_set_style_text_color(letters_[index], lv_color_hex(entered ? (correct ? accent_ : danger_) : muted_), 0);
        lv_obj_set_style_bg_color(tiles_[index], lv_color_hex(entered && !correct ? (daylight_ ? 0xf0d4c8 : 0x493333) : background_), 0);
    }
    lv_obj_set_x(cursor_, left + static_cast<int>(std::min(input.size(), word.size())) * 26 + 2);
    show(cursor_, now / 500 % 2 == 0 && input.size() < word.size());
    std::snprintf(buffer, sizeof(buffer), "you: %s", input.empty() ? "_" : input.c_str()); set_text(typed_, buffer);
    const bool mistake = !state.last_key_correct() && now >= state.last_key_at() && now - state.last_key_at() < 600;
    const char* message = state.page() == model::TypingPage::Ready ? "TYPE TO START" :
        input == word ? "NICE. SPACE FOR THE NEXT WORD" : mistake ? "BACKSPACE TO FIX" : "FIND YOUR RHYTHM";
    set_text(message_, message);
    lv_obj_set_style_text_color(message_, lv_color_hex(mistake ? danger_ : input == word ? accent_ : muted_), 0);
    const auto width = state.limit_ms() ? static_cast<unsigned>(state.remaining_ms(now) * 296 / state.limit_ms()) : state.completed_words() * 296 / 10;
    lv_obj_set_width(progress_, std::max(1U, width));
}
}
