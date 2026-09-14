// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <windows.h>
#include <string>
namespace overlay {
enum Command { Xray=1,Walls,Radius,Limit,Zoom,Brightness,ZoomFactor,Ore,Close };
struct State {bool xray{},walls{},zoom{},brightness{};int radius{},limit{},zoomFactor{};unsigned ores{};std::wstring status;};
using Read=State(*)();using Change=void(*)(Command,int);using Log=void(*)(const std::string&);
void initialize(HMODULE module,Read read,Change change,Log log);
void setOpen(bool value);
bool isOpen();
bool input(HWND window,UINT message,WPARAM key,LPARAM data);
}
