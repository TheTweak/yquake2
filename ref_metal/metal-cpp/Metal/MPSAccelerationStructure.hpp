#pragma once

#include "MTLDefines.hpp"
#include "MTLHeaderBridge.hpp"
#include "MTLPrivate.hpp"

#include <Foundation/Foundation.hpp>

#include "MTLDevice.hpp"

namespace MTL {
_MTL_ENUM(NS::UInteger, MPSAccelerationStructureStatus) {
    Built = 0,
    Unbuilt = 1,
};
class MPSAccelerationStructure : public NS::Copying<MPSAccelerationStructure> {
public:
    MTL::MPSAccelerationStructureStatus status() const;
};
}

// method: status
_MTL_INLINE MTL::MPSAccelerationStructureStatus MTL::MPSAccelerationStructure::status() const
{
    return Object::sendMessage<MTL::MPSAccelerationStructureStatus>(this, _MTL_PRIVATE_SEL(status));
}