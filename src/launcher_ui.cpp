// SPDX-License-Identifier: GPL-3.0-only
// Lumen launcher design: Figma Lfm0C6WFqEGv6z6XLKszSW, node 11:17.
#include "launcher_ui.h"
#include "version.h"
#include "resource.h"
#include <windowsx.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <d2d1.h>
#include <dwrite_3.h>
#include <shellapi.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>
#include <memory>

using Microsoft::WRL::ComPtr;
namespace launcher_ui { namespace {
constexpr unsigned ink=0x050505,panel=0x101010,line=0x292929,paper=0xf5f3ea,muted=0x9b9b92,signal=0xffce00;
constexpr int launchId=1,folderId=20,minimizeId=21,closeId=22,statusId=23;
constexpr UINT updateMessage=WM_APP+10;
HWND window{},launchButton{},folderButton{},minimizeButton{},closeButton{},statusControl{};
LPTHREAD_START_ROUTINE startWorker{};
State current=State::Ready;
std::wstring detail=L"Minecraft will open automatically if it is not running.";
bool previewMode{},waitingToClose{};
float scale=1;
ComPtr<ID2D1Factory> factory;
ComPtr<ID2D1HwndRenderTarget> canvas;
ComPtr<ID2D1DCRenderTarget> controlCanvas;
ComPtr<IDWriteFactory3> writer;
ComPtr<IDWriteFontCollection1> fonts;
std::wstring family=L"Segoe UI";
std::map<int,ComPtr<IDWriteTextFormat>> formats;
struct Update {State state;std::wstring detail;};

bool busy(){return current==State::Loading||current==State::CheckingUpdates||current==State::DownloadingUpdate;}
bool closeAction(){return current==State::Loaded||current==State::Uncertain;}
float extraHeight(){return current==State::Error||current==State::Uncertain||detail.size()>78?32.f:0.f;}
int px(float value){return int(std::lround(value*scale));}
std::filesystem::path directory(){wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);return std::filesystem::path(path).parent_path();}
const wchar_t* title(){switch(current){case State::CheckingUpdates:return L"Checking for updates";case State::DownloadingUpdate:return L"Updating Lumen";case State::Loading:return L"Loading Lumen";case State::Loaded:return L"Lumen is loaded";case State::Error:return L"Could not load Lumen";case State::Uncertain:return L"Check Minecraft";default:return L"Ready to launch";}}
const wchar_t* tag(){switch(current){case State::CheckingUpdates:case State::DownloadingUpdate:return L"AUTOMATIC UPDATES";case State::Loading:return L"LOADING";case State::Loaded:return L"LOADED";case State::Error:return L"NEEDS ATTENTION";case State::Uncertain:return L"LOAD NOT CONFIRMED";default:return L"READY";}}
const wchar_t* action(){if(closeAction())return L"Close launcher";if(current==State::CheckingUpdates)return L"Checking…";if(current==State::DownloadingUpdate)return L"Updating…";if(busy())return L"Loading…";return current==State::Error?L"Try again":L"Launch Lumen";}

void initFonts(){
    // Private DirectWrite collection; no machine-wide font registration.
    ComPtr<IDWriteFontSetBuilder> builder;
    if(FAILED(writer->CreateFontSetBuilder(&builder)))return;
    for(const auto* name:{L"Geist-Regular.ttf",L"Geist-SemiBold.ttf"}){
        ComPtr<IDWriteFontFaceReference> face;
        if(FAILED(writer->CreateFontFaceReference((directory()/L"assets"/name).c_str(),nullptr,0,DWRITE_FONT_SIMULATIONS_NONE,&face))||FAILED(builder->AddFontFaceReference(face.Get())))return;
    }
    ComPtr<IDWriteFontSet> set;ComPtr<IDWriteFontCollection1> collection;
    if(FAILED(builder->CreateFontSet(&set))||FAILED(writer->CreateFontCollectionFromFontSet(set.Get(),&collection)))return;
    ComPtr<IDWriteFontFamily> fam;ComPtr<IDWriteLocalizedStrings> names;UINT32 length{};
    if(FAILED(collection->GetFontFamily(0,&fam))||FAILED(fam->GetFamilyNames(&names))||FAILED(names->GetStringLength(0,&length)))return;
    std::wstring value(length+1,L'\0');if(FAILED(names->GetString(0,value.data(),length+1)))return;
    value.resize(length);family=std::move(value);fonts=collection;
}

struct Paint {
    ID2D1RenderTarget* target;
    ComPtr<ID2D1SolidColorBrush> brush;
    explicit Paint(ID2D1RenderTarget* value):target(value){target->CreateSolidColorBrush(D2D1::ColorF(ink),&brush);}
    void fill(float x,float y,float w,float h,unsigned rgb,float radius=0,float alpha=1){
        if(!brush)return;brush->SetColor(D2D1::ColorF(rgb,alpha));const auto rect=D2D1::RectF(x,y,x+w,y+h);
        if(radius)target->FillRoundedRectangle(D2D1::RoundedRect(rect,radius,radius),brush.Get());else target->FillRectangle(rect,brush.Get());
    }
    void border(float x,float y,float w,float h,unsigned rgb,float radius=0){
        if(!brush)return;brush->SetColor(D2D1::ColorF(rgb));target->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x,y,x+w,y+h),radius,radius),brush.Get());
    }
    void text(float x,float y,float w,float h,const std::wstring& value,float size=14,unsigned rgb=paper,bool bold=false,DWRITE_TEXT_ALIGNMENT align=DWRITE_TEXT_ALIGNMENT_LEADING,bool center=false){
        if(!brush)return;const int key=int(size)*2+int(bold);auto& format=formats[key];
        if(!format&&FAILED(writer->CreateTextFormat(family.c_str(),fonts.Get(),bold?DWRITE_FONT_WEIGHT_SEMI_BOLD:DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"en-US",&format)))return;
        format->SetTextAlignment(align);format->SetParagraphAlignment(center?DWRITE_PARAGRAPH_ALIGNMENT_CENTER:DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
        format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);format->SetLineSpacing(DWRITE_LINE_SPACING_METHOD_UNIFORM,size+6,size);
        brush->SetColor(D2D1::ColorF(rgb));target->DrawTextW(value.c_str(),UINT32(value.size()),format.Get(),D2D1::RectF(x,y,x+w,y+h),brush.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
};

void drawWindow(){
    PAINTSTRUCT ps{};BeginPaint(window,&ps);
    if(!canvas){RECT r{};GetClientRect(window,&r);factory->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),D2D1::HwndRenderTargetProperties(window,D2D1::SizeU(r.right,r.bottom)),&canvas);}
    if(canvas){
        canvas->SetDpi(96*scale,96*scale);canvas->BeginDraw();canvas->Clear(D2D1::ColorF(ink));Paint p(canvas.Get());
        p.border(.5f,.5f,759,591+extraHeight(),line,18);
        p.text(32,27,420,32,L"LUMEN",26,paper,true);
        p.text(32,60,420,18,L"A U R O N  N E T W O R K",10,signal);
        p.text(536,47,80,20,previewMode?L"PREVIEW":L"v" LUMEN_VERSION_W,11,muted);
        p.fill(32,96,696,1,line);
        p.text(32,128,696,42,L"A clearer way to play.",32,paper,true);
        p.text(32,174,696,24,L"Xray, Zoom and Fullbright. One lightweight client.",14,muted);
        const wchar_t* names[]={L"Xray",L"Zoom",L"Fullbright"};const wchar_t* keys[]={L"X",L"Hold C",L"B"};
        const wchar_t* descriptions[]={L"Find what lies beneath.",L"A closer look, on demand.",L"See clearly in the dark."};
        for(int i=0;i<3;++i){const float x=32+i*(221.3333f+16);const float badge=i==1?50.f:24.f;
            p.fill(x,224,221.3333f,88,panel,12);p.border(x+.5f,224.5f,220.3333f,87,line,12);
            p.text(x+16,240,142,26,names[i],16,paper,true);p.fill(x+205.3333f-badge,242,badge,23,line,5);
            p.text(x+205.3333f-badge,242,badge,23,keys[i],11,paper,false,DWRITE_TEXT_ALIGNMENT_CENTER,true);
            p.text(x+16,274,190,22,descriptions[i],12,muted);
        }
        p.text(32,539+extraHeight(),440,22,waitingToClose?L"Please wait for loading to finish.":L"Insert opens your in-game menu.",12,waitingToClose?signal:muted);
        if(canvas->EndDraw()==D2DERR_RECREATE_TARGET)canvas.Reset();
    }
    EndPaint(window,&ps);
}

