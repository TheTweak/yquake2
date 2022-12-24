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
#include "../image/Image.hpp"

#define BLOCK_WIDTH 1024
#define BLOCK_HEIGHT 512

class Model {
    std::unordered_map<std::string, std::shared_ptr<mtl_model_t>> models;
    YQ2_ALIGNAS_TYPE(int) byte mod_novis[MAX_MAP_LEAFS / 8];
    
    Model();
    
    std::shared_ptr<mtl_model_t> loadMD2(std::string name, void *buffer, int modfilelen);
    std::shared_ptr<mtl_model_t> loadSP2(std::string name, void *buffer, int modfilelen);
    std::shared_ptr<mtl_model_t> loadBrushModel(std::string name, void *buffer, int modfilelen);
    void Mod_LoadTexinfo(mtl_model_t *loadmodel, byte *mod_base, lump_t *l);
public:
    static Model& getInstance();
    std::optional<std::shared_ptr<mtl_model_t>> getModel(std::string name, std::optional<std::shared_ptr<mtl_model_t>> parent, bool crash);
    const byte* clusterPVS(int cluster, const mtl_model_t* model);
    model_s* registerModel(char *name, std::shared_ptr<mtl_model_t> worldModel);
};

#endif /* Model_hpp */
