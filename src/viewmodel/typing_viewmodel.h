#pragma once
#include "typing_model.h"
#include "subjects.h"
#include "online_client.h"
#include <vector>
namespace viewmodel {
enum class OnlinePanel { None, Menu, Name, Board, Contact, Busy };
struct RankingRow { unsigned rank, wpm, accuracy; std::string name; };
class TypingViewModel {
public:
    TypingViewModel();
    void key(uint32_t key, uint64_t now);
    void tick(uint64_t now);
    void save();
    const model::TypingModel& state() const { return state_; }
    lv_subject_t* revision_subject() { return revision_.native(); }
    bool quit_requested() const { return quit_; }
    OnlinePanel online_panel() const { return panel_; }
    const std::string& online_message() const { return online_message_; }
    const std::string& player_name() const { return player_name_; }
    const std::string& name_input() const { return name_input_; }
    const std::vector<RankingRow>& ranking() const { return ranking_; }
    unsigned online_duration() const { return 30; }
    bool online_round() const { return online_round_; }
    const std::string& contact_code() const { return contact_code_; }
    const std::string& contact_url() const { return contact_url_; }
private:
    enum class Request { None, Register, Challenge, Board, ContactLink, Submit };
    struct KeyEvent { uint64_t time; char key; };
    void publish();
    void online_key(uint32_t key, uint64_t now);
    void online_poll(uint64_t now);
    void finish_online();
    void submit_score();
    void fetch_board();
    void load_player();
    bool save_player();
    bool request(Request action, const std::string& method, const std::string& path, const std::string& body = {});
    model::TypingModel state_;
    network::OnlineClient client_;
    reactive::IntSubject revision_{0};
    uint64_t last_save_{0}, online_started_{0}, online_ready_{0}, contact_until_{0};
    bool quit_{false}, online_round_{false}, submitted_{false};
    OnlinePanel panel_{OnlinePanel::None};
    Request request_{Request::None};
    std::string player_name_, player_token_, player_id_, name_input_, run_id_, pending_score_;
    std::string online_message_, registration_token_, registration_name_, contact_code_, contact_url_;
    std::vector<RankingRow> ranking_;
    std::vector<KeyEvent> events_;
};
}
