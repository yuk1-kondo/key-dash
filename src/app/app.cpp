/* Derived from the MIT-licensed CardputerZero Template application. */
#include "app.h"
#include "asset_manager.h"
#include "base_viewmodel.h"
#include "typing_viewmodel.h"
#include "typing_screen.h"
#include "linux_input.h"
#include "monotonic_clock.h"
#include "logger.h"
#include "screenshot_service.h"
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#if USE_DESKTOP
#include "desktop_simulator_frame.h"
#else
#if APP_USE_DRM
#include "src/drivers/display/drm/lv_linux_drm.h"
#else
#include "src/drivers/display/fb/lv_linux_fbdev.h"
#endif
#endif
namespace app {
namespace {
volatile std::sig_atomic_t interrupted = 0;
void stop_requested(int) { interrupted = 1; }
bool key_listener(uint32_t key, const char*, bool held, void* context) {
    auto* typing = static_cast<viewmodel::TypingViewModel*>(context);
    if (held) return true;
    if (key == platform::kKeyPrintScreen) {
        const auto result = platform::screenshot::capture_active_screen();
        if (!result.success) std::fprintf(stderr, "Screenshot failed: %s\n", result.error.c_str());
    } else if (key == platform::kKeyHelp) {
        if (typing->state().page() == model::TypingPage::Home || typing->state().page() == model::TypingPage::Help)
            typing->key('4', platform::monotonic_ms());
    } else typing->key(key, platform::monotonic_ms());
    return true;
}
#if !USE_DESKTOP
lv_display_t* init_device_display() {
#if APP_USE_DRM
    auto* display = lv_linux_drm_create();
    if (!display) {
        return nullptr;
    }

    if (lv_linux_drm_set_file(display, APP_DRM_DEVICE, APP_DRM_CONNECTOR_ID) != LV_RESULT_OK) {
        lv_display_delete(display);
        return nullptr;
    }

    platform::init_key_input(display);
    return display;
#else
    auto* display = lv_linux_fbdev_create();
    if (!display) {
        return nullptr;
    }

    if (lv_linux_fbdev_set_file(display, APP_FRAMEBUFFER_DEVICE) != LV_RESULT_OK) {
        lv_display_delete(display);
        return nullptr;
    }

    platform::init_key_input(display);
    return display;
#endif
}
#endif


// Isolated developer rendering smoke: no synthetic inputs in normal launches.
class SmokeRun {
public:
    explicit SmokeRun(const char* output) : output_(output ? output : "") {}
    bool update(viewmodel::TypingViewModel& vm, uint64_t now) {
        if (output_.empty()) return false;
        if (!started_) { started_ = now; return false; }
        const auto elapsed = now - started_;
        if (stage_ == 0 && elapsed >= 250) {
            capture("home.png");
            vm.key('5', now); capture("online-menu.png");
            vm.key(LV_KEY_ENTER, now);
            if (vm.online_panel() == viewmodel::OnlinePanel::Name) {
                for (char key : std::string("player1")) vm.key(key, now);
                capture("online-name.png");
            }
            vm.key(LV_KEY_ESC, now); vm.key('4', now); ++stage_;
        }
        else if (stage_ == 1 && elapsed >= 500) { capture("help.png"); vm.key(LV_KEY_ESC, now); vm.key('0', now); ++stage_; }
        else if (stage_ == 2 && elapsed >= 750) { capture("night.png"); vm.key('0', now); vm.key(LV_KEY_NEXT, now); ++stage_; }
        else if (stage_ == 3 && elapsed >= 1000) { capture("code-menu.png"); vm.key('3', now); ++stage_; }
        else if (stage_ == 4 && elapsed >= 1250) {
            capture("practice.png"); vm.key(LV_KEY_ESC, now); vm.key(LV_KEY_NEXT, now); vm.key('1', now); ++stage_;
        } else if (stage_ == 5 && elapsed >= 1500) {
            capture("ready.png");
            const auto wrong = vm.state().word()[0] == 'x' ? 'z' : 'x';
            vm.key(wrong, now); capture("typo.png");
            vm.key(LV_KEY_BACKSPACE, now);
            vm.key(static_cast<uint32_t>(vm.state().word()[0]), now);
            capture("game.png");
            stage_ = 7;
        } else if (stage_ == 7) {
            if (now - last_key_ >= 170 && vm.state().page() == model::TypingPage::Running) {
                const auto& state = vm.state();
                vm.key(state.input().size() < state.word().size() ? static_cast<uint32_t>(state.word()[state.input().size()]) : ' ', now);
                last_key_ = now;
            }
            if (!captured_game_ && elapsed >= 6500 && vm.state().page() == model::TypingPage::Running) { capture("game.png"); captured_game_ = true; }
            if (vm.state().page() == model::TypingPage::Results) { capture("results.png"); vm.save(); ++stage_; return true; }
        }
        return false;
    }
    bool failed() const { return failed_; }
private:
    void capture(const char* name) {
        lv_refr_now(nullptr);
        const auto result = platform::screenshot::capture_to(output_ / name);
        if (!result.success) { failed_ = true; std::fprintf(stderr, "Smoke screenshot: %s\n", result.error.c_str()); }
    }
    std::filesystem::path output_;
    uint64_t started_{0}, last_key_{0};
    int stage_{0};
    bool captured_game_{false}, failed_{false};
};
}
int Application::run() {
    logger::Logger::init(); logger::Logger::set_tag("key-dash");
    std::signal(SIGINT, stop_requested); std::signal(SIGTERM, stop_requested);
    lv_init();
    AssetManager assets;
    viewmodel::BaseViewModel shell;
    viewmodel::TypingViewModel typing;
    shell.set_dark_mode(!typing.state().records().daylight);
#if USE_DESKTOP
    DesktopSimulatorFrame simulator(assets);
    auto* display = simulator.display(); simulator.bind_dark_mode(shell.dark_mode_subject());
#else
    auto* display = init_device_display();
#endif
    if (!display) { LOG_ERROR("Cannot initialize display"); return 1; }
    screen::TypingScreen scene(shell, assets, typing);
    lv_screen_load(scene.root());
    platform::set_global_key_listener(key_listener, &typing);
    SmokeRun smoke(std::getenv("KEY_DASH_SMOKE_DIR"));
    uint64_t last_frame = 0;
    while (!interrupted && !typing.quit_requested()
#if USE_DESKTOP
           && simulator.process_events()
#endif
    ) {
        const auto now = platform::monotonic_ms();
        typing.tick(now);
        if (shell.is_dark_mode() == typing.state().records().daylight) shell.set_dark_mode(!typing.state().records().daylight);
        if (now - last_frame >= 33) { scene.refresh(now); last_frame = now; }
        lv_timer_handler();
        if (smoke.update(typing, now)) break;
        lv_delay_ms(5);
    }
    platform::clear_global_key_listener(key_listener, &typing);
    typing.save();
    return smoke.failed() ? 2 : 0;
}
}
