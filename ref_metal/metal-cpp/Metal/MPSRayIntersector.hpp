#pragma once

#include "MTLDefines.hpp"
#include "MTLHeaderBridge.hpp"
#include "MTLPrivate.hpp"

#include <Foundation/Foundation.hpp>

#include "MTLDevice.hpp"
#include "MPSTriangleAccelerationStructure.hpp"

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

_MTL_ENUM(NS::UInteger, MPSIntersectionDataType) {
    Distance = 0,
    DistancePrimitiveIndex = 1,
    DistancePrimitiveIndexCoordinates = 2,
    DistancePrimitiveIndexInstanceIndex = 3,
    DistancePrimitiveIndexInstanceIndexCoordinates = 4,
    DistancePrimitiveIndexCoordinatesBarycentrics = 5,
    DistancePrimitiveIndexInstanceIndexCoordinatesBarycentrics = 6,
};

_MTL_OPTIONS(NS::UInteger, MPSRayMaskOptions) {
    MPSRayMaskOptionsInstance = 0,
    MPSRayMaskOptionsPrimitive = 1,
};

class MPSRayIntersector : public NS::Copying<MPSRayIntersector> {
public:
    void setRayDataType(MTL::MPSRayDataType rayDataType);
    void setIntersectionDataType(MTL::MPSIntersectionDataType intersectionDataType);
    void setRayMaskOptions(MTL::MPSRayMaskOptions rayMaskOptions);
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
            const class MTL::MPSTriangleAccelerationStructure* accelerationStructure);
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
            const class MTL::MPSTriangleAccelerationStructure* accelerationStructure)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(encodeIntersection_commandBuffer_intersectionType_rayBuffer_rayBufferOffset_intersectionBuffer_intersectionBufferOffset_rayCount_accelerationStructure_), commandBuffer, intersectionType, rayBuffer, rayBufferOffset, intersectionBuffer, intersectionBufferOffset, rayCount, accelerationStructure);
}

// method: setRayDataType
_MTL_INLINE void MTL::MPSRayIntersector::setRayDataType(MTL::MPSRayDataType rayDataType)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setRayDataType), rayDataType);
}

// method: setRayStride
_MTL_INLINE void MTL::MPSRayIntersector::setRayStride(NS::UInteger rayStride)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setRayStride), rayStride);
}

// method: setIntersectionDataType
_MTL_INLINE void MTL::MPSRayIntersector::setIntersectionDataType(MTL::MPSIntersectionDataType intersectionDataType)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setIntersectionDataType), intersectionDataType);
}

// method: setRayMaskOptions
_MTL_INLINE void MTL::MPSRayIntersector::setRayMaskOptions(MTL::MPSRayMaskOptions rayMaskOptions)
{
    Object::sendMessage<void>(this, _MTL_PRIVATE_SEL(setRayMaskOptions), rayMaskOptions);
}