void drawControl(const DRAWITEMSTRUCT& item){
    if(!controlCanvas){auto props=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_IGNORE));factory->CreateDCRenderTarget(&props,&controlCanvas);}
    if(!controlCanvas||FAILED(controlCanvas->BindDC(item.hDC,&item.rcItem)))return;
    controlCanvas->SetDpi(96*scale,96*scale);controlCanvas->BeginDraw();controlCanvas->Clear(D2D1::ColorF(ink));Paint p(controlCanvas.Get());
    const float w=float(item.rcItem.right-item.rcItem.left)/scale,h=float(item.rcItem.bottom-item.rcItem.top)/scale;
    POINT mouse{};GetCursorPos(&mouse);ScreenToClient(item.hwndItem,&mouse);RECT r{};GetClientRect(item.hwndItem,&r);
    const bool hover=PtInRect(&r,mouse)!=FALSE,enabled=(item.itemState&ODS_DISABLED)==0,pressed=(item.itemState&ODS_SELECTED)!=0;
    if(item.CtlID==statusId){
        p.fill(0,0,w,h,panel,12);p.border(.5f,.5f,w-1,h-1,line,12);
        p.text(20,14,285,18,tag(),11,signal);
        p.text(330,14,w-350,18,L"MINECRAFT BEDROCK 26.51",10,muted,false,DWRITE_TEXT_ALIGNMENT_TRAILING);
        p.text(20,35,w-40,26,title(),18,paper,true);p.text(20,63,w-40,h-68,detail,13,muted);
        if(busy()){const float progress=float(GetTickCount64()%1800)/1800.f;p.fill(20,h-3,w-40,2,line);p.fill(20+(w-140)*progress,h-3,100,2,signal);}
    }else if(item.CtlID==launchId){
        p.fill(0,0,w,h,enabled?(pressed?0xd4ac00:hover?0xffdc4d:signal):0x806700,10);
        p.text(8,0,w-16,h,action(),16,ink,true,DWRITE_TEXT_ALIGNMENT_CENTER,true);
    }else{
        if(hover&&enabled)p.fill(0,0,w,h,line,8);
        const wchar_t* label=item.CtlID==folderId?L"Open Lumen folder":item.CtlID==minimizeId?L"−":L"×";
        p.text(0,0,w,h,label,item.CtlID==folderId?12.f:24.f,enabled?paper:muted,false,DWRITE_TEXT_ALIGNMENT_CENTER,true);
    }
    if((item.itemState&ODS_FOCUS)&&enabled)p.border(3,3,w-6,h-6,item.CtlID==launchId?ink:signal,7);
    if(controlCanvas->EndDraw()==D2DERR_RECREATE_TARGET)controlCanvas.Reset();
}

