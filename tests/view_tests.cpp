// SPDX-License-Identifier: GPL-3.0-only
#include "../src/view_features.h"
#include <iostream>
#include <limits>
#include <stdexcept>
void check(bool ok){if(!ok)throw std::runtime_error("view invariant failed");}
int main(){try {
    check(lumen::gammaValue(0.37f,true)==25.f);
    check(lumen::gammaValue(0.37f,false)==0.37f);
    check(lumen::gammaValue(0.81f,false)==0.81f); // New game option must not restore a stale saved value.
    float x=1.2f,y=0.8f;
    const auto ox=x,oy=y,wx=x*4,wy=y*4;x=wx;y=wy;
    lumen::restoreProjection(x,y,ox,oy,wx,wy);
    check(x==ox&&y==oy);
    x=2.f;y=3.f; // The game already produced a new projection.
    lumen::restoreProjection(x,y,ox,oy,wx,wy);
    check(x==2.f&&y==3.f);
    check(!lumen::validProjection(std::numeric_limits<float>::quiet_NaN(),1.f));
    check(!lumen::validProjection(1.f,std::numeric_limits<float>::infinity()));
    check(!lumen::validProjection(0.f,1.f));
    check(lumen::validProjection(1.2f,0.8f));
    check(lumen::zoomScale(-1)==2.f&&lumen::zoomScale(999)==20.f);
    std::cout<<"VIEW_TESTS_OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
