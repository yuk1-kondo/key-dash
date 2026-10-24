#include "typing_model.h"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <system_error>

namespace model {
namespace {
constexpr unsigned maximum_counter = 100000000;
constexpr unsigned practice_words = 10;
constexpr std::string_view english_words[] = {
    "tiny", "pixel", "cloud", "river", "light", "dream", "green", "music",
    "paper", "water", "moon", "spark", "happy", "orbit", "quiet", "plant",
    "sunny", "apple", "world", "night", "field", "bloom", "magic", "ocean",
    "stone", "copper", "garden", "window", "planet", "pocket", "forest", "silver",
    "little", "coffee", "yellow", "orange", "button", "ripple", "comet", "hello",
    "summer", "winter", "spring", "autumn", "breeze", "thread", "smile", "toast",
    "candle", "velvet", "feather", "meadow", "shell", "raven", "maple", "coral",
    "tiger", "pearl", "lemon", "mint", "ember", "violet", "rain", "moss"
};
constexpr std::string_view code_words[] = {
    "array", "return", "while", "const", "float", "class", "string", "vector",
    "input", "output", "buffer", "stack", "queue", "shell", "linux", "kernel",
    "debug", "build", "commit", "branch", "merge", "push", "fetch", "clone",
    "binary", "source", "header", "import", "export", "module", "public", "static",
    "struct", "switch", "case", "break", "char", "value", "object", "method",
    "thread", "signal", "socket", "server", "client", "packet", "screen", "render",
    "event", "timer", "memory", "cache", "query", "index", "table", "format",
    "parse", "token", "scope", "async", "await", "yield", "false", "true"
};
static_assert(sizeof(english_words) / sizeof(english_words[0]) == 64);
static_assert(sizeof(code_words) / sizeof(code_words[0]) == 64);
void add_bounded(unsigned& value, unsigned amount = 1) {
    value = std::min(value, maximum_counter - std::min(amount, maximum_counter)) + std::min(amount, maximum_counter);
}
}
std::filesystem::path typing_state_path() {
    if (const char* custom = std::getenv("KEY_DASH_STATE_DIR"); custom && *custom) {
        const std::filesystem::path path(custom);
        if (path.is_absolute()) return path / "state.txt";
    }
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) {
        const std::filesystem::path path(xdg);
        if (path.is_absolute()) return path / "key-dash/state.txt";
    }
    if (const char* home = std::getenv("HOME"); home && *home) {
        const std::filesystem::path path(home);
        if (path.is_absolute()) return path / ".config/key-dash/state.txt";
    }
    return {};
}
TypingModel::TypingModel(std::filesystem::path file) : save_file_(std::move(file)) { load(); }
void TypingModel::load() {
    if (save_file_.empty()) { storage_error_ = "Home directory unavailable"; return; }
    std::error_code error;
    const bool exists = std::filesystem::exists(save_file_, error);
    if (error) { storage_error_ = error.message(); storage_blocked_ = true; return; }
    if (!exists) return;
    std::ifstream file(save_file_);
    std::string magic, trailing;
    unsigned version = 0, mode = 0, pack = 0, daylight = 0;
    TypingRecords loaded;
    bool valid = static_cast<bool>(file >> magic >> version >> loaded.rounds >> loaded.words >> mode >> pack >> daylight);
    for (auto& record : loaded.best_wpm) valid = static_cast<bool>(file >> record) && record <= 999 && valid;
    if (version == 2) {
        for (auto& record : loaded.legacy_best_wpm) valid = static_cast<bool>(file >> record) && record <= 999 && valid;
    }
    valid = valid && magic == "KEY_DASH" && (version == 1 || version == 2) && loaded.rounds <= maximum_counter &&
        loaded.words <= maximum_counter && mode <= 2 && pack <= 1 && daylight <= 1 && !(file >> trailing);
    if (!valid) { storage_error_ = "Save unreadable; original kept"; storage_blocked_ = true; return; }
    if (version == 1) { loaded.legacy_best_wpm = loaded.best_wpm; loaded.best_wpm.fill(0); dirty_ = true; }
    loaded.selected_mode = static_cast<TypingMode>(mode);
    loaded.selected_pack = static_cast<WordPack>(pack);
    loaded.daylight = daylight != 0; records_ = loaded;
    random_state_ ^= loaded.words + loaded.rounds * 97U;
    if (!random_state_) random_state_ = 1;
}
bool TypingModel::save() {
    if (storage_blocked_ || save_file_.empty()) return false;
    std::error_code error;
    std::filesystem::create_directories(save_file_.parent_path(), error);
    if (error) { storage_error_ = error.message(); return false; }
    auto temporary = save_file_; temporary += ".tmp";
    {
        std::ofstream file(temporary, std::ios::trunc);
        file << "KEY_DASH 2\n" << records_.rounds << ' ' << records_.words << ' '
             << static_cast<unsigned>(records_.selected_mode) << ' ' << static_cast<unsigned>(records_.selected_pack)
             << ' ' << records_.daylight << '\n';
        for (auto record : records_.best_wpm) file << record << ' ';
        file << '\n';
        for (auto record : records_.legacy_best_wpm) file << record << ' ';
        file << '\n'; file.flush();
        if (!file) { storage_error_ = "Could not write save"; return false; }
        file.close();
        if (!file) { storage_error_ = "Could not close save"; return false; }
    }
    std::filesystem::rename(temporary, save_file_, error);
    if (error) { storage_error_ = error.message(); return false; }
    storage_error_.clear(); dirty_ = false; return true;
}
const char* TypingModel::mode_name(TypingMode mode) {
    switch (mode) {
        case TypingMode::Sprint30: return "30s SPRINT";
        case TypingMode::Sprint60: return "60s SPRINT";
        case TypingMode::Practice: return "10 WORDS";
    }
    return "30s SPRINT";
}
const char* TypingModel::pack_name(WordPack pack) { return pack == WordPack::Code ? "CODE" : "ENGLISH"; }
std::size_t TypingModel::record_index() const {
    return static_cast<unsigned>(records_.selected_mode) * 2U + static_cast<unsigned>(records_.selected_pack);
}
unsigned TypingModel::best_wpm() const { return records_.best_wpm[record_index()]; }
void TypingModel::next_word() {
    const auto* words = records_.selected_pack == WordPack::Code ? code_words : english_words;
    constexpr unsigned word_count = 64;
    const auto previous = word_;
    do {
        random_state_ ^= random_state_ << 13U;
        random_state_ ^= random_state_ >> 17U;
        random_state_ ^= random_state_ << 5U;
        word_ = words[random_state_ % word_count];
    } while (word_ == previous);
    input_.clear();
}
void TypingModel::prepare(TypingMode mode, uint64_t now) {
    records_.selected_mode = mode; dirty_ = true; page_ = TypingPage::Ready;
    attempts_ = correct_attempts_ = completed_words_ = committed_characters_ = 0;
    started_at_ = finished_elapsed_ = last_key_at_ = 0;
    last_key_correct_ = true; new_best_ = false;
    random_state_ ^= static_cast<uint32_t>(now) | 1U;
    if (!random_state_) random_state_ = 1;
    next_word();
}
void TypingModel::prepare_online(TypingMode mode, WordPack pack, uint32_t seed, uint64_t now) {
    prepare(mode, now);
    records_.selected_pack = pack;
    random_state_ = seed ? seed : 1;
    word_.clear();
    next_word();
}
uint64_t TypingModel::limit_ms() const {
    if (records_.selected_mode == TypingMode::Practice) return 0;
    return records_.selected_mode == TypingMode::Sprint60 ? 60000 : 30000;
}
uint64_t TypingModel::elapsed_ms(uint64_t now) const {
    if (page_ == TypingPage::Results) return finished_elapsed_;
    return page_ == TypingPage::Running && now >= started_at_ ? now - started_at_ : 0;
}
uint64_t TypingModel::remaining_ms(uint64_t now) const {
    return limit_ms() - std::min(limit_ms(), elapsed_ms(now));
}
unsigned TypingModel::correct_prefix() const {
    unsigned prefix = 0;
    while (prefix < input_.size() && prefix < word_.size() && input_[prefix] == word_[prefix]) ++prefix;
    return prefix;
}
unsigned TypingModel::accuracy() const {
    return attempts_ ? static_cast<unsigned>(static_cast<uint64_t>(correct_attempts_) * 100 / attempts_) : 100;
}
unsigned TypingModel::wpm(uint64_t now) const {
    const auto elapsed = elapsed_ms(now);
    if (!elapsed) return 0;
    // Only retained, correct text counts toward WPM. Erasing and retyping
    // the same correct letters cannot inflate speed. One standard word = 5 chars.
    const auto characters = static_cast<uint64_t>(committed_characters_) + correct_prefix();
    return static_cast<unsigned>(std::min<uint64_t>(999, characters * 12000 / elapsed));
}
void TypingModel::type(char character, uint64_t now) {
    if (page_ != TypingPage::Ready && page_ != TypingPage::Running) return;
    if (character < 32 || character > 126 || (page_ == TypingPage::Ready && character == ' ')) return;
    if (tick(now)) return;
    if (page_ == TypingPage::Ready) { page_ = TypingPage::Running; started_at_ = now; }
    if (character >= 'A' && character <= 'Z') character = static_cast<char>(character - 'A' + 'a');
    add_bounded(attempts_); last_key_at_ = now;
    const auto position = input_.size();
    last_key_correct_ = position < word_.size() && word_[position] == character;
    if (!last_key_correct_) return;
    add_bounded(correct_attempts_);
    input_.push_back(character);
    if (input_ != word_) return;
    add_bounded(completed_words_);
    add_bounded(committed_characters_, static_cast<unsigned>(word_.size()));
    input_.clear();
    if (records_.selected_mode == TypingMode::Practice && completed_words_ == practice_words) finish(elapsed_ms(now));
    else next_word();
}
void TypingModel::backspace(uint64_t now) {
    if (page_ != TypingPage::Running || tick(now)) return;
    if (!input_.empty()) input_.pop_back();
    last_key_correct_ = true;
}
void TypingModel::finish(uint64_t elapsed) {
    if (page_ != TypingPage::Running) return;
    finished_elapsed_ = elapsed; page_ = TypingPage::Results;
    const auto speed = wpm(0);
    auto& record = records_.best_wpm[record_index()];
    new_best_ = speed > record;
    record = std::max(record, speed);
    add_bounded(records_.rounds); add_bounded(records_.words, completed_words_);
    dirty_ = true;
}
bool TypingModel::tick(uint64_t now) {
    if (page_ == TypingPage::Running && limit_ms() && elapsed_ms(now) >= limit_ms()) {
        finish(limit_ms()); return true;
    }
    return false;
}
void TypingModel::back() { page_ = TypingPage::Home; input_.clear(); }
void TypingModel::help() {
    if (page_ == TypingPage::Help) { page_ = help_return_; return; }
    if (page_ == TypingPage::Running) return;
    help_return_ = page_; page_ = TypingPage::Help;
}
void TypingModel::toggle_pack() {
    if (page_ != TypingPage::Home) return;
    records_.selected_pack = records_.selected_pack == WordPack::English ? WordPack::Code : WordPack::English;
    dirty_ = true;
}
void TypingModel::toggle_theme() { records_.daylight = !records_.daylight; dirty_ = true; }
} // namespace model
