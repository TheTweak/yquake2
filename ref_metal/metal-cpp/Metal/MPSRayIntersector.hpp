#pragma once

#include "MTLDefines.hpp"
#include "MTLHeaderBridge.hpp"
#include "MTLPrivate.hpp"

#include <Foundation/Foundation.hpp>

#include "MTLDevice.hpp"
#include "MPSAccelerationStructure.hpp"

namespace MTL {

_MTL_ENUM(NS::UInteger, MPSIntersectionType) {
    Any = 0,
    Nearest = 1,
};

_MTL_ENUM(NS::UInteger, MPSRayDataType) {
    OriginDirection = 0,
    OriginMaskDirectionMaxDistance = 1,
    OriginMinDistanceDirectionMaxDistance = 2,
    PackedOriginDirection = 3
};

class MPSRayIntersector : public NS::Copying<MPSRayIntersector> {
public:
    void setRayDataType(MTL::MPSRayDataType rayDataType);
    void setRayStride(NS::UInteger rayStride);
    static class MPSRayIntersector* alloc();
    class MPSRayIntersector* init();
    void encodeIntersection(
            const class MTL::CommandBuffer* commandBuffer,
            MTL::MPSIntersectionType intersectionType,
            const class MTL::Buffer* rayBuffer,
            NS::UInteger rayBufferOffset,
            const class MTL::Buffer* intersectionBuffer,
            NS::UInteger intersectionBufferOffset,
            NS::UInteger rayCount,
            const class MTL::MPSAccelerationStructure* accelerationStructure);
};

}

// static method: alloc
_MTL_INLINE MTL::MPSRayIntersector* MTL::MPSRayIntersector::alloc()
{
    return NS::Object::alloc<MTL::MPSRayIntersector>(_MTL_PRIVATE_CLS(MPSRayIntersector));
}

// method: init
_MTL_INLINE MTL::MPSRayIntersector* MTL::MPSRayIntersector::init()
{
    return Object::sendMessage<MTL::MPSRayIntersector*>(this, _MTL_PRIVATE_SEL(init));
}

// method: encodeIntersection(commandBuffer:intersectionType:rayBuffer:rayBufferOffset:intersectionBuffer:intersectionBufferOffset:rayCount:accelerationStructure:)
_MTL_INLINE void MTL::MPSRayIntersector::encodeIntersection(
            const class MTL::CommandBuffer* commandBuffer,
            MTL::MPSIntersectionType intersectionType,
            const class MTL::Buffer* rayBuffer,
            NS::UInteger rayBufferOffset,
            const class MTL::Buffer* intersectionBuffer,
            NS::UInteger intersectionBufferOffset,
            NS::UInteger rayCount,
            const class MTL::MPSAccelerationStructure* accelerationStructure)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(encodeIntersection_commandBuffer_intersectionType_rayBuffer_rayBufferOffset_intersectionBuffer_intersectionBufferOffset_rayCount_accelerationStructure_), commandBuffer, intersectionType, rayBuffer, rayBufferOffset, intersectionBuffer, intersectionBufferOffset, rayCount, accelerationStructure);
}
