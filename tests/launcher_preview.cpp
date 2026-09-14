// SPDX-License-Identifier: GPL-3.0-only
// Visual test harness. No Minecraft discovery, launch, memory access or injection.
#include "../src/launcher_ui.h"
#include <string>
namespace {
std::wstring mode;
DWORD WINAPI preview(void*) {
    launcher_ui::post(launcher_ui::State::Loading,L"Waiting for the main menu. Please keep this launcher open.");
    Sleep(8000);
    if(mode==L"error")launcher_ui::post(launcher_ui::State::Error,L"Lumen.dll is missing next to this launcher. Extract the entire package and try again.");
    else if(mode==L"uncertain")launcher_ui::post(launcher_ui::State::Uncertain,L"Loading is taking longer than expected. Check Minecraft; do not load Lumen again.");
    else launcher_ui::post(launcher_ui::State::Loaded,L"Enter a world and press Insert. You can close this launcher.");
    return 0;
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR command,int){mode=command;return launcher_ui::run(instance,preview,true);}
