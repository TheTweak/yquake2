//
//  Char.cpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 21.12.2022.
//

#include "Char.hpp"
#include "../MetalRenderer.hpp"

MTL::DepthStencilState *Char::noDepthTest = NULL;

Char::Char(int num, int x, int y, float scale, MTL::RenderPipelineState *pipelineState) : Texture2D("conchars", x, y, scale, pipelineState), num(num) {
    if (!noDepthTest) {
        MTL::DepthStencilDescriptor* pDesc = MTL::DepthStencilDescriptor::alloc()->init();
        pDesc = MTL::DepthStencilDescriptor::alloc()->init();
        pDesc->setDepthWriteEnabled(false);
        noDepthTest = MetalRenderer::getInstance().getDevice()->newDepthStencilState(pDesc);
        pDesc->release();
    }
}

std::optional<MTL::DepthStencilState*> Char::getDepthStencilState() {
    return noDepthTest;
}

std::optional<Quad> Char::createQuad(vector_uint2 viewportSize) {
    int row, col;
    float frow, fcol, size, scaledSize;
    num &= 255;
    
    if ((num & 127) == 32)
    {
        return std::nullopt; /* space */
    }
    
    if (y <= -8)
    {
        return std::nullopt; /* totally off screen */
    }
    
    row = num >> 4;
    col = num & 15;
    
    frow = row * 0.0625;
    fcol = col * 0.0625;
    size = 0.0625;
    
    scaledSize = 8*scale;
    
    float sl = fcol;
    float tl = frow;
    float sh = fcol + size;
    float th = frow + size;
            
    float halfWidth = scaledSize / 2.0f;
    float halfHeight = scaledSize / 2.0f;
    float offsetX = x + halfWidth - viewportSize[0] / 2.0;
    float offsetY = viewportSize[1] / 2.0 - (y + halfHeight);
    
    Quad vertices;
        //              Pixel positions               Texture coordinates
    vertices[0] = {{  halfWidth + offsetX, -halfHeight + offsetY }, { sh, th }};
    vertices[1] = {{ -halfWidth + offsetX, -halfHeight + offsetY }, { sl, th }};
    vertices[2] = {{ -halfWidth + offsetX,  halfHeight + offsetY }, { sl, tl }};
    vertices[3] = {{  halfWidth + offsetX, -halfHeight + offsetY }, { sh, th }};
    vertices[4] = {{ -halfWidth + offsetX,  halfHeight + offsetY }, { sl, tl }};
    vertices[5] = {{  halfWidth + offsetX,  halfHeight + offsetY }, { sh, tl }};
    
    return vertices;
}
