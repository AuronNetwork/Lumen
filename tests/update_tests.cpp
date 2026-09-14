// SPDX-License-Identifier: GPL-3.0-only
#include "../src/update_policy.h"
#include <iostream>
#include <functional>
void check(bool value){if(!value)throw std::runtime_error("Update policy assertion failed");}
void rejects(const std::function<void()>& f){bool caught=false;try{f();}catch(...){caught=true;}check(caught);}
int main(){
    using namespace updates;
    check(version("1.4.10")>version("1.4.9"));check(version("2.0.0")>version("1.65535.65535"));
    for(const auto* bad:{"1.2","1.2.3.4","1.2.-1","1.2.3-beta","01.2.3","1.2.65536","1..3","1.2.3/evil"})rejects([&]{version(bad);});
    nlohmann::json j={{"draft",false},{"prerelease",false},{"tag_name","v1.4.10"},{"assets",nlohmann::json::array({{{"name","Lumen-Setup.exe"},{"state","uploaded"},{"size",1024},{"browser_download_url","https://github.com/AuronNetwork/Lumen/releases/download/v1.4.10/Lumen-Setup.exe"},{"digest","sha256:"+std::string(64,'a')}}})}};
    check(release(j.dump(),"AuronNetwork/Lumen")->number==version("1.4.10"));
    auto bad=j;bad["draft"]=true;check(!release(bad.dump(),"AuronNetwork/Lumen"));bad=j;bad["prerelease"]=true;check(!release(bad.dump(),"AuronNetwork/Lumen"));
    for(const auto* url:{"http://github.com/AuronNetwork/Lumen/releases/download/v1.4.10/Lumen-Setup.exe","https://github.com/other/Lumen/releases/download/v1.4.10/Lumen-Setup.exe","https://evil.example/Lumen-Setup.exe"}){bad=j;bad["assets"][0]["browser_download_url"]=url;rejects([&]{release(bad.dump(),"AuronNetwork/Lumen");});}
    for(const auto* hash:{"","sha1:abc","sha256:"}){bad=j;bad["assets"][0]["digest"]=hash;rejects([&]{release(bad.dump(),"AuronNetwork/Lumen");});}
    bad=j;bad["assets"][0]["digest"]=nullptr;rejects([&]{release(bad.dump(),"AuronNetwork/Lumen");});
    bad=j;bad["assets"][0]["size"]=maxInstallerBytes+1;rejects([&]{release(bad.dump(),"AuronNetwork/Lumen");});
    bad=j;bad["assets"].push_back(bad["assets"][0]);rejects([&]{release(bad.dump(),"AuronNetwork/Lumen");});
    bad=j;bad["assets"]=nlohmann::json::array();rejects([&]{release(bad.dump(),"AuronNetwork/Lumen");});
    check(allowedHost(L"release-assets.githubusercontent.com"));check(!allowedHost(L"github.com.evil.example"));check(!allowedHost(L"evilgithubusercontent.com"));
    std::cout<<"UPDATE_POLICY_OK\n";
}
