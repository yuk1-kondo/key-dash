/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>
#include <filesystem>

namespace platform::screenshot {

struct CaptureResult {
    bool success{false};
    std::string path;
    std::string error;
};

CaptureResult capture_active_screen();
CaptureResult capture_to(const std::filesystem::path& path);

} // namespace platform::screenshot
