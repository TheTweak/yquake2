//
//  MetalDraw.hpp
//  yq2
//
//  Created by SOROKIN EVGENY on 13.09.2022.
//

#ifndef MetalDraw_hpp
#define MetalDraw_hpp

#include <stdio.h>

class Renderer {
public:
    Renderer();
    virtual ~Renderer();
};

typedef Renderer* CreateRenderer();
typedef void DestroyRenderer(Renderer*);

#endif /* MetalDraw_hpp */
