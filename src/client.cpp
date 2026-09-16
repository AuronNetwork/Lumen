// SPDX-License-Identifier: GPL-3.0-only
// Original Bedrock ABI/signatures adapted from Latite, commit 2271ce9.
// Bedrock 26.51 layout and signature audit: docs/BEDROCK-26.51.md.
// See NOTICE.md. No Latite runtime, plugin host, network or chat API is used.
#include <windows.h>
#include <commctrl.h>
#include <MinHook.h>
#include <atomic>
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include "scanner.h"
#include "view_features.h"
#include "overlay.h"
#include "version.h"
#include "bedrock_version.h"
#include "mouse_input.h"

using namespace xray;
namespace {
HMODULE dll{};
std::atomic<HWND> settings{}, gameWindow{};
std::atomic<bool> closeRequested{};
std::atomic<bool> mouseReleaseFence{};
uintptr_t grabCursor{},releaseCursor{},mouseGlobal{};
bool menuWasCaptured{};
void setMenu(bool open,bool resume=true);
void saveSettings();
overlay::State uiState();
void uiChange(overlay::Command command,int value);

std::atomic<bool> fullbright{false}, zoomEnabled{true}, zoomHeld{false}, viewFault{false};
std::atomic<int> zoomFactor{4};
std::atomic<uint64_t> zoomFrames{}, gammaCalls{}, gammaOverrides{};
std::atomic<float> lastGamma{};
std::atomic<bool> enabled{false}, through{true}, fault{false}, ready{false};
std::atomic<int> radius{16}, maxBoxes{256};
std::atomic<unsigned> oreMask{defaultMask}, revision{0};
std::atomic<uint64_t> readCount{}, sweepCount{}, frameCount{};
std::atomic<size_t> boxCount{};
std::mutex scanMutex, logMutex;
Scanner scanner;
std::vector<Hit> snapshot;
void* oldRegion{}, *oldDimension{};
unsigned previousRevision{};
std::wstring configPath, logPath;
constexpr UINT ToggleMenu=WM_APP+1;
uintptr_t platformGlobal{}, materialGroup{}, tessBegin{}, tessColor{}, tessVertex{}, meshRender{};
using TickFn=void(*)(void*);
using RenderFn=void(*)(void*,void*,void*);
using LeaveFn=void*(*)(void*);
using WindowFn=LRESULT(*)(HWND,UINT,WPARAM,LPARAM);
TickFn originalTick{};
RenderFn originalRender{};
LeaveFn originalLeave{};
WindowFn originalWindow{};
using GammaFn=float(*)(void*,void*);
GammaFn originalGamma{};
using MouseFn=bool(*)(void*,void*,void*);
MouseFn originalMouse{};
using CursorFn=void(*)(void*);
CursorFn originalGrabCursor{},originalReleaseMouse{};
std::atomic<unsigned> blockedCursorGrabs{};

float gamma(void* options,void* context) {
    const float original=originalGamma(options,context);
    ++gammaCalls;lastGamma=original;
    const bool active=fullbright.load();if(active)++gammaOverrides;
    return lumen::gammaValue(original,active);
}

void log(const std::string& value) {
    std::lock_guard lock(logMutex);
    std::ofstream file(logPath,std::ios::app);
    SYSTEMTIME t{}; GetLocalTime(&t);
    file<<t.wHour<<":"<<t.wMinute<<":"<<t.wSecond<<" "<<value<<"\n";
}
template<class T> T& field(void* object,size_t offset) {return *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(object)+offset);}
template<class R,class... A> R call(void* object,size_t slot,A... args) {
    return reinterpret_cast<R(*)(void*,A...)>((*reinterpret_cast<void***>(object))[slot])(object,args...);
}
struct World {void* client{};void* player{};void* region{};void* dimension{};Vec3 pos{};bool captured{};};
World worldUnsafe() {
    World w;
    auto main=*reinterpret_cast<void**>(platformGlobal); if(!main)return w;
    auto platform=field<void*>(main,8); if(!platform)return w;
    auto app=field<void*>(platform,0x20); if(!app)return w;
    auto game=field<void*>(app,0x48); if(!game)return w;
    // 26.50 map values contain an interface pointer before the owning client
    // pointer. Do not reinterpret the map as std::map<uint8_t, shared_ptr<void>>.
    auto head=field<void*>(game,0x970);if(!head)return w;
    auto first=field<void*>(head,0);if(!first||first==head)return w;
    if(field<uint8_t>(head,0x19)!=1||field<uint8_t>(first,0x19)!=0)return {};
    if(field<uint8_t>(first,0x20)!=0)return w;
    w.client=field<void*>(first,0x30); if(!w.client)return w;
    if(field<void*>(w.client,0x1A8)!=game)return {};
    w.captured=field<bool>(game,0x1E8);
    w.player=call<void*>(w.client,0x1F); if(!w.player)return w;
    auto state=field<void*>(w.player,0x218); if(!state)return w;
    w.pos=field<Vec3>(state,0);
    w.region=call<void*>(w.client,0x1E);
    w.dimension=field<void*>(w.player,0x1C8);
    return w;
}
bool worldSafe(World* out) {
    __try {*out=worldUnsafe();return true;}
    __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
}
bool cursorSafe(void* client,bool grab) {
    __try {reinterpret_cast<void(*)(void*)>(grab?grabCursor:releaseCursor)(client);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void grabCursorHook(void* client) {
    // Gameplay keeps requesting capture while no Minecraft screen is open.
    // Keep the cursor released for the entire lifetime of our own panel.
    if(overlay::isOpen()){++blockedCursorGrabs;return;}
    originalGrabCursor(client);
}
void releaseMouseHook(void* platform) {
    originalReleaseMouse(platform);
    if(overlay::isOpen())SetCursor(LoadCursorW(nullptr,IDC_ARROW));
}
void setMenu(bool open,bool resume) {
    World w{};if(!worldSafe(&w))return;
    if(open) {
        if(!w.client||!w.player||!w.captured)return;
        menuWasCaptured=w.captured;zoomHeld=false;blockedCursorGrabs=0;
        overlay::setOpen(true);
        if(!cursorSafe(w.client,false)){overlay::setOpen(false);log("MENU_ERROR cursor release failed");return;}
        SetCursor(LoadCursorW(nullptr,IDC_ARROW));
        if(gameWindow)for(int key:std::initializer_list<int>{'W','A','S','D',VK_SPACE,VK_SHIFT,VK_CONTROL})
            originalWindow(gameWindow,WM_KEYUP,key,LPARAM((MapVirtualKeyW(key,MAPVK_VK_TO_VSC)<<16)|0xC0000001u));
        log("MENU_OPEN");
    }else {
        mouseReleaseFence=(GetAsyncKeyState(VK_LBUTTON)&0x8000)||(GetAsyncKeyState(VK_RBUTTON)&0x8000)||(GetAsyncKeyState(VK_MBUTTON)&0x8000);
        overlay::setOpen(false);zoomHeld=false;
        if(resume&&menuWasCaptured&&w.client&&w.player)cursorSafe(w.client,true);
        menuWasCaptured=false;log("MENU_CLOSE blocked_cursor_grabs="+std::to_string(blockedCursorGrabs.load()));
        if(settings)PostMessageW(settings,WM_APP+3,0,0);
    }
}
void consumeMouseUnsafe(){
    auto mouse=reinterpret_cast<void*>(mouseGlobal);
    lumen::consumeMouseButtons(field<std::vector<lumen::MouseAction>>(mouse,0x18));
    for(int i=0;i<7;i++)field<bool>(mouse,16+i)=false;
}
bool consumeMouseSafe(){__try{consumeMouseUnsafe();return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
bool mouseInput(void* a,void* b,void* c){
    const auto result=originalMouse(a,b,c);
    const bool fence=mouseReleaseFence.load();
    if((overlay::isOpen()||fence)&&!consumeMouseSafe()){closeRequested=true;}
    if(fence&&!(GetAsyncKeyState(VK_LBUTTON)&0x8000)&&!(GetAsyncKeyState(VK_RBUTTON)&0x8000)&&!(GetAsyncKeyState(VK_MBUTTON)&0x8000))mouseReleaseFence=false;
    return result;
}
int blockUnsafe(void* region,Pos p) {
    // Explicit reference type is essential: slot 2 takes BlockPos const&.
    auto block=call<void*,const Pos&>(region,2,p);
    if(!block)return -1;
    auto legacy=field<void*>(block,0x68); if(!legacy)return -1;
    const auto& name=field<std::string>(legacy,0xE8);
    if(name.size()>96)return -1;
    return classify(std::string_view(name.data(),name.size()));
}
int blockSafe(void* region,Pos p) {
    __try {return blockUnsafe(region,p);}
    __except(EXCEPTION_EXECUTE_HANDLER) {return -2;}
}
void fail(const char* reason) {
    enabled=false; fault=true;
    if(settings)PostMessageW(settings,WM_APP+2,0,0);
    log(std::string("ERROR: ")+reason);
}
void clearScan() {
    std::lock_guard lock(scanMutex);
    scanner.clear(); snapshot.clear(); oldRegion=oldDimension=nullptr;
    boxCount=0; readCount=0; sweepCount=0;
}
void tick(void* level) {
    originalTick(level);
    if(closeRequested.exchange(false))setMenu(false);
    try {
        World w{};
        if(!worldSafe(&w)){if(!fault.exchange(true))fail("World access failed; Xray stopped.");return;}
        if(!w.player || !w.region || !w.dimension){clearScan();return;}
        if(overlay::isOpen()&&w.captured)cursorSafe(w.client,false);
        if(!enabled || fault){clearScan();return;}
        if(!std::isfinite(w.pos.x)||!std::isfinite(w.pos.y)||!std::isfinite(w.pos.z)||
           std::abs(w.pos.x)>30000000 || std::abs(w.pos.z)>30000000)return;
        Pos center{int(std::floor(w.pos.x)),int(std::floor(w.pos.y)),int(std::floor(w.pos.z))};
        std::lock_guard lock(scanMutex);
        const auto rev=revision.load();
        if(oldRegion!=w.region || oldDimension!=w.dimension || previousRevision!=rev) {
            scanner.clear();snapshot.clear();oldRegion=w.region;oldDimension=w.dimension;previousRevision=rev;
        }
        scanner.tick(center,radius.load(),oreMask.load(),800,[&](Pos p){
            const auto ore=blockSafe(w.region,p);
            if(ore==-2)throw std::runtime_error("Block access failed; Xray stopped.");
            return ore;
        });
        snapshot=scanner.visible(center,maxBoxes.load());
        readCount=scanner.reads;sweepCount=scanner.sweeps;boxCount=snapshot.size();
        static bool first=false;
        if(!first && scanner.sweeps>0){first=true;log("SCAN_OK reads="+std::to_string(scanner.reads)+" ores="+std::to_string(snapshot.size()));}
    } catch(const std::exception& e){fail(e.what());}
}
struct HashedString {
    uint64_t hash{14695981039346656037ULL};std::string text;void* last{};
    explicit HashedString(const char* name):text(name){for(unsigned char c:text){hash*=1099511628211ULL;hash^=c;}}
};
std::shared_ptr<void> uiMaterial, selectionMaterial;
void createMaterials() {
    if(!uiMaterial){HashedString name("ui_fill_color");call<void,std::shared_ptr<void>&,const HashedString&>(reinterpret_cast<void*>(materialGroup),1,uiMaterial,name);}
    if(!selectionMaterial){HashedString name("selection_box");call<void,std::shared_ptr<void>&,const HashedString&>(reinterpret_cast<void*>(materialGroup),1,selectionMaterial,name);}
}
bool createMaterialsSafe() {
    __try {createMaterials();return bool(uiMaterial)&&bool(selectionMaterial);}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void drawUnsafe(void* levelRenderer,void* screen,const Hit* hits,size_t count,bool walls) {
    auto player=field<void*>(levelRenderer,0x468); if(!player)return;
    const Vec3 origin=field<Vec3>(player,0x660);
    auto tess=field<void*>(screen,0xB8);
    auto shader=field<Color*>(screen,0x30); if(!tess||!shader)return;
    const Color prior=*shader;*shader={1,1,1,1};
    reinterpret_cast<void(*)(void*,int,int,int,bool)>(tessBegin)(tess,0,4,int(count*24),false);
    constexpr int edges[12][2]={{0,1},{0,2},{0,4},{1,3},{1,5},{2,3},{2,6},{3,7},{4,5},{4,6},{5,7},{6,7}};
    for(size_t i=0;i<count;++i) {
        const auto& h=hits[i];
        reinterpret_cast<void(*)(void*,const Color&)>(tessColor)(tess,colors[h.ore]);
        for(const auto& edge:edges)for(int v:edge)
            reinterpret_cast<void(*)(void*,float,float,float)>(tessVertex)(tess,
                float(h.pos.x+(v&1))-origin.x,float(h.pos.y+((v>>1)&1))-origin.y,float(h.pos.z+((v>>2)&1))-origin.z);
    }
    char padding[0x58]{};
    reinterpret_cast<void(*)(void*,void*,void*,char*)>(meshRender)(screen,tess,walls?uiMaterial.get():selectionMaterial.get(),padding);
    *shader=prior;
}
bool drawSafe(void* level,void* screen,const Hit* hits,size_t count,bool walls) {
    __try {drawUnsafe(level,screen,hits,count,walls);return true;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
// Latite applies projection zoom after renderLevel. Track the last write so
// unchanged camera values cannot multiply again on successive calls.
bool updateZoomSafe(void* level,bool active) {
    static void* priorPlayer{};
    static float originalX{},originalY{},writtenX{},writtenY{};
    static bool owns=false;
    __try {
        auto player=field<void*>(level,0x468);if(!player)return true;
        auto& x=field<float>(player,0xF58);auto& y=field<float>(player,0xF6C);
        if(owns && player==priorPlayer)lumen::restoreProjection(x,y,originalX,originalY,writtenX,writtenY);
        owns=false;
        if(!active)return true;
        if(!lumen::validProjection(x,y))return false;
        originalX=x;originalY=y;const auto factor=lumen::zoomScale(zoomFactor);
        writtenX=x*factor;writtenY=y*factor;priorPlayer=player;
        x=writtenX;y=writtenY;owns=true;++zoomFrames;return true;
    } __except(EXCEPTION_EXECUTE_HANDLER){owns=false;return false;}
}
void render(void* level,void* screen,void* unknown) {
    originalRender(level,screen,unknown);
    World camera{};
    const bool playing=worldSafe(&camera)&&camera.player&&camera.captured&&!overlay::isOpen();
    if(!playing)zoomHeld=false;
    if(!updateZoomSafe(level,playing&&zoomEnabled&&zoomHeld&&!viewFault)) {
        if(!viewFault.exchange(true))log("ZOOM_ERROR: invalid camera access; zoom stopped");
        zoomHeld=false;
    }
    if(!enabled || fault)return;
    try {
        World w{};if(!worldSafe(&w)||!w.player||!w.region)return;
        std::vector<Hit> hits;
        {std::lock_guard lock(scanMutex);if(oldRegion!=w.region||oldDimension!=w.dimension)return;hits=snapshot;}
        if(hits.empty())return;
        if(!createMaterialsSafe()||!drawSafe(level,screen,hits.data(),hits.size(),through))fail("Rendering failed; Xray stopped.");
        else if(++frameCount==1)log("RENDER_OK boxes="+std::to_string(hits.size()));
    }catch(const std::exception& e){fail(e.what());}
}
void* leave(void* level){overlay::setOpen(false);menuWasCaptured=false;zoomHeld=false;clearScan();log("LEAVE_OK");return originalLeave(level);}
LRESULT gameProc(HWND hwnd,UINT msg,WPARAM key,LPARAM data) {
    gameWindow=hwnd;
    if(msg==WM_KILLFOCUS || (msg==WM_ACTIVATEAPP&&!key)){zoomHeld=false;if(overlay::isOpen())setMenu(false,false);}
    static bool escapeConsumed=false;
    if((msg==WM_KEYDOWN||msg==WM_KEYUP)&&key==VK_ESCAPE){
        if(msg==WM_KEYDOWN&&escapeConsumed)return 0;
        if(msg==WM_KEYDOWN&&overlay::isOpen()){escapeConsumed=true;setMenu(false);return 0;}
        if(msg==WM_KEYUP&&escapeConsumed){escapeConsumed=false;return 0;}
    }
    if((msg==WM_KEYDOWN||msg==WM_KEYUP)&&key==VK_INSERT){
        if(msg==WM_KEYDOWN&&!(data&(1LL<<30)))setMenu(!overlay::isOpen());return 0;
    }
    if(overlay::input(hwnd,msg,key,data))return msg==WM_SETCURSOR?TRUE:0;
    if((msg==WM_KEYDOWN||msg==WM_KEYUP)&&key=='C') {
        if(msg==WM_KEYUP)zoomHeld=false;
        World w{};
        if(zoomEnabled&&!viewFault&&worldSafe(&w)&&w.player&&w.captured&&!overlay::isOpen()) {
            zoomHeld=msg==WM_KEYDOWN;
            if(!(data&(1LL<<30))||msg==WM_KEYUP)log(zoomHeld?"ZOOM_HOLD":"ZOOM_RELEASE");
            return 0;
        }
    }
    if((msg==WM_KEYDOWN||msg==WM_KEYUP)&&key=='B') {
        World w{};
        if(worldSafe(&w)&&w.player&&w.captured&&!overlay::isOpen()) {
            if(msg==WM_KEYDOWN&&!(data&(1LL<<30))){fullbright=!fullbright;log(fullbright?"FULLBRIGHT_ON":"FULLBRIGHT_OFF");PostMessageW(settings,WM_APP+3,0,0);}
            return 0;
        }
    }
    if((msg==WM_KEYDOWN||msg==WM_KEYUP)&&key=='X' && !fault) {
        World w{};
        if(worldSafe(&w)&&w.captured) {
            if(msg==WM_KEYDOWN&&!(data&(1LL<<30))){enabled=!enabled;revision++;log(enabled?"XRAY_ON":"XRAY_OFF");}
            return 0;
        }
    }
    return originalWindow(hwnd,msg,key,data);
}
uintptr_t signature(const char* pattern,int relativeOffset=0) {
    std::istringstream input(pattern);std::string token;std::vector<int> bytes;
    while(input>>token)bytes.push_back(token=="?"?-1:std::stoi(token,nullptr,16));
    auto base=reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
    auto nt=reinterpret_cast<IMAGE_NT_HEADERS*>(base+reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    uintptr_t result=0;int matches=0;
    auto sections=IMAGE_FIRST_SECTION(nt);
    for(unsigned s=0;s<nt->FileHeader.NumberOfSections;++s) {
        if(!(sections[s].Characteristics&IMAGE_SCN_MEM_EXECUTE))continue;
        auto begin=base+sections[s].VirtualAddress;const size_t size=sections[s].Misc.VirtualSize;
        for(size_t i=0;i+bytes.size()<=size;++i) {
            if(begin[i]!=bytes[0])continue;
            bool equal=true;for(size_t j=1;j<bytes.size();++j)if(bytes[j]>=0&&begin[i+j]!=bytes[j]){equal=false;break;}
            if(equal){result=reinterpret_cast<uintptr_t>(begin+i);++matches;}
        }
    }
    if(matches!=1)throw std::runtime_error("Signature not unique: "+std::string(pattern).substr(0,25)+" count="+std::to_string(matches));
    if(relativeOffset)result=result+relativeOffset+4+*reinterpret_cast<int32_t*>(result+relativeOffset);
    return result;
}
bool correctVersion() {
    wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);
    DWORD ignored{};const auto size=GetFileVersionInfoSizeW(path,&ignored);if(!size)return false;
    std::vector<uint8_t> data(size);if(!GetFileVersionInfoW(path,0,size,data.data()))return false;
    VS_FIXEDFILEINFO* info{};UINT length{};
    if(!VerQueryValueW(data.data(),L"\\",reinterpret_cast<void**>(&info),&length))return false;
    if(length<sizeof(VS_FIXEDFILEINFO)||info->dwSignature!=VS_FFI_SIGNATURE)return false;
    log("Minecraft version "+std::to_string(HIWORD(info->dwFileVersionMS))+"."+std::to_string(LOWORD(info->dwFileVersionMS))+"."+std::to_string(HIWORD(info->dwFileVersionLS))+"."+std::to_string(LOWORD(info->dwFileVersionLS)));
    return lumen::supportedBedrock(HIWORD(info->dwFileVersionMS),LOWORD(info->dwFileVersionMS),HIWORD(info->dwFileVersionLS),LOWORD(info->dwFileVersionLS));
}
void hook(uintptr_t address,void* target,void** original) {
    const auto result=MH_CreateHook(reinterpret_cast<void*>(address),target,original);
    if(result!=MH_OK)throw std::runtime_error(MH_StatusToString(result));
}
void initializeHooks() {
    if(GetModuleHandleW(L"XrayLight.dll"))throw std::runtime_error("Xray Light is still loaded. Restart Minecraft before loading Lumen.");
    if(GetModuleHandleW(L"Latite.dll")||GetModuleHandleW(L"LatiteNightly.dll")||GetModuleHandleW(L"LatiteDebug.dll"))throw std::runtime_error("Latite is already loaded. Restart Minecraft without Latite.");
    if(!correctVersion())throw std::runtime_error("This Lumen build requires Minecraft Bedrock 26.51 (Windows package 1.26.5101.0).");
    platformGlobal=signature("4C 89 3D ? ? ? ? 4D 85 FF",3);
    materialGroup=signature("48 8D 15 ? ? ? ? 4C 8D 45 ? E8 ? ? ? ? 48 8D 4D ? E8 ? ? ? ? 48 8D 0D ? ? ? ? E8 ? ? ? ? 48 8D 0D ? ? ? ? E8 ? ? ? ? E9 ? ? ? ? 48 8D 0D",3);
    tessBegin=signature("56 57 55 53 48 83 EC ? 48 8B 05 ? ? ? ? 48 31 E0 48 89 44 24 ? 80 B9 ? ? ? ? ? 0F 85 ? ? ? ? 80 B9");
    tessColor=signature("80 B9 ? ? ? ? ? 0F 85 ? ? ? ? F3 0F 10 05 ? ? ? ? F3 0F 10 0A");
    tessVertex=signature("55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC ? ? ? ? 48 8D AC 24 ? ? ? ? 44 0F 29 4D ? 44 0F 29 45 ? 0F 29 7D ? 0F 29 75 ? 48 C7 45 ? ? ? ? ? 0F 28 F3 0F 28 FA 44 0F 28 C1 48 89 CE 48 8B 0D");
    meshRender=signature("55 41 57 41 56 41 54 56 57 53 48 81 EC ? ? ? ? 48 8D AC 24 ? ? ? ? 48 C7 85 ? ? ? ? ? ? ? ? 80 BA ? ? ? ? ? 0F 85 ? ? ? ? 4C 89 CF");
    const auto tickAddress=signature("55 56 57 53 48 81 EC ? ? ? ? 48 8D AC 24 ? ? ? ? 44 0F 29 6D 40 44 0F 29 65 30 44 0F 29 5D 20");
    const auto renderAddress=signature("E8 ? ? ? ? 45 31 E4 48 83 BE",1);
    const auto windowAddress=signature("55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC ? ? ? ? 48 8D AC 24 ? ? ? ? 48 C7 85 ? ? ? ? ? ? ? ? 89 D6 4C 8B 3D");
    const auto levelTable=signature("48 8D 05 ? ? ? ? 48 89 07 48 8D 05 ? ? ? ? 48 89 47 ? 48 8D 05 ? ? ? ? 48 89 BD",3);
    const auto gammaAddress=signature("48 83 EC 38 48 8B 05 ? ? ? ? 48 31 E0 48 89 44 24 ? 48 8B 01 48 8B 40 08 48 8D 54 24 ? 41 B8 32 00 00 00");
    grabCursor=signature("56 48 83 EC ? 48 89 CE 48 8B 01 48 8B 80 ? ? ? ? FF 15 ? ? ? ? 84 C0 74 ? 48 8B 8E ? ? ? ? 48 8B 01 48 8B 80 ? ? ? ? 48 8B 15 ? ? ? ? 48 83 C4 ? 5E 48 FF E2 90 48 83 C4 ? 5E C3 CC CC CC CC CC CC CC CC CC CC CC CC CC 56 48 83 EC");
    releaseCursor=signature("56 48 83 EC ? 48 89 CE 48 8B 01 48 8B 80 ? ? ? ? FF 15 ? ? ? ? 84 C0 74 ? 48 8B 8E ? ? ? ? 48 8B 01 48 8B 80 ? ? ? ? 48 8B 15 ? ? ? ? 48 83 C4 ? 5E 48 FF E2 90 48 83 C4 ? 5E C3 CC CC CC CC CC CC CC CC CC CC CC CC CC 56 53");
    mouseGlobal=signature("89 15 ? ? ? ? C7 47",2);
    const auto mouseAddress=signature("55 41 57 41 56 41 55 41 54 56 57 53 48 81 EC ? ? ? ? 48 8D AC 24 ? ? ? ? 44 0F 29 BD ? ? ? ? 44 0F 29 B5 ? ? ? ? 44 0F 29 AD ? ? ? ? 44 0F 29 A5 ? ? ? ? 44 0F 29 9D ? ? ? ? 44 0F 29 95 ? ? ? ? 44 0F 29 8D ? ? ? ? 44 0F 29 85 ? ? ? ? 0F 29 BD ? ? ? ? 0F 29 B5 ? ? ? ? 48 C7 85 ? ? ? ? ? ? ? ? 48 89 CE 8B 05");
    const auto releaseMouseAddress=signature("56 57 48 83 EC ? 48 89 CE B9 ? ? ? ? FF 15");
    if(MH_Initialize()!=MH_OK)throw std::runtime_error("MinHook initialization failed");
    try {
        hook(tickAddress,reinterpret_cast<void*>(tick),reinterpret_cast<void**>(&originalTick));
        hook(renderAddress,reinterpret_cast<void*>(render),reinterpret_cast<void**>(&originalRender));
        hook(windowAddress,reinterpret_cast<void*>(gameProc),reinterpret_cast<void**>(&originalWindow));
        hook(reinterpret_cast<uintptr_t*>(levelTable)[2],reinterpret_cast<void*>(leave),reinterpret_cast<void**>(&originalLeave));
        hook(gammaAddress,reinterpret_cast<void*>(gamma),reinterpret_cast<void**>(&originalGamma));
        hook(mouseAddress,reinterpret_cast<void*>(mouseInput),reinterpret_cast<void**>(&originalMouse));
        hook(grabCursor,reinterpret_cast<void*>(grabCursorHook),reinterpret_cast<void**>(&originalGrabCursor));
        hook(releaseMouseAddress,reinterpret_cast<void*>(releaseMouseHook),reinterpret_cast<void**>(&originalReleaseMouse));
        overlay::initialize(dll,uiState,uiChange,log);
        if(MH_EnableHook(MH_ALL_HOOKS)!=MH_OK)throw std::runtime_error("Hook activation failed");
    } catch(...) {MH_DisableHook(MH_ALL_HOOKS);MH_Uninitialize();throw;}
    ready=true;log("READY Lumen " LUMEN_VERSION "; in-game overlay; cursor capture fix; Xray/Zoom/Fullbright; no OP requirement; no Latite/Chakra");
}
void saveSettings() {
    auto save=[](const wchar_t* key,int value){WritePrivateProfileStringW(L"Xray",key,std::to_wstring(value).c_str(),configPath.c_str());};
    save(L"Radius",radius);save(L"MaxBoxes",maxBoxes);save(L"Ores",oreMask);save(L"Through",through);
    save(L"Fullbright",fullbright);save(L"ZoomEnabled",zoomEnabled);save(L"ZoomFactor",zoomFactor);
}
overlay::State uiState(){
    overlay::State state;state.xray=enabled;state.walls=through;state.zoom=zoomEnabled;state.brightness=fullbright;
    state.radius=radius;state.limit=maxBoxes;state.zoomFactor=zoomFactor;state.ores=oreMask;
    if(fault)state.status=L"Xray stopped. See lumen.log for details.";
    else if(viewFault)state.status=L"Zoom stopped. See lumen.log for details.";
    else if(!enabled)state.status=L"Xray is off. Press X or use the toggle to enable it.";
    else state.status=std::to_wstring(boxCount.load())+L" ores marked · "+std::to_wstring(readCount.load())+L" queries";
    return state;
}
void uiChange(overlay::Command command,int value){
    switch(command){
    case overlay::Xray:if(!fault){enabled=value!=0;revision++;log(enabled?"XRAY_ON":"XRAY_OFF");}break;
    case overlay::Walls:through=value!=0;break;
    case overlay::Radius:radius=std::clamp(value,4,24);revision++;break;
    case overlay::Limit:maxBoxes=std::clamp(value,32,512);revision++;break;
    case overlay::Zoom:zoomEnabled=value!=0;zoomHeld=false;break;
    case overlay::Brightness:fullbright=value!=0;log(fullbright?"FULLBRIGHT_ON":"FULLBRIGHT_OFF");break;
    case overlay::ZoomFactor:zoomFactor=std::clamp(value,2,20);break;
    case overlay::Ore:if(value>=0&&value<10){oreMask.fetch_xor(1u<<value);revision++;}break;
    case overlay::Close:closeRequested=true;break;
    }
    if(settings)PostMessageW(settings,WM_APP+3,0,0);
}
LRESULT CALLBACK workerProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    if(msg==WM_APP+3){saveSettings();return 0;}
    if(msg==WM_TIMER){
        static bool zoomLogged=false,gammaLogged=false;
        if(!zoomLogged&&zoomFrames>0){zoomLogged=true;log("ZOOM_RENDER_OK");}
        if(!gammaLogged&&gammaOverrides>0){gammaLogged=true;log("FULLBRIGHT_GAMMA_OK");}
        return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
void createWorkerWindow(){
    WNDCLASSW wc{};wc.lpfnWndProc=workerProc;wc.hInstance=dll;wc.lpszClassName=L"Lumen.Worker";
    RegisterClassW(&wc);settings=CreateWindowExW(0,wc.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,dll,nullptr);
    if(!settings)throw std::runtime_error("Settings worker could not be created");
    SetTimer(settings,1,500,nullptr);
}
BOOL CALLBACK findGame(HWND hwnd,LPARAM) {DWORD pid{};GetWindowThreadProcessId(hwnd,&pid);if(pid==GetCurrentProcessId()&&IsWindowVisible(hwnd)&&hwnd!=settings){wchar_t title[128]{};GetWindowTextW(hwnd,title,128);if(wcsstr(title,L"Minecraft")){gameWindow=hwnd;return FALSE;}}return TRUE;}
DWORD WINAPI worker(void*) {
    wchar_t local[32768]{};GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
    const std::wstring folder=std::wstring(local)+L"\\Lumen";CreateDirectoryW(folder.c_str(),nullptr);
    configPath=folder+L"\\settings.ini";logPath=folder+L"\\lumen.log";
    // One-time import keeps the user's working Xray filters and radius.
    const auto oldConfig=std::wstring(local)+L"\\XrayLight\\settings.ini";
    if(GetFileAttributesW(configPath.c_str())==INVALID_FILE_ATTRIBUTES)CopyFileW(oldConfig.c_str(),configPath.c_str(),TRUE);
    log("START pid="+std::to_string(GetCurrentProcessId()));
    radius=std::clamp(int(GetPrivateProfileIntW(L"Xray",L"Radius",16,configPath.c_str())),4,24);
    maxBoxes=std::clamp(int(GetPrivateProfileIntW(L"Xray",L"MaxBoxes",256,configPath.c_str())),32,512);
    oreMask=GetPrivateProfileIntW(L"Xray",L"Ores",defaultMask,configPath.c_str())&1023;
    through=GetPrivateProfileIntW(L"Xray",L"Through",1,configPath.c_str())!=0;
    fullbright=GetPrivateProfileIntW(L"Xray",L"Fullbright",0,configPath.c_str())!=0;
    zoomEnabled=GetPrivateProfileIntW(L"Xray",L"ZoomEnabled",1,configPath.c_str())!=0;
    zoomFactor=std::clamp(int(GetPrivateProfileIntW(L"Xray",L"ZoomFactor",4,configPath.c_str())),2,20);
    try {initializeHooks();EnumWindows(findGame,0);createWorkerWindow();}
    catch(const std::exception& e){fail(e.what());MessageBoxA(nullptr,e.what(),"Lumen",MB_OK|MB_ICONERROR);return 0;}
    MSG message{};
    while(GetMessageW(&message,nullptr,0,0)>0) {
        TranslateMessage(&message);DispatchMessageW(&message);
    }
    // DLL intentionally lives until process exit; no unsafe live FreeLibrary.
    return 0;
}
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,void*) {
    if(reason==DLL_PROCESS_ATTACH){dll=instance;DisableThreadLibraryCalls(instance);if(auto thread=CreateThread(nullptr,0,worker,nullptr,0,nullptr))CloseHandle(thread);}
    return TRUE;
}
