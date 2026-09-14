// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <windows.h>
#include <string>

namespace launcher_ui {
enum class State { Ready, Loading, Loaded, Error, Uncertain, CheckingUpdates, DownloadingUpdate, InstallerStarted };
// Worker threads only post immutable state; the window owns rendering and controls.
void post(State state, std::wstring detail);
int run(HINSTANCE instance, LPTHREAD_START_ROUTINE start, bool preview = false, LPTHREAD_START_ROUTINE startup = nullptr);
}
