#pragma once

#include "MTLDefines.hpp"
#include "MTLHeaderBridge.hpp"
#include "MTLPrivate.hpp"

#include <Foundation/Foundation.hpp>

#include "MTLDevice.hpp"

namespace MTL {

class MPSTriangleAccelerationStructure : public NS::Copying<MPSTriangleAccelerationStructure> {
public:
    static class MPSTriangleAccelerationStructure* alloc();
    class MPSTriangleAccelerationStructure* init();
    void setVertexBuffer(const class Buffer* buffer);
    void setTriangleCount(NS::UInteger triangleCount);
    void setMaskBuffer(const class Buffer* buffer);
    void rebuild();
};

}

// static method: alloc
_MTL_INLINE MTL::MPSTriangleAccelerationStructure* MTL::MPSTriangleAccelerationStructure::alloc()
{
    return NS::Object::alloc<MTL::MPSTriangleAccelerationStructure>(_MTL_PRIVATE_CLS(MPSTriangleAccelerationStructure));
}

// method: init
_MTL_INLINE MTL::MPSTriangleAccelerationStructure* MTL::MPSTriangleAccelerationStructure::init()
{
    return Object::sendMessage<MTL::MPSTriangleAccelerationStructure*>(this, _MTL_PRIVATE_SEL(init));
}

// method: setVertexBuffer
_MTL_INLINE void MTL::MPSTriangleAccelerationStructure::setVertexBuffer(const MTL::Buffer* buffer)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setVertexBuffer_), buffer);
}

// method: setTriangleCount
_MTL_INLINE void MTL::MPSTriangleAccelerationStructure::setTriangleCount(NS::UInteger triangleCount)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setTriangleCount_), triangleCount);
}

// method: rebuild
_MTL_INLINE void MTL::MPSTriangleAccelerationStructure::rebuild()
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(rebuild));
}

// method: setMasksBuffer
_MTL_INLINE void MTL::MPSTriangleAccelerationStructure::setMaskBuffer(const MTL::Buffer* buffer)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setMaskBuffer_), buffer);
}