#include "typing_screen.h"
#include "monotonic_clock.h"
#include "asset_manager.h"
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
    const auto neutral = assets.resolve("images/stackchan-neutral.png");
    const auto happy = assets.resolve("images/stackchan-happy.png");
    if (!neutral.empty()) neutral_image_ = "A:" + neutral.string();
    if (!happy.empty()) happy_image_ = "A:" + happy.string();
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
    timer_ = speed_ = words_ = accuracy_ = mistakes_ = typed_ = message_ = progress_ = cursor_ = nullptr;
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
    visible_panel_ = typing_.online_panel(); visible_message_ = typing_.online_message();
    visible_name_ = typing_.name_input(); visible_duration_ = typing_.online_duration();
    if (visible_panel_ != viewmodel::OnlinePanel::None) online();
    else if (is_game(visible_page_)) game();
    else if (visible_page_ == model::TypingPage::Home) home();
    else if (visible_page_ == model::TypingPage::Results) results();
    else help();
    if (storage_warning_) {
        rectangle(content_, 0, 134, 320, 15, 0x743e43);
        text(content_, 10, 134, "Save unavailable - check state.txt", &lv_font_montserrat_12, 0xffffff);
    }
}
void TypingScreen::stackchan(int x, int y, int width, int height, bool happy) {
    const auto& source = happy ? happy_image_ : neutral_image_;
    if (source.empty()) return;
    auto* image = lv_image_create(content_);
    lv_image_set_src(image, source.c_str());
    lv_obj_set_pos(image, x, y); lv_obj_set_size(image, width, height);
    lv_image_set_inner_align(image, LV_IMAGE_ALIGN_CONTAIN);
    lv_image_set_antialias(image, true);
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
    text(content_, 32, 6, "M5CZT", &lv_font_montserrat_14, ink_);
    auto* pack = text(content_, 230, 8, model::TypingModel::pack_name(visible_pack_), &lv_font_montserrat_12, accent_);
    lv_obj_set_width(pack, 78); lv_obj_set_style_text_align(pack, LV_TEXT_ALIGN_RIGHT, 0);
    rectangle(content_, 12, 28, 296, 1, surface_);
    text(content_, 12, 36, "tiny keys.", &lv_font_montserrat_28, ink_);
    text(content_, 13, 68, "quick fingers.", &lv_font_montserrat_20, accent_);
    stackchan(244, 34, 64, 78);
    char buffer[80]; std::snprintf(buffer, sizeof(buffer), "Best %u WPM  /  %s", state.best_wpm(), model::TypingModel::mode_name(visible_mode_));
    text(content_, 14, 94, buffer, &lv_font_montserrat_12, muted_);
    constexpr const char* modes[] = {"1  30s", "2  60s", "3  Practice"};
    for (unsigned index = 0; index < 3; ++index) {
        const bool selected = index == static_cast<unsigned>(visible_mode_);
        auto* chip = rectangle(content_, 12 + index * 100, 116, 96, 25, selected ? accent_ : surface_, 4);
        auto* label = text(chip, 0, 0, modes[index], &lv_font_montserrat_12, selected ? background_ : ink_);
        lv_obj_center(label);
    }
    footer("Tab Words", "5 Online", "4 Help / Esc");
}
void TypingScreen::game() {
    text(content_, 12, 7, typing_.online_round() ? "ONLINE SPRINT" : model::TypingModel::mode_name(visible_mode_), &lv_font_montserrat_12, ink_);
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
    stackchan(260, 56, 52, 68);
    cursor_ = rectangle(content_, 0, 96, 21, 2, accent_);
    typed_ = text(content_, 12, 104, "", &lv_font_montserrat_12, muted_);
    lv_obj_set_width(typed_, 240); lv_obj_set_style_text_align(typed_, LV_TEXT_ALIGN_CENTER, 0);
    words_ = text(content_, 12, 127, "0 words", &lv_font_montserrat_12, muted_);
    mistakes_ = text(content_, 110, 127, "0 misses", &lv_font_montserrat_12, muted_);
    accuracy_ = text(content_, 202, 127, "100%", &lv_font_montserrat_12, muted_);
    lv_obj_set_width(accuracy_, 106); lv_obj_set_style_text_align(accuracy_, LV_TEXT_ALIGN_RIGHT, 0);
    footer("Bksp Undo", "Auto Next", "Esc Stop");
}
void TypingScreen::results() {
    const auto& state = typing_.state();
    text(content_, 12, 8, state.new_best() ? "NEW PERSONAL BEST" : "ROUND COMPLETE", &lv_font_montserrat_12, accent_);
    rectangle(content_, 12, 29, 296, 1, surface_);
    auto* pack = text(content_, 230, 8, model::TypingModel::pack_name(visible_pack_), &lv_font_montserrat_12, muted_);
    lv_obj_set_width(pack, 78); lv_obj_set_style_text_align(pack, LV_TEXT_ALIGN_RIGHT, 0);
    stackchan(8, 30, 112, 100, true);
    char buffer[96]; std::snprintf(buffer, sizeof(buffer), "%u", state.wpm(0));
    text(content_, 132, 36, buffer, &lv_font_montserrat_36, ink_);
    text(content_, 225, 57, "WPM", &lv_font_montserrat_12, muted_);
    std::snprintf(buffer, sizeof(buffer), "%u%%", state.accuracy());
    text(content_, 136, 80, buffer, &lv_font_montserrat_20, ink_);
    text(content_, 199, 85, "accuracy", &lv_font_montserrat_12, muted_);
    std::snprintf(buffer, sizeof(buffer), "%u words / %llus", state.completed_words(),
        static_cast<unsigned long long>(state.elapsed_ms(0) / 1000));
    text(content_, 136, 103, buffer, &lv_font_montserrat_12, muted_);
    std::snprintf(buffer, sizeof(buffer), "%u miss%s", state.mistakes(), state.mistakes() == 1 ? "" : "es");
    text(content_, 136, 116, buffer, &lv_font_montserrat_12, state.mistakes() ? danger_ : muted_);
    std::snprintf(buffer, sizeof(buffer), "Personal best: %u WPM", state.best_wpm());
    auto* status = text(content_, 12, 130, typing_.online_round() ? typing_.online_message().c_str() : buffer, &lv_font_montserrat_12, accent_);
    lv_obj_set_size(status, 296, 16); lv_label_set_long_mode(status, LV_LABEL_LONG_SCROLL_CIRCULAR);
    footer("Enter Again", "Esc Menu", typing_.online_round() ? "4 Rankings" : "0 Theme");
}
void TypingScreen::help() {
    text(content_, 12, 8, "A TINY TYPING CHALLENGE", &lv_font_montserrat_12, ink_);
    rectangle(content_, 12, 28, 296, 1, surface_);
    text(content_, 12, 37, "1 30s / 2 60s / 3 Untimed practice", &lv_font_montserrat_12, ink_);
    text(content_, 12, 56, "Tab English or Code. 0 Day or night.", &lv_font_montserrat_12, muted_);
    text(content_, 12, 76, "Last letter goes to the next word.", &lv_font_montserrat_12, ink_);
    text(content_, 12, 96, "Wrong key? Retry. Bksp undoes.", &lv_font_montserrat_12, ink_);
    text(content_, 12, 117, "Misses counted. Esc stops. Autosave.", &lv_font_montserrat_12, muted_);
    footer("Enter Close", "Esc Back", "5 Online");
}
void TypingScreen::online() {
    using viewmodel::OnlinePanel;
    const auto panel = typing_.online_panel();
    char buffer[100];
    text(content_, 12, 7, "M5CZT / ONLINE", &lv_font_montserrat_12, accent_);
    rectangle(content_, 12, 27, 296, 1, surface_);
    if (panel == OnlinePanel::Contact) {
        if (!typing_.contact_url().empty()) {
            auto* qr = lv_qrcode_create(content_);
            lv_obj_set_pos(qr, 10, 31); lv_qrcode_set_size(qr, 112);
            lv_qrcode_set_quiet_zone(qr, true);
            lv_qrcode_set_dark_color(qr, lv_color_black()); lv_qrcode_set_light_color(qr, lv_color_white());
            const auto& url = typing_.contact_url();
            if (lv_qrcode_update(qr, url.data(), static_cast<uint32_t>(url.size())) != LV_RESULT_OK)
                std::fprintf(stderr, "M5CZT: contact QR generation failed\n");
        }
        text(content_, 132, 35, "Optional email", &lv_font_montserrat_16, ink_);
        text(content_, 132, 60, typing_.contact_url().empty() ? "Enter for a new QR" : "Scan on your phone", &lv_font_montserrat_12, muted_);
        text(content_, 132, 80, typing_.contact_code().c_str(), &lv_font_montserrat_12, accent_);
        text(content_, 132, 103, typing_.contact_url().empty() ? "Code expired" : "Valid for 10 minutes", &lv_font_montserrat_12, muted_);
        text(content_, 132, 123, "Save / change / delete", &lv_font_montserrat_12, muted_);
        footer("Enter New QR", "Private", "Esc Back");
        return;
    } else if (panel == OnlinePanel::Name) {
        text(content_, 12, 36, "Choose your player name", &lv_font_montserrat_16, ink_);
        rectangle(content_, 12, 63, 296, 31, surface_, 4);
        std::snprintf(buffer, sizeof(buffer), "%s_", typing_.name_input().c_str());
        text(content_, 20, 68, buffer, &lv_font_montserrat_20, ink_);
        text(content_, 12, 101, "3-12 letters, digits, _ or -", &lv_font_montserrat_12, muted_);
        footer("Enter Register", "Bksp Fix", "Esc Back");
    } else if (panel == OnlinePanel::Board) {
        std::snprintf(buffer, sizeof(buffer), "%us / %s / TOP 5", typing_.online_duration(), model::TypingModel::pack_name(visible_pack_));
        text(content_, 12, 34, buffer, &lv_font_montserrat_12, ink_);
        int y = 53;
        for (const auto& row : typing_.ranking()) {
            std::snprintf(buffer, sizeof(buffer), "%u  %-12s", row.rank, row.name.c_str());
            text(content_, 12, y, buffer, &lv_font_montserrat_12, ink_);
            std::snprintf(buffer, sizeof(buffer), "%u WPM %u%%", row.wpm, row.accuracy);
            auto* label = text(content_, 205, y, buffer, &lv_font_montserrat_12, accent_);
            lv_obj_set_width(label, 103); lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0); y += 18;
        }
        if (typing_.ranking().empty()) text(content_, 12, 78, "No scores yet. Be the first!", &lv_font_montserrat_14, muted_);
        footer("1 30s / 2 60s", "Tab Words", "Esc Menu");
        return;
    } else if (panel == OnlinePanel::Busy) {
        text(content_, 12, 53, "Connecting...", &lv_font_montserrat_20, ink_);
        text(content_, 12, 87, "Please wait a moment.", &lv_font_montserrat_12, muted_);
        footer("Wi-Fi required", "", "ONLINE");
    } else {
        std::snprintf(buffer, sizeof(buffer), "Player: %s", typing_.player_name().empty() ? "not registered" : typing_.player_name().c_str());
        text(content_, 12, 37, buffer, &lv_font_montserrat_14, ink_);
        std::snprintf(buffer, sizeof(buffer), "%us / %s", typing_.online_duration(), model::TypingModel::pack_name(visible_pack_));
        text(content_, 12, 62, buffer, &lv_font_montserrat_20, accent_);
        text(content_, 12, 91, "1 30s / 2 60s   Tab: words", &lv_font_montserrat_12, muted_);
        text(content_, 12, 108, "6 Optional contact email", &lv_font_montserrat_12, muted_);
        footer("Enter Start", "4 Rankings", "Esc Back");
    }
    auto* status = text(content_, 12, 124, typing_.online_message().c_str(), &lv_font_montserrat_12, muted_);
    lv_obj_set_size(status, 296, 21); lv_label_set_long_mode(status, LV_LABEL_LONG_SCROLL_CIRCULAR);
}
void TypingScreen::refresh(uint64_t now) {
    const auto& state = typing_.state();
    const bool transitioned = state.page() != visible_page_ && !(is_game(state.page()) && is_game(visible_page_));
    if (typing_.online_panel() != visible_panel_ || typing_.online_message() != visible_message_ ||
        typing_.name_input() != visible_name_ || typing_.online_duration() != visible_duration_ || transitioned || state.records().selected_pack != visible_pack_ || state.records().selected_mode != visible_mode_ ||
        state.records().daylight != daylight_ || storage_warning_ != !state.storage_error().empty()) rebuild();
    if (!timer_) return;
    char buffer[96];
    if (state.limit_ms()) std::snprintf(buffer, sizeof(buffer), "%llus", static_cast<unsigned long long>((state.remaining_ms(now) + 999) / 1000));
    else std::snprintf(buffer, sizeof(buffer), "%u/10", state.completed_words());
    set_text(timer_, buffer);
    std::snprintf(buffer, sizeof(buffer), "%u WPM", state.wpm(now)); set_text(speed_, buffer);
    std::snprintf(buffer, sizeof(buffer), "%u %s", state.completed_words(), state.completed_words() == 1 ? "word" : "words"); set_text(words_, buffer);
    std::snprintf(buffer, sizeof(buffer), "%u%%", state.accuracy()); set_text(accuracy_, buffer);
    std::snprintf(buffer, sizeof(buffer), "%u miss%s", state.mistakes(), state.mistakes() == 1 ? "" : "es"); set_text(mistakes_, buffer);
    const auto& word = state.word(); const auto& input = state.input();
    const int left = (252 - static_cast<int>(word.size()) * 26) / 2;
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
        mistake ? "MISS! RETRY THE SAME LETTER" : "FIND YOUR RHYTHM";
    set_text(message_, message);
    lv_obj_set_style_text_color(message_, lv_color_hex(mistake ? danger_ : input == word ? accent_ : muted_), 0);
    const auto width = state.limit_ms() ? static_cast<unsigned>(state.remaining_ms(now) * 296 / state.limit_ms()) : state.completed_words() * 296 / 10;
    lv_obj_set_width(progress_, std::max(1U, width));
}
}
