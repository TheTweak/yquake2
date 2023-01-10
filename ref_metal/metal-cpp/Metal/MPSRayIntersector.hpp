#pragma once

#include "MTLDefines.hpp"
#include "MTLHeaderBridge.hpp"
#include "MTLPrivate.hpp"

#include <Foundation/Foundation.hpp>

#include "MTLDevice.hpp"

namespace MTL {

class MPSRayIntersector : public NS::Copying<MPSRayIntersector> {
public:
    static class MPSRayIntersector* alloc();
    class MPSRayIntersector* init();
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
