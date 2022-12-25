//
//  AliasModel.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 24.12.2022.
//

#ifndef AliasModel_hpp
#define AliasModel_hpp

#include <string>
#include <vector>
#include <optional>
#include <simd/simd.h>

#include "Renderable.hpp"

class Polygon: public Renderable {
    std::string textureName;
    std::vector<Vertex> vertices;
    std::optional<simd_float4x4> mvp;
    simd_float4x4 translation;
    float alpha;
    bool triangle = true;
    bool clamp = false;
    
    MTL::RenderPipelineState *pipelineState;
public:
    Polygon() = default;
    Polygon(std::string textureName, simd_float4x4 translation, float alpha, MTL::RenderPipelineState *pipelineState);
    void setIsTriangle(bool isTriangle);
    bool isTriangle();
    void setClamp(bool clamp);
    void setMVP(simd_float4x4 mvp);
    void addVertex(Vertex v);
    void render(MTL::RenderCommandEncoder*, vector_uint2 viewportSize) override;
};

#endif /* AliasModel_hpp */
