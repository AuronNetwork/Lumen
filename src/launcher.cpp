// SPDX-License-Identifier: GPL-3.0-only
#include <windows.h>
#include <tlhelp32.h>
#include <aclapi.h>
#include <sddl.h>
#include <shellapi.h>
#include "launcher_ui.h"
#include "updater.h"
#include <exception>
#include <filesystem>
#include <string>
#include <vector>

namespace {
using launcher_ui::State;
struct Handle {HANDLE value{};~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}operator HANDLE()const{return value;}};
void status(std::wstring text){launcher_ui::post(State::Loading,std::move(text));}
DWORD minecraft() {
    Handle snap{CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0)};
    PROCESSENTRY32W entry{sizeof(entry)};DWORD result=0;int count=0;
    if(Process32FirstW(snap,&entry))do{if(!_wcsicmp(entry.szExeFile,L"Minecraft.Windows.exe")){result=entry.th32ProcessID;++count;}}while(Process32NextW(snap,&entry));
    return count==1?result:0;
}
std::vector<MODULEENTRY32W> modules(DWORD pid) {
    Handle snap{CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid)};
    std::vector<MODULEENTRY32W> result;MODULEENTRY32W entry{sizeof(entry)};
    if(Module32FirstW(snap,&entry))do{result.push_back(entry);}while(Module32NextW(snap,&entry));return result;
}
// Allow only read/execute on our DLL for a packaged game's token. Preserve
// the existing ACL; never modify Minecraft or Windows installation permissions.
bool allowPackagedRead(const std::wstring& path) {
    PSID sid{};if(!ConvertStringSidToSidW(L"S-1-15-2-1",&sid))return false;
    PACL oldAcl{},newAcl{};PSECURITY_DESCRIPTOR descriptor{};
    auto result=GetNamedSecurityInfoW(path.c_str(),SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,nullptr,nullptr,&oldAcl,nullptr,&descriptor);
    if(result==ERROR_SUCCESS){EXPLICIT_ACCESSW access{};access.grfAccessPermissions=GENERIC_READ|GENERIC_EXECUTE;access.grfAccessMode=GRANT_ACCESS;
        access.Trustee.TrusteeForm=TRUSTEE_IS_SID;access.Trustee.TrusteeType=TRUSTEE_IS_WELL_KNOWN_GROUP;access.Trustee.ptstrName=reinterpret_cast<LPWSTR>(sid);
        result=SetEntriesInAclW(1,&access,oldAcl,&newAcl);
        if(result==ERROR_SUCCESS)result=SetNamedSecurityInfoW(const_cast<LPWSTR>(path.c_str()),SE_FILE_OBJECT,DACL_SECURITY_INFORMATION,nullptr,nullptr,newAcl,nullptr);
    }
    if(newAcl)LocalFree(newAcl);if(descriptor)LocalFree(descriptor);LocalFree(sid);return result==ERROR_SUCCESS;
}
DWORD WINAPI start(void*) {
    auto finish=[](std::wstring text,State state=State::Error){launcher_ui::post(state,std::move(text));};
    wchar_t executable[32768]{};GetModuleFileNameW(nullptr,executable,32768);
    const auto dll=std::filesystem::path(executable).parent_path()/L"Lumen.dll";
    if(!std::filesystem::is_regular_file(dll)){finish(L"Lumen.dll is missing next to this launcher. Extract the entire package and try again.");return 0;}
    DWORD pid=minecraft();
    if(!pid){
        status(L"Starting Minecraft. Waiting for the main menu ...");
        if(reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",L"minecraft://",nullptr,nullptr,SW_SHOWNORMAL))<=32){finish(L"Could not open Minecraft. Start Minecraft 26.51 and try again.");return 0;}
        for(int i=0;i<120&&!pid;++i){Sleep(1000);pid=minecraft();}
        if(!pid){finish(L"Could not find a single Minecraft process. Run one instance of Minecraft 26.51 and try again.");return 0;}
        Sleep(5000);
    }
    const auto loaded=modules(pid);
    if(loaded.empty()){finish(L"Could not read Minecraft modules. Wait for the game to finish starting, then try again.");return 0;}
    for(const auto& m:loaded){
        if(!_wcsicmp(m.szModule,L"Latite.dll")||!_wcsicmp(m.szModule,L"LatiteNightly.dll")||!_wcsicmp(m.szModule,L"LatiteDebug.dll")){
            finish(L"Latite is still loaded. Close Minecraft and restart without Latite, then try again.");return 0;}
        if(!_wcsicmp(m.szModule,L"Lumen.dll")){finish(L"Lumen is already loaded. Enter a world and press Insert. You can close this launcher.",State::Loaded);return 0;}
    }
    for(const auto& m:loaded)if(!_wcsicmp(m.szModule,L"XrayLight.dll")){finish(L"Xray Light is still loaded. Save, close and restart Minecraft before loading Lumen.");return 0;}
    if(!allowPackagedRead(dll.wstring())){finish(L"Could not grant Minecraft read access to Lumen.dll.");return 0;}
    const auto assets=dll.parent_path()/L"assets";
    if(!std::filesystem::is_directory(assets)||!allowPackagedRead(assets.wstring())){finish(L"The assets folder is missing or unreadable. Extract the entire package.");return 0;}
    for(const auto* name:{L"Geist-Regular.ttf",L"Geist-SemiBold.ttf"}){
        const auto file=assets/name;
        if(!std::filesystem::is_regular_file(file)||!allowPackagedRead(file.wstring())){finish(L"The Geist font files are missing or unreadable. Extract the entire package.");return 0;}
    }
    Handle process{OpenProcess(PROCESS_CREATE_THREAD|PROCESS_QUERY_INFORMATION|PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_VM_READ,FALSE,pid)};
    if(!process){finish(L"Could not access Minecraft. Windows error "+std::to_wstring(GetLastError()));return 0;}
    const auto localLoad=GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"LoadLibraryW");
    HMODULE owner{};GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(localLoad),&owner);
    wchar_t ownerPath[32768]{};GetModuleFileNameW(owner,ownerPath,32768);
    const auto ownerName=std::filesystem::path(ownerPath).filename().wstring();
    uintptr_t remoteLoad{};
    for(const auto& m:loaded)if(!_wcsicmp(m.szModule,ownerName.c_str()))remoteLoad=reinterpret_cast<uintptr_t>(m.modBaseAddr)+(reinterpret_cast<uintptr_t>(localLoad)-reinterpret_cast<uintptr_t>(owner));
    if(!remoteLoad){finish(L"Could not locate the Windows library loader.");return 0;}
    const std::wstring path=dll.wstring();const size_t bytes=(path.size()+1)*sizeof(wchar_t);
    void* remote=VirtualAllocEx(process,nullptr,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!remote){finish(L"Could not allocate memory for the DLL path.");return 0;}
    SIZE_T written{};
    if(!WriteProcessMemory(process,remote,path.c_str(),bytes,&written)||written!=bytes){VirtualFreeEx(process,remote,0,MEM_RELEASE);finish(L"Could not copy the DLL path to Minecraft.");return 0;}
    status(L"Loading Lumen ...");
    Handle thread{CreateRemoteThread(process,nullptr,0,reinterpret_cast<LPTHREAD_START_ROUTINE>(remoteLoad),remote,0,nullptr)};
    if(!thread){VirtualFreeEx(process,remote,0,MEM_RELEASE);finish(L"Could not start the DLL loader. Windows error "+std::to_wstring(GetLastError()));return 0;}
    if(WaitForSingleObject(thread,30000)!=WAIT_OBJECT_0){finish(L"Loading is taking longer than expected. Check Minecraft; do not load Lumen again.",State::Uncertain);return 0;}
    VirtualFreeEx(process,remote,0,MEM_RELEASE);
    bool present=false;for(const auto& m:modules(pid))if(!_wcsicmp(m.szModule,L"Lumen.dll"))present=true;
    if(!present){finish(L"Windows could not load Lumen.dll. Extract the DLL and launcher together.");return 0;}
    finish(L"Enter a world and press Insert. You can close this launcher.",State::Loaded);return 0;
}
DWORD WINAPI safeStart(void*) {
    try {return start(nullptr);}
    catch(const std::exception&) {launcher_ui::post(State::Error,L"Could not read the Lumen files. Extract the entire package into a writable folder and try again.");}
    catch(...) {launcher_ui::post(State::Error,L"The loading task failed unexpectedly. Restart the launcher and try again.");}
    return 0;
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int) {
    // Serialize startup and updates for this Windows session.
    Handle mutex{CreateMutexW(nullptr,FALSE,L"Local\\AuronNetwork.Lumen.Launcher")};
    if(!mutex.value)return 1;
    if(GetLastError()==ERROR_ALREADY_EXISTS){if(auto existing=FindWindowW(L"Lumen.Launcher.Figma",nullptr)){ShowWindow(existing,SW_RESTORE);SetForegroundWindow(existing);}return 0;}
    return launcher_ui::run(instance,safeStart,false,updates::startup);
}
