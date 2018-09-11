#pragma once

#include "filesystem.h"

#include <vector>


struct AppConfig
{
    std::filesystem::path scene_file;
    std::filesystem::path light_file;
    std::filesystem::path camera_file;
    std::filesystem::path spp_file;
    std::filesystem::path output_dir;
    size_t width = 0;
    size_t height = 0;
    size_t split_num = 1;
    size_t split_idx = 0;
    std::int32_t offset_idx = 0;
    std::uint32_t num_bounces = 5;
    std::uint32_t device_idx = 0;
    bool gamma_correction = false;
};