void layout(bool resizeWindow){
    const auto monitor=MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST);MONITORINFO info{sizeof(info)};GetMonitorInfoW(monitor,&info);
    scale=std::min({float(GetDpiForWindow(window))/96.f,float(info.rcWork.right-info.rcWork.left-16)/760.f,float(info.rcWork.bottom-info.rcWork.top-16)/(592+extraHeight())});
    if(resizeWindow){RECT r{};GetWindowRect(window,&r);const int w=px(760),h=px(592+extraHeight());
        SetWindowPos(window,nullptr,std::clamp(r.left,info.rcWork.left,std::max(info.rcWork.left,info.rcWork.right-w)),std::clamp(r.top,info.rcWork.top,std::max(info.rcWork.top,info.rcWork.bottom-h)),w,h,SWP_NOZORDER|SWP_NOACTIVATE);
    }
    auto move=[](HWND control,float x,float y,float w,float h){MoveWindow(control,px(x),px(y),px(w),px(h),TRUE);};
    move(launchButton,32,456+extraHeight(),696,52);move(folderButton,578,528+extraHeight(),150,40);
    move(minimizeButton,624,36,40,40);move(closeButton,704,36,40,40);move(statusControl,32,336,696,96+extraHeight());
    InvalidateRect(window,nullptr,FALSE);
}
void apply(State state,std::wstring value){
    if(state==State::InstallerStarted){DestroyWindow(window);return;}
    current=state;detail=std::move(value);waitingToClose=false;
    EnableWindow(launchButton,!busy());EnableWindow(closeButton,current!=State::Loading);SetWindowTextW(launchButton,action());
    SetWindowTextW(statusControl,(std::wstring(title())+L". "+detail).c_str());
    if(busy())SetTimer(window,1,40,nullptr);else KillTimer(window,1);
    layout(true);InvalidateRect(statusControl,nullptr,FALSE);InvalidateRect(launchButton,nullptr,FALSE);
}

