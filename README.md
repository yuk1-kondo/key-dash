# M5 Cardputer Zero Typing (M5CZT)

**Small keys. Quick fingers.**

Rules 2 (v0.4.1): wrong letters count as misses and never advance the cursor. Type the expected letter to continue. The final correct letter automatically starts the next word; no Enter/Space confirmation is needed. Backspace undoes accepted text. Live and result screens show misses. WPM counts actual accepted letters only (five characters per standard word). Legacy local bests and online rankings are retained separately. A pocket typing game for the CardputerZero keyboard.

![M5CZT](screenshot/game.png)

C++17 / LVGL 9.5. Based on the MIT-licensed CardputerZero Template and the local KEY CRITTER display, keyboard, packaging and screenshot infrastructure. Offline play works without a connection. Optional online challenges use a public player name and a private per-player key. No root execution, external hardware, or background service.

Public leaderboard: https://yuk1-kondo.github.io/key-dash-ranking/

## Play

- **1**: 30-second sprint.
- **2**: 60-second sprint.
- **3**: untimed practice; finish ten words.
- **Tab**: English / Code vocabulary (64 short words in each).
- **0**: paper / night theme, from the menu or results.
- **4 / hardware Help**: controls, from the menu.
- **5**: online challenge and leaderboards. Enter registers a player name, then starts an English-only 30-second challenge; 4 opens the top five. Name and score are public. Wi-Fi is required.
- Online results upload automatically; **5** retries a failed upload with the same challenge. Scores need at least one completed word. **4** opens rankings from online results.
- **Enter**: replay the selected mode.
- **Esc**: return to the menu; from the menu, save and quit.

The clock starts with your first typed character, not on entering the ready screen; Enter and initial Space do not start it. Type the expected lowercase letter; wrong keys count as misses without changing the accepted prefix. The final correct letter advances immediately, without Enter or Space. **Backspace** undoes an accepted letter. Uppercase input is accepted case-insensitively. Physical key-repeat is ignored: each character needs a separate press. See [controls and bottom-bar documentation](docs/controls.md).

The built-in desktop simulator also lets you click the physical keys on the CardputerZero illustration. There is no Japanese IME/romaji-conversion mode in this first version.

## Scoring

- **WPM** = retained correct characters × 12,000 ÷ elapsed milliseconds, capped at 999 for display. A standard word is five characters. Each confirmed word includes its separating space. A correct prefix of the current word counts at timeout.
- Erasing and retyping the same correct letters does **not** inflate WPM.
- **Accuracy** = correct printable-key attempts ÷ all printable-key attempts. Backspace does not count as a new attempt, and correcting a typo does not remove it from the attempt history. Premature/wrong Space/Enter submissions count as errors.
- Timed rounds end at exactly 30 or 60 seconds. Late input cannot extend the round or increase the score. Untimed practice ends after ten correctly confirmed words.
- Personal bests are independent for all three modes and both word packs. A cancelled round creates no record. Practice also records its own best, separately from timed sprints.

## StackChan companion

The mascot follows the appearance of the official M5Stack StackChan K151: white housing, black face, circular lower camera and grey base. A small neutral companion accompanies typing, with a larger happy expression on results. This is an independent game illustration, not an official M5Stack app. [Credits and references](assets/images/STACKCHAN-CREDITS.md).

## Optional private contact email

After registering a player name, select **5 Online → 6 Contact**. Scan the QR on a phone, or enter its code on the leaderboard site's **連絡先（任意）** form. Email registration is optional and does not affect scores or rank eligibility. The address is stored privately only after an explicit checkbox for competition/result contact. The administrator contacts players manually; email ownership verification and automatic email sending are outside this version.

The QR contains a 10-minute, one-use contact code, never the player's private key. The site supports saving, replacing and deleting contact details with a fresh code. Deleting a contact preserves scores. The app never receives or stores the email address. The public leaderboard API never returns it. See [online/README.md](online/README.md) for retention and authorized administrator lookup.

## Online rankings

The site is hosted on GitHub Pages; Cloudflare Worker + D1 handles public registration, challenges, server-side replay scoring and storage. GitHub login is not required for players. Online play has one division: English / 30 seconds. Previous divisions remain readable in the website archive. See [online/README.md](online/README.md) for the API, limits, data retention and setup.

Only selecting Online makes network requests. Normal play is local. Registration uses a public 3–12 character ASCII name; a random personal key is stored as `online-player.json` beside the save with owner-only file permissions. Keep a private backup when changing devices; there is no password-recovery flow. Do not share that file or put it in GitHub Issues.

A network thread performs bounded HTTPS requests with certificate verification and an 8-second timeout. The compiled API URL can be changed with CMake `KEY_DASH_API_URL`; the environment variable of the same name overrides it at runtime. HTTP is accepted only for localhost development. No administrative or GitHub tokens are embedded in the app. The public endpoint is `https://key-dash-ranking.yukilab-m.workers.dev`.

Online runs use a server-issued word seed. The server recalculates WPM/accuracy from the timed key events, rejects premature/expired submissions, and safely accepts retries. This reduces casual score forgery; it is not proof against bots or a modified client. Input events are not retained in D1. Names and bests remain public until deletion is requested from the ranking repository administrator.

For an isolated native→API→D1→native leaderboard integration test, run a local Wrangler instance as described in `online/README.md`, then:

```sh
KEY_DASH_API_URL=http://127.0.0.1:8787 \
KEY_DASH_STATE_DIR="$PWD/.cache/online-test-state" \
./build/darwin-arm64/Debug/typing-online-flow-tests
```

