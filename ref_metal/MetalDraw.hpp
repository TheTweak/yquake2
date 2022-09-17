//
//  MetalDraw.hpp
//  yq2
//
//  Created by SOROKIN EVGENY on 13.09.2022.
//
#pragma once

#ifndef MetalDraw_hpp
#define MetalDraw_hpp

#pragma mark Renderer
class Renderer {
public:
    Renderer() {};
    virtual ~Renderer() {};
};

typedef Renderer* CreateRenderer_t();
typedef void DestroyRenderer_t(Renderer*);

#endif /* MetalDraw_hpp */
