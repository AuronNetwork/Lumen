// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace xray {
struct Pos { int x{}, y{}, z{}; bool operator==(const Pos&) const = default; };
struct Vec3 { float x{}, y{}, z{}; };
struct Color { float r, g, b, a{1}; };
struct Hit { Pos pos; int ore; };
struct PosHash {
    size_t operator()(Pos p) const {
        return (uint64_t(uint32_t(p.x)) * 0x9e3779b185ebca87ULL) ^
               (uint64_t(uint32_t(p.y)) * 0xc2b2ae3d27d4eb4fULL) ^ uint32_t(p.z);
    }
};
inline constexpr std::array<const wchar_t*, 10> names = {
    L"Diamond", L"Emerald", L"Gold", L"Iron", L"Redstone", L"Lapis Lazuli",
    L"Coal", L"Copper", L"Quartz", L"Ancient Debris"};
inline constexpr std::array<Color, 10> colors = {{
    {.1f,1,1}, {.1f,1,.3f}, {1,.8f,.1f}, {1,.7f,.5f}, {1,.15f,.15f},
    {.25f,.4f,1}, {.65f,.65f,.65f}, {1,.45f,.2f}, {.95f,.95f,1}, {.8f,.35f,1}}};
inline constexpr unsigned defaultMask = (1<<0)|(1<<1)|(1<<2)|(1<<3)|(1<<9);
inline int classify(std::string_view name) {
    if (!name.starts_with("minecraft:")) return -1;
    name.remove_prefix(10);
    if (name == "ancient_debris") return 9;
    if (name.starts_with("lit_")) name.remove_prefix(4);
    if (name.starts_with("deepslate_")) name.remove_prefix(10);
    if (name.starts_with("nether_")) name.remove_prefix(7);
    constexpr std::string_view ores[] = {"diamond_ore","emerald_ore","gold_ore","iron_ore",
        "redstone_ore","lapis_ore","coal_ore","copper_ore","quartz_ore"};
    for (int i=0;i<9;++i) if(name==ores[i]) return i;
    return -1;
}
inline int dist2(Pos a, Pos b) {
    // The scanner only keeps nearby positions; use double before squaring to
    // also make large teleport distances safe.
    const double x=double(a.x)-b.x, y=double(a.y)-b.y, z=double(a.z)-b.z;
    return int(std::min(2147483647.0, x*x+y*y+z*z));
}
class Scanner {
    std::vector<Pos> offsets;
    std::unordered_map<Pos,int,PosHash> found;
    Pos anchor{};
    int radius{};
    unsigned mask{};
    size_t cursor{}, refreshCursor{};
public:
    uint64_t reads{}, sweeps{};
    void clear() { found.clear(); offsets.clear(); radius=0; cursor=refreshCursor=0; reads=sweeps=0; }
    template<class Reader> void tick(Pos player,int requestedRadius,unsigned selected,int budget,Reader read,
                                    std::chrono::microseconds timeLimit=std::chrono::microseconds(1500)) {
        requestedRadius=std::clamp(requestedRadius,4,24);
        budget=std::clamp(budget,64,3000);
        if(radius!=requestedRadius || mask!=selected || dist2(player,anchor)>radius*radius) {
            clear(); radius=requestedRadius; mask=selected; anchor=player;
            for(int x=-radius;x<=radius;++x) for(int y=-radius;y<=radius;++y) for(int z=-radius;z<=radius;++z)
                if(x*x+y*y+z*z<=radius*radius) offsets.push_back({x,y,z});
            std::sort(offsets.begin(),offsets.end(),[](Pos a,Pos b){return dist2(a,{})<dist2(b,{});});
        }
        for(auto it=found.begin();it!=found.end();) {
            if(dist2(it->first,player)>radius*radius) it=found.erase(it); else ++it;
        }
        const auto start=std::chrono::steady_clock::now();
        int used=0;
        auto query=[&](Pos p) {
            const int ore=read(p); ++used; ++reads;
            if(ore>=0 && (selected&(1u<<ore))) {
                if(found.size()<4096 || found.contains(p)) found[p]=ore;
            } else found.erase(p);
        };
        // Revisit known ores within the same total read budget, including
        // mined blocks. Copy positions because query may erase an entry.
        std::vector<Pos> keys;
        keys.reserve(found.size());
        for(const auto& [p,ore]:found) { (void)ore; keys.push_back(p); }
        const size_t count=std::min<size_t>(keys.size(),std::min(64,budget/4));
        for(size_t i=0;i<count;++i) {
            if(std::chrono::steady_clock::now()-start>=timeLimit) break;
            query(keys[(refreshCursor+i)%keys.size()]);
        }
        refreshCursor+=count;
        while(used<budget && std::chrono::steady_clock::now()-start<timeLimit) {
            if(cursor==offsets.size()) { cursor=0; anchor=player; ++sweeps; }
            const Pos off=offsets[cursor++];
            const Pos p{anchor.x+off.x,anchor.y+off.y,anchor.z+off.z};
            if(p.y>=-64 && p.y<=319 && dist2(p,player)<=radius*radius) query(p);
        }
    }
    std::vector<Hit> visible(Pos player,size_t limit) const {
        std::vector<Hit> result;
        result.reserve(found.size());
        for(const auto& [p,ore]:found) result.push_back({p,ore});
        std::sort(result.begin(),result.end(),[&](const Hit& a,const Hit& b){return dist2(a.pos,player)<dist2(b.pos,player);});
        if(result.size()>limit) result.resize(limit);
        return result;
    }
};
}
