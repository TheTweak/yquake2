//
//  BufferAllocator.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 17.11.2022.
//

#ifndef BufferAllocator_hpp
#define BufferAllocator_hpp

#include <stdio.h>
#include <vector>
#include <utility>
#include <MetalKit/MetalKit.hpp>

#include "Utils.hpp"

using BufferPool = std::pair<std::vector<MTL::Buffer*>, size_t>;

template<size_t BufferSize>
class BufferAllocator {
    BufferPool _pools[MAX_FRAMES_IN_FLIGHT];
    MTL::Device* _pDevice;
    void growPool(size_t pool);
    size_t _frame = 0;
    static const size_t _growFactor;
public:
    BufferAllocator(MTL::Device* pDevice);
    void updateFrame(size_t frame);
    MTL::Buffer* getNextBuffer();
};

template<size_t BufferSize> const size_t BufferAllocator<BufferSize>::_growFactor = 5;

template<size_t BufferSize>
BufferAllocator<BufferSize>::BufferAllocator(MTL::Device* pDevice): _pDevice(pDevice) {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        _pools[i] = std::make_pair(std::vector<MTL::Buffer*>(0), 0);
    }
}

template<size_t BufferSize>
void BufferAllocator<BufferSize>::updateFrame(size_t frame) {
    _frame = frame;
    _pools[_frame].second = 0;
}

template<size_t BufferSize>
void BufferAllocator<BufferSize>::growPool(size_t pool) {
    for (size_t i = 0; i < _growFactor; i++) {
        _pools[pool].first.push_back(_pDevice->newBuffer(BufferSize, MTL::ResourceStorageModeShared));
    }
}

template<size_t BufferSize>
MTL::Buffer* BufferAllocator<BufferSize>::getNextBuffer() {
    BufferPool& pool = _pools[_frame];
    if (pool.first.size() <= pool.second) {
        growPool(_frame);
    }
    return pool.first.at(pool.second++);
}

#endif /* BufferAllocator_hpp */
