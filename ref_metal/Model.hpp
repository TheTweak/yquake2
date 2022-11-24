//
//  Model.hpp
//  ref_metal
//
//  Created by SOROKIN EVGENY on 23.11.2022.
//

#ifndef Model_hpp
#define Model_hpp

#include <stdio.h>
#include <unordered_map>
#include <string>
#include <memory>
#include <utility>

#include "model.h"

#define MAX_LIGHTMAPS_PER_SURFACE 4

class Model {
    std::unordered_map<std::string, std::shared_ptr<mtl_model_t>> models;
    std::shared_ptr<mtl_model_t> loadMD2(std::string name, void *buffer, int modfilelen);
    std::shared_ptr<mtl_model_t> loadSP2(std::string name, void *buffer, int modfilelen);
    std::shared_ptr<mtl_model_t> loadBrushModel(std::string name, void *buffer, int modfilelen);
    
    YQ2_ALIGNAS_TYPE(int) byte mod_novis[MAX_MAP_LEAFS / 8];
public:
    std::optional<std::shared_ptr<mtl_model_t>> getModel(std::string name, std::optional<std::shared_ptr<mtl_model_t>> parent, bool crash);
    const byte* clusterPVS(int cluster, const mtl_model_t* model);
    
    Model();
};

#endif /* Model_hpp */
