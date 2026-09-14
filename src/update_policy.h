// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>
#include <charconv>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include "../third_party/nlohmann/json.hpp"

namespace updates {
using Version=std::array<unsigned,3>;
constexpr std::uint64_t maxInstallerBytes=64*1024*1024;
inline Version version(std::string_view value){
    Version result{};
    for(int i=0;i<3;++i){const auto end=value.find('.');const auto part=value.substr(0,end);
        if(part.empty()||(part.size()>1&&part[0]=='0')||(i<2&&end==std::string_view::npos)||(i==2&&end!=std::string_view::npos))throw std::runtime_error("Invalid release version");
        const auto parsed=std::from_chars(part.data(),part.data()+part.size(),result[i]);
        if(parsed.ec!=std::errc{}||parsed.ptr!=part.data()+part.size()||result[i]>65535)throw std::runtime_error("Invalid release version");
        if(i<2)value.remove_prefix(end+1);
    }return result;
}
struct Release {Version number;std::string tag,url,sha256;std::uint64_t size;};
inline std::optional<Release> release(std::string_view json,const std::string& repository){
    const auto data=nlohmann::json::parse(json);
    if(data.at("draft").get<bool>()||data.at("prerelease").get<bool>())return std::nullopt;
    const auto tag=data.at("tag_name").get<std::string>();
    if(tag.empty()||tag[0]!='v')throw std::runtime_error("Invalid release tag");
    const auto number=version(std::string_view(tag).substr(1));
    std::optional<Release> result;
    for(const auto& asset:data.at("assets")){
        if(asset.at("name").get<std::string>()!="Lumen-Setup.exe")continue;
        if(result)throw std::runtime_error("Duplicate installer asset");
        const auto url=asset.at("browser_download_url").get<std::string>();
        if(url!="https://github.com/"+repository+"/releases/download/"+tag+"/Lumen-Setup.exe")throw std::runtime_error("Unexpected installer URL");
        const auto size=asset.at("size").get<std::uint64_t>();
        if(!size||size>maxInstallerBytes||asset.at("state").get<std::string>()!="uploaded")throw std::runtime_error("Invalid installer asset");
        auto hash=asset.at("digest").get<std::string>();
        if(hash.size()!=71||hash.substr(0,7)!="sha256:")throw std::runtime_error("Missing installer SHA-256");
        hash.erase(0,7);for(auto& c:hash){if(c>='A'&&c<='F')c+=32;if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))throw std::runtime_error("Invalid installer SHA-256");}
        result=Release{number,tag,url,hash,size};
    }
    if(!result)throw std::runtime_error("Release has no verified installer");
    return result;
}
inline bool allowedHost(std::wstring_view host){return host==L"api.github.com"||host==L"github.com"||host.ends_with(L".githubusercontent.com");}
} // namespace updates