This test takes 30 seconds and refuses a public endpoint. It registers a test player in the local database, corrects a typo, submits a round, and verifies the actual returned ranking.

## Save data

Best WPM, completed sessions/words, selected mode, word pack and theme are saved locally. Default: `$XDG_CONFIG_HOME/key-dash/state.txt`, or `$HOME/.config/key-dash/state.txt`. Set **KEY_DASH_STATE_DIR** to an absolute directory for isolated testing.

Writes use a temporary file and same-directory rename. Invalid saves are preserved and a visible warning is shown. Saves happen after completed rounds, about once a second after setting changes, and on normal exit / SIGINT / SIGTERM. Power loss or SIGKILL can lose recent unsaved settings. Use one app instance per save directory.

**KEY_DASH_SCREENSHOT_DIR** overrides the normal `~/Pictures/Screenshots` location for **PrintScreen / Fn+J**. Captures are native 320×170 PNGs.

## Build and test on macOS

Requires Apple command-line tools and CMake >= 3.31, Ninja, SDL2, libpng, JPEG, FreeType, fmt and zlib:

```sh
brew install cmake ninja sdl2 libpng jpeg freetype fmt zlib
cmake --preset darwin-arm64
cmake --build --preset darwin-arm64-dbg -j 6
ctest --test-dir build/darwin-arm64 -C Debug --output-on-failure
./build/darwin-arm64/Debug/key-dash
```

Generate a local Apple Silicon preview bundle (bundles local Homebrew dynamic dependencies and ad-hoc signs the `.app`; this is not a notarized macOS distribution):

```sh
python3 scripts/bundle_macos_preview.py
```

A developer-only rendering smoke run takes about 32 seconds. It captures home, help, both themes, vocabulary selection, practice-ready, timed-ready, a typo, active gameplay and the timed result. It runs synthetic inputs only when **KEY_DASH_SMOKE_DIR** is explicitly set. Always use an isolated save directory:

```sh
SDL_VIDEODRIVER=dummy \
KEY_DASH_STATE_DIR="$PWD/temp/smoke-state" \
KEY_DASH_SMOKE_DIR="$PWD/temp/screenshots" \
./build/darwin-arm64/Debug/key-dash
```

The simulator's physical-device illustration comes from the official Template. All screenshots in this repository are simulator captures, not claims of hardware execution.

## CardputerZero build and package

Put an `aarch64-linux-gnu-gcc/g++` toolchain on PATH. The first configure downloads the official BSP and pinned build dependencies:

```sh
cmake --preset cp0-cross
cmake --build --preset cp0-cross-rel -j 6
python3 scripts/package_deb.py
python3 scripts/validate_release.py
```

The Python packager verifies a genuine ELF64 AArch64 executable and produces `dist/key-dash_0.4.1-rev1_arm64.deb`. It works on macOS, whose CPack distribution lacks the DEB generator. The device package contains the binary, APPLaunch entry, icon, documentation and applicable static-dependency licenses; no daemon is installed.

After copying the package to the device:

```sh
sudo apt install ./key-dash_0.4.1-rev1_arm64.deb
```

APPLaunch starts `/usr/bin/key-dash` as the logged-in user. The default display is `/dev/fb0`; the existing platform driver discovers evdev keyboards, with A–Z, digits, Space, Enter, Backspace and Tab mapping. Existing non-root access to the device nodes is required. Do not run as root.

**Physical-device execution is not yet verified.** Cross-compilation and simulator tests do not establish display/input compatibility or performance on a real CardputerZero.

## Store preparation

`app-builder.json` is an unpublished Store draft. Network permission is true for optional online features; the other six optional permissions remain false. The source repository is https://github.com/yuk1-kondo/key-dash (currently private), while the ranking website/API source is public at https://github.com/yuk1-kondo/key-dash-ranking. Store share code `M5ZT` is prepared and the official metadata preflight passed. Initial Store submission is waiting for developer-portal GitHub login and a real-device demo video. Repository visibility is pending the owner’s choice. See [submission checklist](docs/store-submission.md).

Before submission, test on the real device and film **APPLaunch → start → typing and typo correction → round result → exit**. Attach that physical-device demo to the Store submission PR. Simulator video is not a substitute. GitHub publication does not constitute a Store submission.

## Structure / license

- `src/model/typing_model.*`: words, attempts, accuracy, time limits, WPM and record persistence, without LVGL.
- `src/viewmodel/typing_viewmodel.*`, `typing_online.cpp`: key actions, round finalization, online flow and reactive updates.
- `src/network/online_client.*`: bounded asynchronous HTTPS requests.
- `online/`: Pages site, Worker, D1 schema and API tests (mirrored to the public ranking repository).
- `src/view/screens/typing_screen.*`: native 320×170 home, game, results and help.
- `src/platform/monotonic_clock.h`: shared monotonic clock for keys, rendering and deadlines.
- `tests/typing_model_tests.cpp`, `tests/typing_controller_tests.cpp`: correction history, score invariants, deadline boundaries, persistence and keyboard routing.

MIT. Existing Template notices remain intact. The original keycap monogram is generated by `scripts/generate_icon.py`. Third-party static licenses accompany the Debian package, and macOS dependency licenses are in the preview's Resources directory.

## 配布物・作業資料

配布パッケージ、Macプレビュー、操作ガイド、検証画像、デザイン検討資料は [artifacts/](artifacts/README.md) に保存しています。
