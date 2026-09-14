// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <windows.h>
#include <filesystem>
#include <optional>
#include <string>
#include "update_policy.h"
namespace updates {
std::optional<Release> latest();
std::filesystem::path download(const Release& release);
std::string sha256(const std::filesystem::path& path);
DWORD WINAPI startup(void*);
}
