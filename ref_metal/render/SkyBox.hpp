//
//  SkyBox.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 27.12.2022.
//

#ifndef SkyBox_hpp
#define SkyBox_hpp

#include <string>
#include <array>

#include "Renderable.hpp"
#include "../../src/client/vid/header/ref.h"

class SkyBox: public Renderable {
    float rotate;
    vec3_t axis;
    std::array<std::string, 6> textureNames;
public:
    const std::string name;
    SkyBox(std::string name, float rotate, vec3_t axis);
    void render(MTL::RenderCommandEncoder*, vector_uint2 viewportSize) override;
};

#endif /* SkyBox_hpp */
