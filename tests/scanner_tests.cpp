#include "../src/scanner.h"
#include <iostream>
#include <stdexcept>
using namespace xray;
void check(bool value,const char* name){if(!value)throw std::runtime_error(name);}
int main(){try{
    check(classify("minecraft:deepslate_diamond_ore")==0,"deepslate");
    check(classify("minecraft:lit_deepslate_redstone_ore")==4,"lit redstone");
    check(classify("minecraft:nether_gold_ore")==2,"nether");
    check(classify("custom:diamond_ore")==-1,"namespace");
    check(classify("minecraft:stone")==-1,"ordinary block");
    std::unordered_map<Pos,int,PosHash> blocks{{{1,0,0},0},{{2,0,0},2},{{3,0,0},6},{{9,0,0},0}};
    Scanner scan;
    int actualReads=0;
    auto reader=[&](Pos p){++actualReads;auto it=blocks.find(p);return it==blocks.end()?-1:it->second;};
    auto tick=[&](Pos p,unsigned mask=defaultMask){actualReads=0;scan.tick(p,4,mask,100,reader,std::chrono::seconds(1));check(actualReads<=100,"shared read budget");};
    for(int i=0;i<10;++i)tick({});
    check(scan.visible({},256).size()==2,"radius and ore filters");
    check(scan.visible({},1).at(0).pos==Pos{1,0,0},"nearest first limit");
    blocks.erase({1,0,0});
    for(int i=0;i<4;++i)tick({});
    check(scan.visible({},256).size()==1,"mining refresh");
    blocks[{2,0,0}]=0;
    tick({});
    check(scan.visible({},256).at(0).ore==0,"ore type refresh");
    tick({30000000,10,-30000000});
    check(scan.visible({},256).empty(),"teleport clears stale hits");
    scan.clear();
    check(scan.reads==0 && scan.visible({},256).empty(),"leave reset");
    for(int i=0;i<10;++i)tick({},1u<<6);
    check(scan.visible({},256).size()==1 && scan.visible({},256)[0].ore==6,"filter change rebuild");
    std::cout<<"PASS: variants, namespace, filters, radius, nearest limit, total read budget, mining, type changes, teleport, reset\n";
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
