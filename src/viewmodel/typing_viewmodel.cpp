#include "typing_viewmodel.h"
#include <cstdio>
namespace viewmodel {
TypingViewModel::TypingViewModel() : state_(model::typing_state_path()) { load_player(); }
void TypingViewModel::publish() { revision_.set((revision_.value() + 1) % 1000000); }
void TypingViewModel::key(uint32_t key, uint64_t now) {
    using model::TypingMode;
    using model::TypingPage;
    online_poll(now);
    if (request_ == Request::Submit && client_.busy()) return;
    if (panel_ != OnlinePanel::None) { online_key(key, now); publish(); return; }
    if (state_.tick(now)) { finish_online(); save(); publish(); return; }
    const auto page = state_.page();
    if (page == TypingPage::Help) {
        if (key == LV_KEY_ESC || key == LV_KEY_ENTER || key == '4') state_.help();
    } else if (key == LV_KEY_ESC) {
        if (page == TypingPage::Home) quit_ = true;
        else { state_.back(); online_round_ = false; pending_score_.clear(); online_message_.clear(); }
    } else if (page == TypingPage::Home) {
        online_round_ = false; pending_score_.clear();
        if (key >= '1' && key <= '3') state_.prepare(static_cast<TypingMode>(key - '1'), now);
        else if (key == LV_KEY_ENTER) state_.prepare(state_.records().selected_mode, now);
        else if (key == LV_KEY_NEXT || key == LV_KEY_PREV) state_.toggle_pack();
        else if (key == '0') state_.toggle_theme();
        else if (key == '4') state_.help();
        else if (key == '5') {
            panel_ = OnlinePanel::Menu;
            online_message_ = "English / 30s only. Wi-Fi required.";
        }
    } else if (page == TypingPage::Results) {
        if (key == LV_KEY_ENTER || key == '1') {
            if (online_round_) { state_.back(); online_round_ = false; pending_score_.clear(); panel_ = OnlinePanel::Menu; online_message_ = "Enter to start a new online round."; }
            else state_.prepare(state_.records().selected_mode, now);
        } else if (key == '5' && online_round_) submit_score();
        else if (key == '4' && online_round_) { state_.back(); online_round_ = false; fetch_board(); }
        else if (key == '0') state_.toggle_theme();
    } else if (page == TypingPage::Ready || page == TypingPage::Running) {
        char event = '\0';
        if (key == LV_KEY_BACKSPACE || key == LV_KEY_DEL) {
            if (page == TypingPage::Running) event = '\b';
            state_.backspace(now);
        } else if (key >= 32 && key <= 126) {
            event = static_cast<char>(key);
            if (page == TypingPage::Ready && event == ' ') event = '\0';
            if (event) state_.type(event, now);
        }
        if (online_round_ && event) {
            if (events_.empty()) online_started_ = now;
            if (events_.size() >= 2400) {
                online_round_ = false; online_message_ = "Too many keys for online ranking.";
            } else events_.push_back({now - online_started_, event});
        }
    }
    publish();
}
void TypingViewModel::tick(uint64_t now) {
    online_poll(now);
    if (panel_ == OnlinePanel::Contact && contact_until_ && now >= contact_until_) {
        contact_until_ = 0; contact_code_.clear(); contact_url_.clear();
        online_message_ = "Code expired. Enter for a new QR."; publish();
    }
    if (online_round_ && state_.page() == model::TypingPage::Ready && now - online_ready_ > 8 * 60000) {
        state_.back(); online_round_ = false; panel_ = OnlinePanel::Menu;
        online_message_ = "Challenge expired. Enter to restart."; publish();
    }
    if (state_.tick(now)) { finish_online(); save(); publish(); }
    if (state_.dirty() && now - last_save_ >= 1000) { last_save_ = now; save(); publish(); }
}
void TypingViewModel::save() {
    if (state_.dirty() && !state_.save()) std::fprintf(stderr, "taM5CZTyping save: %s\n", state_.storage_error().c_str());
}
}
