// SPDX-License-Identifier: GPL-3.0-only
// DirectX interop approach adapted from Latite and Microsoft's D3D11On12 sample.
#include "overlay.h"
#include <MinHook.h>
#include <d3d11.h>
#include <d3d12.h>
#include <d3d11on12.h>
#include <dxgi1_4.h>
#include <d2d1_1.h>
#include <dwrite_3.h>
#include <wrl/client.h>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>
#include <mutex>
#include <stdexcept>
#include <vector>
using Microsoft::WRL::ComPtr;
namespace overlay { namespace {
std::atomic<bool> opened{};
Read readState{};Change changeState{};Log logMessage{};
HMODULE module{};
std::recursive_mutex renderMutex;
std::mutex queueMutex;
std::vector<ComPtr<ID3D12CommandQueue>> queues;
using Present=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT);
using Resize=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT,UINT,DXGI_FORMAT,UINT);
using Resize1=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*,UINT,UINT,UINT,DXGI_FORMAT,UINT,const UINT*,IUnknown* const*);
using Execute=void(STDMETHODCALLTYPE*)(ID3D12CommandQueue*,UINT,ID3D12CommandList* const*);
Present originalPresent{};Resize originalResize{};Resize1 originalResize1{};Execute originalExecute{};
IDXGISwapChain* activeChain{};
ComPtr<IDXGISwapChain3> chain3;
ComPtr<ID3D11Device> device;
ComPtr<ID3D11DeviceContext> context;
ComPtr<ID3D11On12Device> on12;
ComPtr<ID2D1Factory1> factory;
ComPtr<ID2D1Device> d2device;
ComPtr<ID2D1DeviceContext> dc;
ComPtr<ID2D1SolidColorBrush> brush;
ComPtr<IDWriteFactory3> writeFactory;
ComPtr<IDWriteFontCollection1> fonts;
std::map<int,ComPtr<IDWriteTextFormat>> formats;
std::vector<ComPtr<ID3D11Resource>> wrapped;
std::vector<ComPtr<ID2D1Bitmap1>> targets;
bool initialized{},lastDown{};int dragging{};
std::atomic<bool> resetInput{true};
float mx{},my{};bool clicked{},down{};
std::wstring family=L"Geist";
void check(HRESULT result){if(FAILED(result))throw std::runtime_error("DirectX HRESULT "+std::to_string(unsigned(result)));}
D2D1_COLOR_F color(unsigned rgb,float a=1){return D2D1::ColorF(rgb,a);}
constexpr unsigned ink=0x050505,panel=0x101010,line=0x292929,paper=0xf5f3ea,muted=0x9b9b92,signal=0xffce00;
void fill(float x,float y,float w,float h,unsigned c,float a=1,float radius=0){
    brush->SetColor(color(c,a));const auto r=D2D1::RectF(x,y,x+w,y+h);
    if(radius)dc->FillRoundedRectangle(D2D1::RoundedRect(r,radius,radius),brush.Get());else dc->FillRectangle(r,brush.Get());
}
void border(float x,float y,float w,float h,unsigned c,float a=1,float radius=0){
    brush->SetColor(color(c,a));dc->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(x,y,x+w,y+h),radius,radius),brush.Get(),1);
}
void text(float x,float y,float w,float h,const std::wstring& s,float size=14,unsigned c=paper,bool bold=false,bool right=false){
    const int key=int(size)*2+int(bold);auto& format=formats[key];
    if(!format){check(writeFactory->CreateTextFormat(family.c_str(),fonts.Get(),bold?DWRITE_FONT_WEIGHT_SEMI_BOLD:DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"en-US",&format));format->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);}
    format->SetTextAlignment(right?DWRITE_TEXT_ALIGNMENT_TRAILING:DWRITE_TEXT_ALIGNMENT_LEADING);
    brush->SetColor(color(c));dc->DrawTextW(s.c_str(),UINT32(s.size()),format.Get(),D2D1::RectF(x,y,x+w,y+h),brush.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
}
bool hit(float x,float y,float w,float h){return mx>=x&&my>=y&&mx<x+w&&my<y+h;}
void toggle(float x,float y,bool value,Command command){
    const bool hover=hit(x-10,y-8,64,40);fill(x,y,44,24,value?signal:line,1,12);
    if(hover)border(x-2,y-2,48,28,signal,0.55f,14);
    fill(x+(value?24:4),y+4,16,16,value?ink:muted,1,8);
    if(clicked&&hover)changeState(command,!value);
}
void slider(float x,float y,float w,int value,int lo,int hi,Command command,int step=1){
    if(clicked&&hit(x-8,y-10,w+16,26))dragging=int(command);
    if(down&&dragging==int(command)){
        const float p=std::clamp((mx-x)/w,0.f,1.f);
        const int v=std::clamp(int(std::lround((lo+p*(hi-lo))/step))*step,lo,hi);
        if(v!=value)changeState(command,v);
    }
    const float pos=w*float(value-lo)/float(hi-lo);fill(x,y,w,4,line,1,2);fill(x,y,pos,4,signal,1,2);
    fill(x+pos-5,y-3,10,10,paper,1,5);
}
void draw(HWND window,UINT width,UINT height){
    auto state=readState();
    const float scale=std::min({float(width)/1020.f,float(height)/820.f,1.25f});
    const float ox=(width-920*scale)/2,oy=(height-728*scale)/2;
    POINT point{};RECT client{};GetCursorPos(&point);ScreenToClient(window,&point);GetClientRect(window,&client);
    const float sx=client.right>0?float(width)/client.right:1.f,sy=client.bottom>0?float(height)/client.bottom:1.f;
    mx=(point.x*sx-ox)/scale;my=(point.y*sy-oy)/scale;
    down=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
    if(resetInput.exchange(false)){lastDown=down;dragging=0;}
    clicked=down&&!lastDown;lastDown=down;if(!down)dragging=0;
    dc->SetTransform(D2D1::Matrix3x2F::Identity());fill(0,0,float(width),float(height),ink,0.14f);
    dc->SetTransform(D2D1::Matrix3x2F::Scale(scale,scale)*D2D1::Matrix3x2F::Translation(ox,oy));
    fill(5,10,920,728,ink,0.32f,20);fill(0,0,920,728,ink,0.92f,20);border(0,0,920,728,0x45443c,0.7f,20);
    text(30,27,400,42,L"LUMEN",30,paper,true);text(30,72,550,18,L"AURON NETWORK  /  BEDROCK 26.50",10,signal,true);
    text(590,30,300,18,L"YOUR WORLD. CLEARER.",11,muted,false,true);
    text(590,54,300,24,L"INSERT  /  ESC   Close",12,paper,false,true);
    if(clicked&&hit(590,48,300,35))changeState(Close,0);
    fill(30,108,498,540,panel,0.90f,14);fill(548,108,342,264,panel,0.90f,14);fill(548,390,342,258,panel,0.90f,14);
    text(52,132,390,28,L"Xray",18,paper,true);toggle(462,130,state.xray,Xray);
    text(52,170,454,22,L"Discover what lies beneath the surface.",12,muted);
    text(52,202,340,24,L"Radius");text(390,202,116,24,std::to_wstring(state.radius)+L" blocks",14,signal,true,true);
    slider(52,234,454,state.radius,4,24,Radius);
    text(52,252,340,24,L"Markers");text(390,252,116,24,std::to_wstring(state.limit),14,signal,true,true);
    slider(52,284,454,state.limit,32,512,Limit,32);
    text(52,324,390,28,L"Show through walls");toggle(462,322,state.walls,Walls);
    text(52,362,400,20,L"ORE FILTERS",10,muted,true);
    static const wchar_t* names[]={L"Diamond",L"Emerald",L"Gold",L"Iron",L"Redstone",L"Lapis",L"Coal",L"Copper",L"Quartz",L"Ancient Debris"};
    for(int i=0;i<10;i++){
        const float x=52.f+(i%3)*156.f,y=386.f+(i/3)*36.f;const bool active=(state.ores&(1u<<i))!=0;
        const bool hover=hit(x,y,142,30);fill(x,y,142,30,active?signal:ink,active?(hover?0.16f:0.07f):0.22f,7);
        border(x,y,142,30,active||hover?signal:line,active?0.8f:1.f,7);
        text(x+12,y+6,118,22,names[i],12,active?signal:muted,true);
        if(clicked&&hover)changeState(Ore,i);
    }
    text(52,577,454,48,state.status,12,muted);
    text(570,132,230,28,L"Zoom",18,paper,true);toggle(824,130,state.zoom,Zoom);
    text(570,174,298,24,L"Get closer with a single key.",12,muted);
    text(570,208,225,26,L"Magnification");text(803,208,65,26,std::to_wstring(state.zoomFactor)+L"×",14,signal,true,true);
    slider(570,250,298,state.zoomFactor,2,20,ZoomFactor);
    text(570,278,298,22,L"HOLD C",11,signal,true);
    text(570,309,298,42,L"Release to restore your normal view.",11,muted);
    text(570,416,246,26,L"Fullbright",18,paper,true);toggle(824,412,state.brightness,Brightness);
    text(570,459,298,25,L"See clearly, even in the dark.",12,muted);
    text(570,498,298,24,L"B  Toggle fullbright",13,signal,true);
    text(570,539,298,48,L"Turn off to restore your\noriginal game brightness.",12,muted);
    text(570,598,298,24,state.brightness?L"Currently enabled":L"Currently disabled",11,state.brightness?signal:muted);
    text(30,686,590,24,L"X  Xray    ·    Hold C  Zoom    ·    B  Fullbright",12,muted);
    const bool back=hit(690,671,200,40);if(back)fill(678,671,220,40,signal,0.10f,8);
    text(690,684,200,26,L"Back to game",13,signal,true,true);if(clicked&&back)changeState(Close,0);
}
void releaseResources(){
    initialized=false;if(dc)dc->SetTarget(nullptr);targets.clear();wrapped.clear();brush.Reset();dc.Reset();d2device.Reset();
    if(context){context->ClearState();context->Flush();}on12.Reset();context.Reset();device.Reset();chain3.Reset();activeChain=nullptr;
}
bool same(IUnknown* a,IUnknown* b){ComPtr<IUnknown> x,y;if(!a||!b)return false;a->QueryInterface(IID_PPV_ARGS(&x));b->QueryInterface(IID_PPV_ARGS(&y));return x.Get()==y.Get();}
bool initializeResources(IDXGISwapChain* chain){
    releaseResources();DXGI_SWAP_CHAIN_DESC desc{};check(chain->GetDesc(&desc));
    ComPtr<ID3D12Device> d12;chain->GetDevice(IID_PPV_ARGS(&d12));
    if(d12){
        ComPtr<ID3D12CommandQueue> queue;{std::lock_guard lock(queueMutex);for(auto& q:queues){ComPtr<ID3D12Device> qd;q->GetDevice(IID_PPV_ARGS(&qd));if(same(d12.Get(),qd.Get())){queue=q;break;}}}
        if(!queue)return false;
        IUnknown* list[]={queue.Get()};check(D3D11On12CreateDevice(d12.Get(),D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,list,1,0,&device,&context,nullptr));
        check(device.As(&on12));
    }else{check(chain->GetDevice(IID_PPV_ARGS(&device)));device->GetImmediateContext(&context);}
    check(chain->QueryInterface(IID_PPV_ARGS(&chain3)));
    if(!factory)check(D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED,IID_PPV_ARGS(&factory)));
    ComPtr<IDXGIDevice> dxgi;check(device.As(&dxgi));check(factory->CreateDevice(dxgi.Get(),&d2device));check(d2device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,&dc));
    dc->SetDpi(96,96);dc->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    for(UINT i=0;i<(on12?desc.BufferCount:1);i++){
        ComPtr<ID3D11Resource> resource;
        if(on12){ComPtr<ID3D12Resource> back;check(chain->GetBuffer(i,IID_PPV_ARGS(&back)));D3D11_RESOURCE_FLAGS flags{D3D11_BIND_RENDER_TARGET};
            check(on12->CreateWrappedResource(back.Get(),&flags,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PRESENT,IID_PPV_ARGS(&resource)));
        }else check(chain->GetBuffer(i,IID_PPV_ARGS(&resource)));
        ComPtr<IDXGISurface> surface;check(resource.As(&surface));DXGI_SURFACE_DESC sd{};check(surface->GetDesc(&sd));
        auto prop=D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET|D2D1_BITMAP_OPTIONS_CANNOT_DRAW,D2D1::PixelFormat(sd.Format,D2D1_ALPHA_MODE_PREMULTIPLIED),96,96);
        ComPtr<ID2D1Bitmap1> bitmap;check(dc->CreateBitmapFromDxgiSurface(surface.Get(),&prop,&bitmap));wrapped.push_back(resource);targets.push_back(bitmap);
    }
    check(dc->CreateSolidColorBrush(color(paper),&brush));activeChain=chain;initialized=true;logMessage(on12?"OVERLAY_READY DX12":"OVERLAY_READY DX11");return true;
}
void STDMETHODCALLTYPE execute(ID3D12CommandQueue* q,UINT count,ID3D12CommandList* const* lists){
    if(q->GetDesc().Type==D3D12_COMMAND_LIST_TYPE_DIRECT){std::lock_guard lock(queueMutex);bool found=false;for(auto& saved:queues)if(saved.Get()==q)found=true;if(!found&&queues.size()<8)queues.emplace_back(q);}
    originalExecute(q,count,lists);
}
HRESULT STDMETHODCALLTYPE present(IDXGISwapChain* chain,UINT interval,UINT flags){
    if(opened&&!(flags&DXGI_PRESENT_TEST)){
        std::lock_guard lock(renderMutex);DXGI_SWAP_CHAIN_DESC desc{};
        if(SUCCEEDED(chain->GetDesc(&desc))&&desc.OutputWindow&&GetForegroundWindow()==desc.OutputWindow){
            try{
                if((initialized&&activeChain==chain)||initializeResources(chain)){
                    const UINT index=on12?chain3->GetCurrentBackBufferIndex():0;
                    if(index<targets.size()){
                        auto resource=wrapped[index].Get();if(on12)on12->AcquireWrappedResources(&resource,1);
                        dc->SetTarget(targets[index].Get());dc->BeginDraw();const auto size=targets[index]->GetPixelSize();
                        try{draw(desc.OutputWindow,size.width,size.height);}catch(...){dc->EndDraw();if(on12)on12->ReleaseWrappedResources(&resource,1);context->Flush();throw;}
                        const HRESULT result=dc->EndDraw();if(on12)on12->ReleaseWrappedResources(&resource,1);context->Flush();check(result);
                    }
                }
            }catch(const std::exception& e){logMessage(std::string("OVERLAY_ERROR ")+e.what());releaseResources();opened=false;changeState(Close,0);}
        }
    }
    return originalPresent(chain,interval,flags);
}
HRESULT STDMETHODCALLTYPE resize(IDXGISwapChain* chain,UINT count,UINT w,UINT h,DXGI_FORMAT format,UINT flags){std::lock_guard lock(renderMutex);if(activeChain==chain)releaseResources();return originalResize(chain,count,w,h,format,flags);}
HRESULT STDMETHODCALLTYPE resize1(IDXGISwapChain3* chain,UINT count,UINT w,UINT h,DXGI_FORMAT format,UINT flags,const UINT* masks,IUnknown* const* qs){std::lock_guard lock(renderMutex);if(activeChain==chain)releaseResources();return originalResize1(chain,count,w,h,format,flags,masks,qs);}
void addHook(void* address,void* target,void** original){const auto result=MH_CreateHook(address,target,original);if(result!=MH_OK)throw std::runtime_error(MH_StatusToString(result));}
void initFonts(){
    check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory3),reinterpret_cast<IUnknown**>(writeFactory.GetAddressOf())));
    wchar_t path[32768]{};GetModuleFileNameW(module,path,32768);const auto assets=std::filesystem::path(path).parent_path()/L"assets";
    ComPtr<IDWriteFontSetBuilder> builder;check(writeFactory->CreateFontSetBuilder(&builder));
    for(const auto* filename:{L"Geist-Regular.ttf",L"Geist-SemiBold.ttf"}){ComPtr<IDWriteFontFaceReference> face;check(writeFactory->CreateFontFaceReference((assets/filename).c_str(),nullptr,0,DWRITE_FONT_SIMULATIONS_NONE,&face));check(builder->AddFontFaceReference(face.Get()));}
    ComPtr<IDWriteFontSet> set;check(builder->CreateFontSet(&set));check(writeFactory->CreateFontCollectionFromFontSet(set.Get(),&fonts));
    ComPtr<IDWriteFontFamily> fam;check(fonts->GetFontFamily(0,&fam));ComPtr<IDWriteLocalizedStrings> names;check(fam->GetFamilyNames(&names));UINT32 length{};check(names->GetStringLength(0,&length));std::wstring value(length+1,L'\0');check(names->GetString(0,value.data(),length+1));value.resize(length);family=value;
    logMessage("OVERLAY_FONT Geist loaded");
}
} // namespace
void setOpen(bool value){opened=value;resetInput=true;}
bool isOpen(){return opened;}
bool input(HWND,UINT msg,WPARAM,LPARAM){
    if(!opened)return false;
    if(msg==WM_SETCURSOR){SetCursor(LoadCursorW(nullptr,IDC_ARROW));return true;}
    // Keep pointer motion flowing through GameCore. Capture is prevented by
    // the ClientInstance hook; only game buttons/keys are consumed here.
    return msg==WM_CHAR||msg==WM_KEYDOWN||msg==WM_SYSKEYDOWN||
        (msg>=WM_MOUSEFIRST&&msg<=WM_MOUSELAST&&msg!=WM_MOUSEMOVE);
}
void initialize(HMODULE instance,Read read,Change change,Log log){
    module=instance;readState=read;changeState=change;logMessage=log;initFonts();
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=module;wc.lpszClassName=L"Lumen.DX.Probe";RegisterClassW(&wc);
    HWND probe=CreateWindowW(wc.lpszClassName,L"",WS_OVERLAPPED,0,0,64,64,nullptr,nullptr,module,nullptr);
    if(!probe)throw std::runtime_error("DirectX probe window failed");
    try{
        DXGI_SWAP_CHAIN_DESC desc{};desc.BufferCount=2;desc.BufferDesc.Width=64;desc.BufferDesc.Height=64;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.OutputWindow=probe;desc.SampleDesc.Count=1;desc.Windowed=TRUE;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
        ComPtr<IDXGISwapChain> swap;ComPtr<ID3D11Device> d11;ComPtr<ID3D11DeviceContext> ctx;
        check(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,&desc,&swap,&d11,nullptr,&ctx));
        auto table=*reinterpret_cast<void***>(swap.Get());addHook(table[8],reinterpret_cast<void*>(present),reinterpret_cast<void**>(&originalPresent));addHook(table[13],reinterpret_cast<void*>(resize),reinterpret_cast<void**>(&originalResize));
        ComPtr<IDXGISwapChain3> s3;if(SUCCEEDED(swap.As(&s3)))addHook((*reinterpret_cast<void***>(s3.Get()))[39],reinterpret_cast<void*>(resize1),reinterpret_cast<void**>(&originalResize1));
        ComPtr<ID3D12Device> d12;if(SUCCEEDED(D3D12CreateDevice(nullptr,D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&d12)))){
            D3D12_COMMAND_QUEUE_DESC qd{};qd.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;ComPtr<ID3D12CommandQueue> q;check(d12->CreateCommandQueue(&qd,IID_PPV_ARGS(&q)));
            addHook((*reinterpret_cast<void***>(q.Get()))[10],reinterpret_cast<void*>(execute),reinterpret_cast<void**>(&originalExecute));
        }
    }catch(...){DestroyWindow(probe);UnregisterClassW(wc.lpszClassName,module);throw;}
    DestroyWindow(probe);UnregisterClassW(wc.lpszClassName,module);
}
} // namespace overlay
