// SPDX-License-Identifier: GPL-3.0-only
#include "updater.h"
#include "launcher_ui.h"
#include "version.h"
#include <winhttp.h>
#include <bcrypt.h>
#include <tlhelp32.h>
#include <fstream>
#include <functional>
#include <vector>

namespace updates { namespace {
struct Internet {HINTERNET value{};~Internet(){if(value)WinHttpCloseHandle(value);}operator HINTERNET()const{return value;}};
struct Handle {HANDLE value{};~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}operator HANDLE()const{return value;}};
struct Algorithm {BCRYPT_ALG_HANDLE value{};~Algorithm(){if(value)BCryptCloseAlgorithmProvider(value,0);}};
struct Hash {BCRYPT_HASH_HANDLE value{};~Hash(){if(value)BCryptDestroyHash(value);}};
std::wstring wide(const std::string& value){return std::wstring(value.begin(),value.end());}
std::filesystem::path folder(){wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);return std::filesystem::path(path).parent_path();}
std::filesystem::path cache(){wchar_t path[32768]{};const auto count=GetEnvironmentVariableW(L"LOCALAPPDATA",path,32768);if(!count||count>=32768)throw std::runtime_error("LOCALAPPDATA is unavailable");return std::filesystem::path(path)/L"Lumen"/L"Updates";}
void log(const std::string& message){try{const auto dir=cache();std::filesystem::create_directories(dir);std::ofstream file(dir/L"updater.log",std::ios::app);file<<LUMEN_VERSION<<" "<<message<<'\n';}catch(...){}}
bool minecraftRunning(){Handle snap{CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0)};if(snap.value==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot inspect running applications");PROCESSENTRY32W e{sizeof(e)};
    if(Process32FirstW(snap,&e))do{if(!_wcsicmp(e.szExeFile,L"Minecraft.Windows.exe"))return true;}while(Process32NextW(snap,&e));return false;
}
// Download only GitHub release metadata/assets with normal TLS verification.
// Redirects may lead to GitHub's asset CDN, but can never downgrade to HTTP.
void request(const std::wstring& url,std::uint64_t limit,const std::function<void(const char*,DWORD)>& consume){
    URL_COMPONENTS parts{sizeof(parts)};parts.dwHostNameLength=parts.dwUrlPathLength=parts.dwExtraInfoLength=DWORD(-1);
    if(!WinHttpCrackUrl(url.c_str(),DWORD(url.size()),0,&parts)||parts.nScheme!=INTERNET_SCHEME_HTTPS)throw std::runtime_error("HTTPS is required");
    const std::wstring host(parts.lpszHostName,parts.dwHostNameLength);
    if(!allowedHost(host))throw std::runtime_error("Unexpected update host");
    const std::wstring path=std::wstring(parts.lpszUrlPath,parts.dwUrlPathLength)+(parts.dwExtraInfoLength?std::wstring(parts.lpszExtraInfo,parts.dwExtraInfoLength):L"");
    Internet session{WinHttpOpen(L"Lumen/" LUMEN_VERSION_W,WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
    if(!session.value)throw std::runtime_error("Cannot open HTTPS session");
    WinHttpSetTimeouts(session,5000,5000,10000,10000);
    DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;WinHttpSetOption(session,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect));
    Internet connection{WinHttpConnect(session,host.c_str(),parts.nPort,0)};if(!connection.value)throw std::runtime_error("Cannot connect to GitHub");
    Internet call{WinHttpOpenRequest(connection,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};
    if(!call.value)throw std::runtime_error("Cannot create update request");
    const wchar_t* headers=L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
    if(!WinHttpSendRequest(call,headers,DWORD(-1),WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(call,nullptr))throw std::runtime_error("GitHub is unavailable");
    DWORD status{},size=sizeof(status);if(!WinHttpQueryHeaders(call,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX)||status!=200)throw std::runtime_error("GitHub response "+std::to_string(status));
    DWORD urlSize=0;WinHttpQueryOption(call,WINHTTP_OPTION_URL,nullptr,&urlSize);std::vector<wchar_t> finalUrl(urlSize/sizeof(wchar_t)+1);
    if(!WinHttpQueryOption(call,WINHTTP_OPTION_URL,finalUrl.data(),&urlSize))throw std::runtime_error("Cannot verify download destination");
    URL_COMPONENTS destination{sizeof(destination)};destination.dwHostNameLength=DWORD(-1);
    if(!WinHttpCrackUrl(finalUrl.data(),0,0,&destination)||destination.nScheme!=INTERNET_SCHEME_HTTPS||!allowedHost(std::wstring_view(destination.lpszHostName,destination.dwHostNameLength)))throw std::runtime_error("Unexpected download destination");
    const auto began=GetTickCount64();std::uint64_t total=0;char buffer[65536];DWORD read{};
    while(WinHttpReadData(call,buffer,sizeof(buffer),&read)){
        if(!read)return;total+=read;if(total>limit||GetTickCount64()-began>120000)throw std::runtime_error("Update download exceeded its limit");consume(buffer,read);
    }throw std::runtime_error("Incomplete update download");
}
std::wstring quoted(const std::wstring& value){std::wstring result=L"\"";size_t slashes=0;for(auto c:value){if(c==L'\\'){++slashes;continue;}if(c==L'\"')result.append(slashes*2+1,L'\\');else result.append(slashes,L'\\');slashes=0;result+=c;}result.append(slashes*2,L'\\');return result+L"\"";}
void install(const std::filesystem::path& setup){
    // Setup waits for this exact parent to exit before checking or replacing files.
    // It never closes Minecraft and never requests a machine restart.
    auto command=quoted(setup.wstring())+L" /SILENT /SUPPRESSMSGBOXES /NORESTART /NOCLOSEAPPLICATIONS /NORESTARTAPPLICATIONS /LUMENAUTOUPDATE=1 /LUMENWAITPID="+std::to_wstring(GetCurrentProcessId())+L" /DIR="+quoted(folder().wstring())+L" /LOG="+quoted((cache()/L"setup.log").wstring());
    STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION pi{};
    if(!CreateProcessW(setup.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,setup.parent_path().c_str(),&si,&pi))throw std::runtime_error("Could not start update installer");
    CloseHandle(pi.hThread);CloseHandle(pi.hProcess);log("INSTALLER_STARTED");launcher_ui::post(launcher_ui::State::InstallerStarted,L"");
}
} // namespace

std::string sha256(const std::filesystem::path& path){
    Algorithm algorithm;Hash hash;
    if(BCryptOpenAlgorithmProvider(&algorithm.value,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0||BCryptCreateHash(algorithm.value,&hash.value,nullptr,0,nullptr,0,0)<0)throw std::runtime_error("Cannot initialize SHA-256");
    std::ifstream file(path,std::ios::binary);if(!file)throw std::runtime_error("Cannot read installer");char buffer[65536];
    while(file){file.read(buffer,sizeof(buffer));if(file.gcount()&&BCryptHashData(hash.value,reinterpret_cast<PUCHAR>(buffer),ULONG(file.gcount()),0)<0)throw std::runtime_error("Cannot hash installer");}
    if(!file.eof())throw std::runtime_error("Cannot finish reading installer");
    unsigned char bytes[32]{};if(BCryptFinishHash(hash.value,bytes,sizeof(bytes),0)<0)throw std::runtime_error("Cannot finish SHA-256");
    constexpr char digits[]="0123456789abcdef";std::string result;for(auto byte:bytes){result+=digits[byte>>4];result+=digits[byte&15];}return result;
}
std::optional<Release> latest(){std::string json;request(L"https://api.github.com/repos/"+wide(LUMEN_REPOSITORY)+L"/releases/latest",2*1024*1024,[&](const char* data,DWORD n){json.append(data,n);});return release(json,LUMEN_REPOSITORY);}
std::filesystem::path download(const Release& release){
    const auto dir=cache()/wide(release.tag+"-"+release.sha256.substr(0,16));std::filesystem::create_directories(dir);const auto file=dir/L"Lumen-Setup.exe";
    if(std::filesystem::is_regular_file(file)&&std::filesystem::file_size(file)==release.size&&sha256(file)==release.sha256)return file;
    const auto partial=dir/(L"download-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64())+L".part");
    try{
        std::ofstream stream(partial,std::ios::binary|std::ios::trunc);if(!stream)throw std::runtime_error("Cannot create update file");
        request(wide(release.url),release.size,[&](const char* data,DWORD n){stream.write(data,n);if(!stream)throw std::runtime_error("Cannot save update");});stream.close();
        if(!stream||std::filesystem::file_size(partial)!=release.size||sha256(partial)!=release.sha256)throw std::runtime_error("Installer checksum mismatch");
        if(!MoveFileExW(partial.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot finish update file");return file;
    }catch(...){std::error_code ignored;std::filesystem::remove(partial,ignored);throw;}
}
DWORD WINAPI startup(void*){
    using launcher_ui::State;
    try{
        if(!std::filesystem::is_regular_file(folder()/L"lumen-install.ini")){launcher_ui::post(State::Ready,L"Minecraft will open automatically. Install Lumen to enable automatic updates.");return 0;}
        const auto ini=cache().parent_path()/L"updates.ini";
        if(GetPrivateProfileIntW(L"Updates",L"Enabled",1,ini.c_str())==0){launcher_ui::post(State::Ready,L"Automatic updates are disabled. Minecraft will open automatically.");return 0;}
        const auto found=latest();
        if(!found||found->number<=version(LUMEN_VERSION)){launcher_ui::post(State::Ready,L"You are up to date. Minecraft will open automatically if needed.");return 0;}
        if(minecraftRunning()){launcher_ui::post(State::Ready,L"Update "+wide(found->tag)+L" is available. Close Minecraft and reopen Lumen to install it.");log("DEFERRED_MINECRAFT_RUNNING "+found->tag);return 0;}
        launcher_ui::post(State::DownloadingUpdate,L"Downloading "+wide(found->tag)+L". Lumen will restart after the update.");
        const auto setup=download(*found);
        if(minecraftRunning()){launcher_ui::post(State::Ready,L"Update downloaded. Close Minecraft and reopen Lumen to install it.");log("DEFERRED_AFTER_DOWNLOAD");return 0;}
        // Recheck the cached payload immediately before executing it.
        if(sha256(setup)!=found->sha256)throw std::runtime_error("Installer changed after verification");
        install(setup);
    }catch(const std::exception& e){log(std::string("UPDATE_ERROR ")+e.what());launcher_ui::post(State::Ready,L"Update check or download unavailable. You can still launch your installed version.");}
    return 0;
}
} // namespace updates
