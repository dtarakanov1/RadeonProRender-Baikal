/**********************************************************************
Copyright (c) 2018 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
********************************************************************/

#pragma once

#include "app_config.h"
#include "data_generator_params.h"
#include "filesystem.h"

#include "Rpr/WrapObject/CameraObject.h"
#include "Rpr/WrapObject/LightObject.h"

#include <vector>


class ObjectLoader
{
public:
    explicit ObjectLoader(const AppConfig& config);

    const std::vector<CameraObject>& Cameras() const;
    const std::vector<LightObject>& Lights() const;
    const std::vector<size_t>& Spp() const;

    DataGeneratorParams GetDataGeneratorParams() const;

private:
    void ValidateConfig(const AppConfig& config) const;

    void LoadCameras();
    void LoadLights();
    void LoadSpp();

    void LoadScene();

    std::vector<CameraObject> m_cameras;
    std::vector<LightObject> m_lights;
    std::vector<size_t> m_spp;

    std::shared_ptr<Baikal::Scene1> m_scene;

    AppConfig m_app_config;
};