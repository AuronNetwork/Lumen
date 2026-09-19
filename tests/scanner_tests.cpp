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
    check(classify("minecraft:chest")==Chest,"chest");
    check(classify("minecraft:trapped_chest")==TrappedChest,"trapped chest");
    check(classify("minecraft:ender_chest")==EnderChest,"ender chest");
    for(const auto name:{"custom:chest","minecraft:chest_minecart","minecraft:chest_boat",
                         "minecraft:deepslate_chest","minecraft:barrel","minecraft:stone"})
        check(classify(name)==-1,"only supported chest blocks");
    check(!selected(defaultMask,Chest)&&!selected(defaultMask,TrappedChest)&&
          !selected(defaultMask,EnderChest),"existing selections remain opt-in for chests");
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
    blocks={{{1,0,0},classify("minecraft:chest")},{{2,0,0},classify("minecraft:chest")},
            {{0,0,1},classify("minecraft:trapped_chest")},{{0,0,2},classify("minecraft:ender_chest")},
            {{3,0,0},classify("minecraft:diamond_ore")},{{5,0,0},classify("minecraft:chest")},
            {{0,1,0},blockCount},{{0,2,0},32}};
    const unsigned chests=(1u<<Chest)|(1u<<TrappedChest)|(1u<<EnderChest);
    for(int i=0;i<10;++i)tick({},chests);
    check(scan.visible({},256).size()==4,"all chest variants and double chest halves in range");
    check(scan.visible({},2).size()==2,"chests share marker limit");
    for(const auto& hit:scan.visible({},256))
        check(hit.ore>=Chest&&hit.ore<blockCount,"ore and invalid IDs excluded from chest scan");
    for(int i=0;i<10;++i)tick({},1u<<EnderChest);
    check(scan.visible({},256).size()==1&&scan.visible({},256)[0].ore==EnderChest,"independent ender chest filter");
    for(int i=0;i<10;++i)tick({},defaultMask|chests);
    check(scan.visible({},256).size()==5,"ore and chest filters together");
    for(int i=0;i<10;++i)tick({},1u<<Chest);
    check(scan.visible({},256).size()==2,"normal chest filter only");
    blocks.erase({1,0,0});
    for(int i=0;i<4;++i)tick({},1u<<Chest);
    check(scan.visible({},256).size()==1&&scan.visible({},256)[0].pos==Pos{2,0,0},"removed double chest half disappears");
    blocks[{2,0,0}]=TrappedChest;
    for(int i=0;i<4;++i)tick({},1u<<Chest);
    check(scan.visible({},256).empty(),"replaced chest no longer matches filter");
    for(int i=0;i<10;++i)tick({},~0u);
    check(scan.visible({},256).size()==4,"invalid reader IDs cannot reach renderer or shift out of bounds");
    tick({},0);
    check(scan.visible({},256).empty(),"disabling all filters clears chests");
    std::cout<<"PASS: ore/chest variants, independent/combined filters, double chests, radius, marker/read limits, removal, invalid IDs, teleport, reset\n";
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
