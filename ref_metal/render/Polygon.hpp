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
    simd_float3 staticLight = simd_make_float3(0, 0, 0);
    
    MTL::RenderPipelineState *pipelineState;
public:
    Polygon() = default;
    Polygon(std::string textureName, simd_float4x4 translation, float alpha, MTL::RenderPipelineState *pipelineState);
    void setIsTriangle(bool isTriangle);
    bool isTriangle();
    void setClamp(bool clamp);
    void setMVP(simd_float4x4 mvp);
    void addVertex(Vertex v);
    void setStaticLight(float r, float g, float b);
    const std::vector<Vertex>& getVertices() const;
    void render(MTL::RenderCommandEncoder*, vector_uint2 viewportSize) override;
    void render(MTL::RenderCommandEncoder*, vector_uint2 viewportSize, MTL::Buffer *vertexBuffer, size_t vertexBufferOffset);
};

#endif /* Polygon_hpp */
