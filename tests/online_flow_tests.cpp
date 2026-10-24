// Explicit integration check against a LOCAL Wrangler/D1 instance, never a public board.
#include "typing_viewmodel.h"
#include "monotonic_clock.h"
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <thread>
using namespace std::chrono_literals;
uint64_t now() { return platform::monotonic_ms(); }
void pump(viewmodel::TypingViewModel& vm) {
    const auto deadline = now() + 12000;
    do { vm.tick(now()); std::this_thread::sleep_for(10ms); } while (vm.online_panel() == viewmodel::OnlinePanel::Busy && now() < deadline);
    assert(vm.online_panel() != viewmodel::OnlinePanel::Busy);
}
int main() {
    const char* endpoint = std::getenv("KEY_DASH_API_URL");
    if (!endpoint || std::string(endpoint).rfind("http://127.0.0.1:", 0) != 0 || !std::getenv("KEY_DASH_STATE_DIR")) {
        std::cerr << "Use an isolated KEY_DASH_STATE_DIR and local KEY_DASH_API_URL.\n"; return 2;
    }
    lv_init(); viewmodel::TypingViewModel vm;
    const auto name = "qa" + std::to_string(now() % 1000000000);
    vm.key(LV_KEY_NEXT, now()); assert(vm.state().records().selected_pack == model::WordPack::Code);
    vm.key('5', now()); assert(vm.online_panel() == viewmodel::OnlinePanel::Menu);
    vm.key(LV_KEY_ENTER, now()); assert(vm.online_panel() == viewmodel::OnlinePanel::Name);
    for (char ch : name) vm.key(ch, now());
    vm.key(LV_KEY_ENTER, now()); pump(vm);
    assert(vm.player_name() == name);
    const auto identity = std::filesystem::path(std::getenv("KEY_DASH_STATE_DIR")) / "online-player.json";
    assert(std::filesystem::exists(identity));
    assert((std::filesystem::status(identity).permissions() & std::filesystem::perms::others_read) == std::filesystem::perms::none);
    vm.key('2', now()); vm.key(LV_KEY_NEXT, now());
    vm.key(LV_KEY_ENTER, now()); pump(vm);
    assert(vm.state().records().selected_pack == model::WordPack::English && vm.state().limit_ms() == 30000);
    assert(vm.state().page() == model::TypingPage::Ready && vm.online_round());
    vm.key(LV_KEY_ENTER, now()); assert(vm.state().page() == model::TypingPage::Ready);
    const auto started = now();
    for (unsigned count = 0; count < 8; count++) {
        const auto word = vm.state().word();
        if (!count) { vm.key(word[0] == 'z' ? 'x' : 'z', now()); std::this_thread::sleep_for(60ms); vm.key(LV_KEY_BACKSPACE, now()); }
        for (char ch : word) { vm.key(ch, now()); std::this_thread::sleep_for(60ms); }
        assert(vm.state().input().empty() && vm.state().completed_words() == count + 1);
    }
    while (now() - started < 30030) { vm.tick(now()); std::this_thread::sleep_for(10ms); }
    vm.tick(now()); assert(vm.state().page() == model::TypingPage::Results);
    const auto deadline = now() + 12000;
    while (vm.online_message().find("Uploading") != std::string::npos && now() < deadline) { vm.tick(now()); std::this_thread::sleep_for(10ms); }
    if (vm.online_message().find("Score registered!") == std::string::npos) { std::cerr << vm.online_message() << '\n'; return 1; }
    assert(vm.state().mistakes() == 1);
    const auto wpm = vm.state().wpm(0), accuracy = vm.state().accuracy();
    assert(wpm > 0 && accuracy < 100);
    vm.key('4', now()); pump(vm); assert(vm.online_panel() == viewmodel::OnlinePanel::Board);
    bool found = false;
    for (const auto& row : vm.ranking()) if (row.name == name && row.wpm == wpm && row.accuracy == accuracy) found = true;
    assert(found);
    vm.key('6', now()); pump(vm);
    assert(vm.online_panel() == viewmodel::OnlinePanel::Contact);
    assert(vm.contact_code().size() == 14);
    assert(vm.contact_url().rfind("https://yuk1-kondo.github.io/key-dash-ranking/#contact=", 0) == 0);
    vm.tick(now() + 600001);
    assert(vm.contact_url().empty() && vm.contact_code().empty());
    assert(vm.online_message().find("expired") != std::string::npos);
    std::cout << "PASS: native private contact QR issuance and local expiry.\n";
    std::cout << "PASS: native registration -> deterministic challenge -> typo correction -> score upload -> D1 -> native leaderboard (" << wpm << " WPM / " << accuracy << "%).\n";
}
