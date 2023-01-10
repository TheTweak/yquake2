//
//  Polygon.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 24.12.2022.
//

#ifndef Polygon_hpp
#define Polygon_hpp

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
    VertexBufferInfo render(MTL::RenderCommandEncoder*, vector_uint2 viewportSize) override;
};

#endif /* Polygon_hpp */
