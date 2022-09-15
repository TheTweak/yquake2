//
//  MetalDraw.cpp
//  yq2
//
//  Created by SOROKIN EVGENY on 13.09.2022.
//

#include <iostream>
#include "MetalDraw.hpp"

class MetalRenderer : Renderer {
public:
    MetalRenderer() {
        std::cout << "MetalRenderer ctor";
    }
    
    virtual ~MetalRenderer() {
        std::cout << "MetalRenderer de-ctor";
    }
};

extern "C" Renderer* CreateRenderer() {
    return new MetalRenderer();
}

extern "C" void DestroyRenderer(Renderer* r) {
    if (r) {
        delete r;
    }
}