LRESULT CALLBACK controlProc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR){
    if(message==WM_MOUSEMOVE){TRACKMOUSEEVENT event{sizeof(event),TME_LEAVE,hwnd,0};TrackMouseEvent(&event);InvalidateRect(hwnd,nullptr,FALSE);}
    if(message==WM_MOUSELEAVE||message==WM_SETFOCUS||message==WM_KILLFOCUS||message==WM_ENABLE)InvalidateRect(hwnd,nullptr,FALSE);
    if(message==WM_NCDESTROY)RemoveWindowSubclass(hwnd,controlProc,1);
    return DefSubclassProc(hwnd,message,wp,lp);
}
LRESULT CALLBACK proc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp){
    switch(message){
    case WM_NCCALCSIZE:if(wp)return 0;break;
    case WM_NCHITTEST:{POINT pt{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ScreenToClient(hwnd,&pt);if(pt.y<px(96)&&pt.x<px(620))return HTCAPTION;return HTCLIENT;}
    case WM_CREATE:{window=hwnd;const auto instance=reinterpret_cast<LPCREATESTRUCTW>(lp)->hInstance;
        auto button=[&](const wchar_t* caption,int id){auto result=CreateWindowW(L"BUTTON",caption,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,0,0,1,1,hwnd,reinterpret_cast<HMENU>(INT_PTR(id)),instance,nullptr);SetWindowSubclass(result,controlProc,1,0);return result;};
        launchButton=button(action(),launchId);folderButton=button(L"Open Lumen folder",folderId);minimizeButton=button(L"Minimize",minimizeId);closeButton=button(L"Close launcher",closeId);
        statusControl=CreateWindowW(L"STATIC",L"Ready to launch. Minecraft will open automatically if it is not running.",WS_CHILD|WS_VISIBLE|SS_OWNERDRAW,0,0,1,1,hwnd,reinterpret_cast<HMENU>(INT_PTR(statusId)),instance,nullptr);
        return 0;}
    case WM_PAINT:drawWindow();return 0;
    case WM_ERASEBKGND:return 1;
    case WM_DRAWITEM:drawControl(*reinterpret_cast<DRAWITEMSTRUCT*>(lp));return TRUE;
    case WM_SIZE:if(canvas&&wp!=SIZE_MINIMIZED)canvas->Resize(D2D1::SizeU(LOWORD(lp),HIWORD(lp)));return 0;
    case WM_DPICHANGED:{const auto r=reinterpret_cast<RECT*>(lp);SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);layout(true);return 0;}
    case WM_DISPLAYCHANGE:layout(true);return 0;
    case WM_TIMER:if(wp==1)InvalidateRect(statusControl,nullptr,FALSE);return 0;
    case DM_GETDEFID:return MAKELONG(launchId,DC_HASDEFID);
    case WM_COMMAND:
        switch(LOWORD(wp)){
        case launchId:
            if(busy())return 0;
            if(closeAction()){SendMessageW(hwnd,WM_CLOSE,0,0);return 0;}
            apply(State::Loading,L"Checking Minecraft and the Lumen files. Please keep this launcher open.");
            if(auto thread=CreateThread(nullptr,0,startWorker,nullptr,0,nullptr))CloseHandle(thread);else apply(State::Error,L"Could not start the loading task. Windows error "+std::to_wstring(GetLastError()));
            return 0;
        case folderId:{const auto path=directory().wstring();ShellExecuteW(hwnd,L"open",path.c_str(),nullptr,nullptr,SW_SHOWNORMAL);return 0;}
        case minimizeId:ShowWindow(hwnd,SW_MINIMIZE);return 0;
        case closeId:case IDCANCEL:SendMessageW(hwnd,WM_CLOSE,0,0);return 0;
        }break;
    case updateMessage:{std::unique_ptr<Update> update(reinterpret_cast<Update*>(lp));apply(update->state,std::move(update->detail));return 0;}
    case WM_CLOSE:if(current==State::Loading){waitingToClose=true;InvalidateRect(hwnd,nullptr,FALSE);return 0;}DestroyWindow(hwnd);return 0;
    case WM_DESTROY:KillTimer(hwnd,1);PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(hwnd,message,wp,lp);
}
} // namespace

