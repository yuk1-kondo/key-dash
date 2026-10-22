#include "typing_viewmodel.h"
#include <cstdio>
namespace viewmodel {
TypingViewModel::TypingViewModel() : state_(model::typing_state_path()) {}
void TypingViewModel::publish() { revision_.set((revision_.value() + 1) % 1000000); }
void TypingViewModel::key(uint32_t key, uint64_t now) {
    using model::TypingMode;
    using model::TypingPage;
    if (state_.tick(now)) { save(); publish(); return; }
    const auto page = state_.page();
    if (page == TypingPage::Help) {
        if (key == LV_KEY_ESC || key == LV_KEY_ENTER || key == '4') state_.help();
    } else if (key == LV_KEY_ESC) {
        if (page == TypingPage::Home) quit_ = true;
        else state_.back();
    } else if (page == TypingPage::Home) {
        if (key >= '1' && key <= '3') state_.prepare(static_cast<TypingMode>(key - '1'), now);
        else if (key == LV_KEY_ENTER) state_.prepare(state_.records().selected_mode, now);
        else if (key == LV_KEY_NEXT || key == LV_KEY_PREV) state_.toggle_pack();
        else if (key == '0') state_.toggle_theme();
        else if (key == '4') state_.help();
    } else if (page == TypingPage::Results) {
        if (key == LV_KEY_ENTER || key == '1') state_.prepare(state_.records().selected_mode, now);
        else if (key == '0') state_.toggle_theme();
    } else if (page == TypingPage::Ready || page == TypingPage::Running) {
        if (key == LV_KEY_BACKSPACE || key == LV_KEY_DEL) state_.backspace(now);
        else if (key == LV_KEY_ENTER) state_.type(' ', now);
        else if (key >= 32 && key <= 126) state_.type(static_cast<char>(key), now);
    }
    publish();
}
void TypingViewModel::tick(uint64_t now) {
    if (state_.tick(now)) { save(); publish(); }
    if (state_.dirty() && now - last_save_ >= 1000) { last_save_ = now; save(); publish(); }
}
void TypingViewModel::save() {
    if (state_.dirty() && !state_.save()) std::fprintf(stderr, "KEY DASH save: %s\n", state_.storage_error().c_str());
}
}
