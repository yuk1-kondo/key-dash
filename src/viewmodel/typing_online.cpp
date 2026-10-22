#include "typing_viewmodel.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <random>
#include <system_error>
namespace viewmodel {
using Json = nlohmann::json;
namespace {
std::filesystem::path player_path() { return model::typing_state_path().parent_path() / "online-player.json"; }
bool valid_name(const std::string& name) {
    return name.size() >= 3 && name.size() <= 12 && std::all_of(name.begin(), name.end(), [](unsigned char ch) {
        return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-';
    });
}
std::string random_player_key() {
    std::random_device random;
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    for (unsigned i = 0; i < 32; ++i) {
        const auto byte = random() & 255U;
        result.push_back(digits[byte >> 4]); result.push_back(digits[byte & 15U]);
    }
    return result;
}
bool valid_token(const std::string& token) {
    return token.size() == 64 && std::all_of(token.begin(), token.end(), [](char ch) { return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'); });
}
const char* pack_name(model::WordPack pack) { return pack == model::WordPack::Code ? "code" : "english"; }
}
void TypingViewModel::load_player() {
    std::ifstream file(player_path());
    if (!file) return;
    try {
        const auto identity = Json::parse(file);
        if (identity.at("endpoint").get<std::string>() != client_.endpoint()) return;
        const auto name = identity.at("name").get<std::string>(), token = identity.at("token").get<std::string>();
        if (!valid_name(name) || !valid_token(token)) throw std::runtime_error("Invalid player");
        player_name_ = name; player_token_ = token; player_id_ = identity.at("id").get<std::string>();
    } catch (const std::exception&) { online_message_ = "Player file unreadable. Keep a backup."; }
}
bool TypingViewModel::save_player() {
    const auto path = player_path(); auto temporary = path; temporary += ".tmp";
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return false;
    std::ofstream file(temporary, std::ios::trunc);
    if (!file) return false;
    std::filesystem::permissions(temporary, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write,
        std::filesystem::perm_options::replace, error);
    if (error) return false;
    file << Json{{"id", player_id_}, {"name", player_name_}, {"token", player_token_}, {"endpoint", client_.endpoint()}}.dump(2) << '\n';
    file.flush(); if (!file) return false;
    file.close(); if (!file) return false;
    std::filesystem::rename(temporary, path, error);
    return !error;
}
bool TypingViewModel::request(Request action, const std::string& method, const std::string& path, const std::string& body) {
    if (!client_.configured()) { online_message_ = "Ranking service not configured yet."; return false; }
    if (!client_.start(method, path, body, player_token_)) return false;
    request_ = action;
    if (action != Request::Submit) panel_ = OnlinePanel::Busy;
    online_message_ = action == Request::Submit ? "Uploading score..." : "Connecting...";
    return true;
}
void TypingViewModel::fetch_board() {
    const std::string query = "/v1/leaderboard?mode=" + std::to_string(online_duration_) + "&pack=" + pack_name(state_.records().selected_pack);
    request(Request::Board, "GET", query);
}
void TypingViewModel::finish_online() {
    if (!online_round_) return;
    Json events = Json::array();
    for (const auto& event : events_) events.push_back(Json::array({event.time, std::string(1, event.key)}));
    pending_score_ = Json{{"events", events}}.dump();
    if (!state_.completed_words() || !state_.wpm(0)) { online_message_ = "Finish a word to enter rankings."; pending_score_.clear(); return; }
    submit_score();
}
void TypingViewModel::submit_score() {
    if (!online_round_ || submitted_ || pending_score_.empty() || client_.busy()) return;
    if (request(Request::Submit, "POST", "/v1/runs/" + run_id_ + "/score", pending_score_)) submitted_ = true;
}
void TypingViewModel::online_key(uint32_t key, uint64_t) {
    if (panel_ == OnlinePanel::Busy) return;
    if (key == LV_KEY_ESC) { panel_ = panel_ == OnlinePanel::Contact ? OnlinePanel::Menu : OnlinePanel::None; return; }
    if (panel_ == OnlinePanel::Contact) {
        if (key == LV_KEY_ENTER || key == '6') request(Request::ContactLink, "POST", "/v1/contact-links", "{}");
        return;
    }
    if (panel_ == OnlinePanel::Name) {
        if (key == LV_KEY_BACKSPACE || key == LV_KEY_DEL) { if (!name_input_.empty()) name_input_.pop_back(); }
        else if (key == LV_KEY_ENTER) {
            if (!valid_name(name_input_)) online_message_ = "Use 3-12 letters or digits.";
            else {
                if (registration_name_ != name_input_ || registration_token_.empty()) {
                    try { registration_token_ = random_player_key(); }
                    catch (const std::exception&) { online_message_ = "Could not create a player key."; return; }
                    registration_name_ = name_input_;
                }
                request(Request::Register, "POST", "/v1/players", Json{{"name", name_input_}, {"token", registration_token_}}.dump());
            }
        } else if (name_input_.size() < 12 && ((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9') || key == '_' || key == '-')) name_input_.push_back(static_cast<char>(key));
        return;
    }
    if (key == '1' || key == '2') { online_duration_ = key == '1' ? 30 : 60; if (panel_ == OnlinePanel::Board) fetch_board(); }
    else if (key == LV_KEY_NEXT || key == LV_KEY_PREV) { state_.toggle_pack(); if (panel_ == OnlinePanel::Board) fetch_board(); }
    else if (key == '4') fetch_board();
    else if (key == '6') {
        if (player_token_.empty()) online_message_ = "Register a player name with Enter first.";
        else request(Request::ContactLink, "POST", "/v1/contact-links", "{}");
    }
    else if (key == LV_KEY_ENTER) {
        if (panel_ == OnlinePanel::Board) { panel_ = OnlinePanel::Menu; return; }
        if (player_token_.empty()) { panel_ = OnlinePanel::Name; online_message_ = "Public name. No GitHub login needed."; return; }
        request(Request::Challenge, "POST", "/v1/runs", Json{{"mode", online_duration_}, {"pack", pack_name(state_.records().selected_pack)}, {"rules", 1}}.dump());
    }
}
void TypingViewModel::online_poll(uint64_t now) {
    const auto response = client_.poll();
    if (!response) return;
    const auto action = request_; request_ = Request::None;
    try {
        if (!response->error.empty()) throw std::runtime_error(response->error);
        auto data = Json::parse(response->body);
        if (response->status < 200 || response->status >= 300) throw std::runtime_error(data.value("error", "Service unavailable. Try again."));
        if (action == Request::Register) {
            const auto name = data.at("name").get<std::string>(), token = data.at("token").get<std::string>();
            if (!valid_name(name) || !valid_token(token)) throw std::runtime_error("Invalid registration response");
            player_name_ = name; player_token_ = token; player_id_ = data.at("id").get<std::string>();
            panel_ = OnlinePanel::Menu;
            online_message_ = save_player() ? "Registered. Enter: play / 6: contact" : "Player key not saved. Check storage.";
        } else if (action == Request::ContactLink) {
            const auto code = data.at("code").get<std::string>();
            const std::string alphabet = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
            if (code.size() != 12 || code.find_first_not_of(alphabet) != std::string::npos || data.at("expires_in") != 600)
                throw std::runtime_error("Invalid contact response");
            contact_code_ = code.substr(0, 4) + "-" + code.substr(4, 4) + "-" + code.substr(8, 4);
            contact_url_ = "https://yuk1-kondo.github.io/key-dash-ranking/#contact=" + code;
            contact_until_ = now + 590000;
            panel_ = OnlinePanel::Contact; online_message_ = "Private contact for event results.";
        } else if (action == Request::Challenge) {
            const auto seed = data.at("seed").get<uint32_t>();
            const auto id = data.at("id").get<std::string>();
            if (data.at("rules") != 1 || data.at("mode") != online_duration_ || data.at("pack") != pack_name(state_.records().selected_pack) || !seed || id.size() != 36 || id.find_first_not_of("0123456789abcdef-") != std::string::npos) throw std::runtime_error("Invalid challenge response");
            run_id_ = id; events_.clear(); pending_score_.clear(); submitted_ = false;
            online_round_ = true; online_started_ = 0; online_ready_ = now;
            state_.prepare_online(online_duration_ == 60 ? model::TypingMode::Sprint60 : model::TypingMode::Sprint30, state_.records().selected_pack, seed, now);
            panel_ = OnlinePanel::None; online_message_ = "ONLINE CHALLENGE";
        } else if (action == Request::Board) {
            ranking_.clear();
            for (const auto& row : data.at("scores")) {
                if (ranking_.size() == 5) break;
                const auto name = row.at("name").get<std::string>();
                if (!valid_name(name)) throw std::runtime_error("Invalid ranking response");
                ranking_.push_back({row.at("rank").get<unsigned>(), row.at("wpm").get<unsigned>(), row.at("accuracy").get<unsigned>(), name});
            }
            panel_ = OnlinePanel::Board;
            online_message_ = ranking_.empty() ? "No scores yet. Be the first!" : "Top 5 / personal bests";
        } else if (action == Request::Submit) {
            if (data.at("wpm").get<unsigned>() != state_.wpm(0) || data.at("accuracy").get<unsigned>() != state_.accuracy()) throw std::runtime_error("Score verification mismatch");
            online_message_ = "Score registered! 4: Rankings"; pending_score_.clear();
        }
    } catch (const std::exception& error) {
        online_message_ = error.what();
        if (online_message_.size() > 100) online_message_ = "Service response invalid. Try again.";
        if (action == Request::Submit) { submitted_ = false; online_message_ += " 5: Retry"; }
        else panel_ = action == Request::Register ? OnlinePanel::Name : OnlinePanel::Menu;
    }
    publish();
}
}
