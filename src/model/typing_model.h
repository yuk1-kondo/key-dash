#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace model {
enum class TypingPage { Home, Ready, Running, Results, Help };
enum class TypingMode : unsigned { Sprint30, Sprint60, Practice };
enum class WordPack : unsigned { English, Code };
struct TypingRecords {
    unsigned rounds{0};
    unsigned words{0};
    std::array<unsigned, 6> best_wpm{};
    std::array<unsigned, 6> legacy_best_wpm{};
    TypingMode selected_mode{TypingMode::Sprint30};
    WordPack selected_pack{WordPack::English};
    bool daylight{true};
};
class TypingModel {
public:
    explicit TypingModel(std::filesystem::path save_file);
    void prepare(TypingMode mode, uint64_t now);
    void prepare_online(TypingMode mode, WordPack pack, uint32_t seed, uint64_t now);
    void type(char character, uint64_t now);
    void backspace(uint64_t now);
    void back();
    void help();
    void toggle_pack();
    void toggle_theme();
    bool tick(uint64_t now);
    bool save();
    TypingPage page() const { return page_; }
    const TypingRecords& records() const { return records_; }
    const std::string& word() const { return word_; }
    const std::string& input() const { return input_; }
    unsigned attempts() const { return attempts_; }
    unsigned completed_words() const { return completed_words_; }
    unsigned accuracy() const;
    unsigned mistakes() const { return attempts_ - correct_attempts_; }
    unsigned wpm(uint64_t now) const;
    unsigned best_wpm() const;
    unsigned correct_prefix() const;
    uint64_t limit_ms() const;
    uint64_t elapsed_ms(uint64_t now) const;
    uint64_t remaining_ms(uint64_t now) const;
    bool new_best() const { return new_best_; }
    bool last_key_correct() const { return last_key_correct_; }
    uint64_t last_key_at() const { return last_key_at_; }
    bool dirty() const { return dirty_; }
    const std::string& storage_error() const { return storage_error_; }
    static const char* mode_name(TypingMode mode);
    static const char* pack_name(WordPack pack);
private:
    void load();
    void next_word();
    void finish(uint64_t elapsed);
    std::size_t record_index() const;
    std::filesystem::path save_file_;
    TypingRecords records_;
    TypingPage page_{TypingPage::Home};
    TypingPage help_return_{TypingPage::Home};
    std::string word_, input_, storage_error_;
    uint32_t random_state_{0x4b455944};
    uint64_t started_at_{0}, finished_elapsed_{0}, last_key_at_{0};
    unsigned attempts_{0}, correct_attempts_{0}, completed_words_{0}, committed_characters_{0};
    bool last_key_correct_{true}, new_best_{false}, dirty_{false}, storage_blocked_{false};
};
std::filesystem::path typing_state_path();
} // namespace model
