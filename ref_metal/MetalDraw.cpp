//
//  MetalDraw.cpp
//  yq2
//
//  Created by SOROKIN EVGENY on 13.09.2022.
//

#include <iostream>
#include "MetalDraw.hpp"

class MetalRenderer : public Renderer {
public:
    MetalRenderer() {
        std::cout << "MetalRenderer ctor";
    }
    
    virtual ~MetalRenderer() {
        std::cout << "MetalRenderer de-ctor";
    }
};

__attribute__((__visibility__("default")))
extern "C" Renderer* CreateRenderer() {
    return new MetalRenderer();
}

extern "C" void DestroyRenderer(Renderer* r) {
    if (r) {
        delete r;
    }
}
