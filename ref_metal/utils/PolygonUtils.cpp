//
//  PolygonUtils.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 25.12.2022.
//

#include "PolygonUtils.hpp"

std::array<Vertex, 3> PolygonUtils::cutTriangle(glpoly_t* poly, int vertexIndex) {    
    std::array<Vertex, 3> vertices;
    {
        auto v = poly->vertices[0];
        vector_float3 vertex = {v.pos[0], v.pos[1], v.pos[2]};
        vector_float2 texCoord = {v.texCoord[0], v.texCoord[1]};
        vertices[0] = {vertex, texCoord};
    }
    {
        auto v = poly->vertices[vertexIndex-1];
        vector_float3 vertex = {v.pos[0], v.pos[1], v.pos[2]};
        vector_float2 texCoord = {v.texCoord[0], v.texCoord[1]};
        vertices[1] = {vertex, texCoord};
    }
    {
        auto v = poly->vertices[vertexIndex];
        vector_float3 vertex = {v.pos[0], v.pos[1], v.pos[2]};
        vector_float2 texCoord = {v.texCoord[0], v.texCoord[1]};
        vertices[2] = {vertex, texCoord};
    }
    return vertices;
}
