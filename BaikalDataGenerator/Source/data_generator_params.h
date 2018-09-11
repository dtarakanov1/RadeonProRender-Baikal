#pragma once

#include "Rpr/WrapObject/CameraObject.h"
#include "Rpr/WrapObject/LightObject.h"


struct DataGeneratorParams
{
    const char* output_dir;
    SceneObject* scene;

    CameraObject const* cameras;
    unsigned int cameras_num;
    int cameras_start_idx;

    LightObject const* lights;
    unsigned int lights_num;

    unsigned int const* spp;
    unsigned int spp_num;

    unsigned int width;
    unsigned int height;

    unsigned int bounces_num;
    unsigned int device_idx;
};