void post(State state,std::wstring text){auto update=std::make_unique<Update>(Update{state,std::move(text)});if(PostMessageW(window,updateMessage,0,reinterpret_cast<LPARAM>(update.get())))update.release();}
int run(HINSTANCE instance,LPTHREAD_START_ROUTINE start,bool preview,LPTHREAD_START_ROUTINE startup){
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);startWorker=start;previewMode=preview;
    if(FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,factory.GetAddressOf()))||FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory3),reinterpret_cast<IUnknown**>(writer.GetAddressOf())))){
        MessageBoxW(nullptr,L"The launcher could not initialize Windows graphics.",L"Lumen",MB_OK|MB_ICONERROR);return 1;
    }
    initFonts();INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_STANDARD_CLASSES};InitCommonControlsEx(&controls);
    WNDCLASSEXW wc{sizeof(WNDCLASSEXW)};wc.lpfnWndProc=proc;wc.hInstance=instance;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.lpszClassName=L"Lumen.Launcher.Figma";wc.style=CS_DBLCLKS;
    wc.hIcon=static_cast<HICON>(LoadImageW(instance,MAKEINTRESOURCEW(IDI_LUMEN),IMAGE_ICON,GetSystemMetrics(SM_CXICON),GetSystemMetrics(SM_CYICON),LR_SHARED));
    wc.hIconSm=static_cast<HICON>(LoadImageW(instance,MAKEINTRESOURCEW(IDI_LUMEN),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_SHARED));
    RegisterClassExW(&wc);
    window=CreateWindowExW(WS_EX_APPWINDOW,wc.lpszClassName,preview?L"Lumen " LUMEN_VERSION_W L" · UI Preview":L"Lumen " LUMEN_VERSION_W L" · Launcher",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,760,592,nullptr,nullptr,instance,nullptr);
    if(!window)return 1;
    const BOOL dark=TRUE;const DWORD corner=2;DwmSetWindowAttribute(window,20,&dark,sizeof(dark));DwmSetWindowAttribute(window,33,&corner,sizeof(corner));
    const MARGINS margins{1,1,1,1};DwmExtendFrameIntoClientArea(window,&margins);layout(true);
    MONITORINFO monitor{sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor);
    SetWindowPos(window,nullptr,monitor.rcWork.left+(monitor.rcWork.right-monitor.rcWork.left-px(760))/2,monitor.rcWork.top+(monitor.rcWork.bottom-monitor.rcWork.top-px(592))/2,0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED);
    if(startup){apply(State::CheckingUpdates,L"Checking GitHub for the latest Lumen release.");if(auto t=CreateThread(nullptr,0,startup,nullptr,0,nullptr))CloseHandle(t);else apply(State::Ready,L"Could not check for updates. You can still launch Lumen.");}
    ShowWindow(window,SW_SHOW);UpdateWindow(window);SetFocus(busy()?closeButton:launchButton);
    MSG message{};while(GetMessageW(&message,nullptr,0,0)>0){if(!IsDialogMessageW(window,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}
    canvas.Reset();controlCanvas.Reset();formats.clear();fonts.Reset();writer.Reset();factory.Reset();return int(message.wParam);
}
} // namespace launcher_ui
