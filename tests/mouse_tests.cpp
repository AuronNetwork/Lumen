// SPDX-License-Identifier: GPL-3.0-only
#include "../src/mouse_input.h"
#include <iostream>
#include <stdexcept>
void check(bool ok){if(!ok)throw std::runtime_error("mouse input regression");}
int main(){try{
    // A single poll can mix movement, held/released buttons and scrolling.
    // Consecutive button events must not survive erasure or discard motion.
    std::vector<lumen::MouseAction> events{
        {100,200,5,-4,0,0,11,false},
        {100,200,0,0,1,1,11,true},
        {100,200,0,0,1,0,11,true},
        {100,200,0,0,4,120,11,true},
        {116,198,16,-2,0,0,12,false},
        {116,198,0,0,2,1,12,true},
        {116,198,0,0,2,0,12,true},
    };
    lumen::consumeMouseButtons(events);
    check(events.size()==2);
    check(events[0].x==100&&events[0].y==200&&events[0].dx==5&&events[0].dy==-4&&events[0].pointerId==11);
    check(events[1].x==116&&events[1].y==198&&events[1].dx==16&&events[1].dy==-2&&events[1].pointerId==12);
    lumen::consumeMouseButtons(events);check(events.size()==2);
    events.clear();lumen::consumeMouseButtons(events);check(events.empty());
    std::cout<<"MOUSE_TESTS_OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
