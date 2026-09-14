// SPDX-License-Identifier: GPL-3.0-only
#include "../src/updater.h"
#include "../src/launcher_ui.h"
#include <fstream>
#include <iostream>
namespace launcher_ui {void post(State,std::wstring){}}
int main(int argc,char** argv){
    try{
        if(argc==2&&std::string(argv[1])=="--download"){
            const auto release=updates::latest();if(!release)throw std::runtime_error("No stable release");
            const auto file=updates::download(*release);std::cout<<"VERIFIED_RELEASE "<<release->tag<<" "<<release->sha256<<"\n"<<file.string()<<'\n';return 0;
        }
        const auto path=std::filesystem::temp_directory_path()/("lumen-hash-test-"+std::to_string(GetCurrentProcessId()));
        {std::ofstream f(path,std::ios::binary);f<<"abc";}
        const auto first=updates::sha256(path);
        {std::ofstream f(path,std::ios::binary);f<<"abd";}
        const auto changed=updates::sha256(path);std::filesystem::remove(path);
        if(first!="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"||changed==first)throw std::runtime_error("SHA-256 validation failed");
        std::cout<<"UPDATER_HASH_OK\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
