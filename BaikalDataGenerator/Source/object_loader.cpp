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

#include "object_loader.h"
#include "XML/tinyxml2.h"

#include "BaikalIO/image_io.h"
#include "BaikalIO/scene_io.h"
#include "Rpr/RadeonProRender.h"
#include "Rpr/WrapObject/Materials/MaterialObject.h"

#include "material_io.h"
#include "utils.h"

#include <unordered_map>


// validation checks helper macroses to reduce copy paste
#define ASSERT_PATH(file_name) \
    if (file_name.empty()) \
    { \
        THROW_EX("Missing: " << file_name.string()) \
    }

#define ASSERT_XML(file_name) \
    if (file_name.extension() != ".xml") \
    { \
        THROW_EX("Not and XML file: " << file_name.string()) \
    }

#define ASSERT_FILE_EXISTS(file_name) \
    if (!std::filesystem::exists(file_name)) \
    { \
        THROW_EX("File not found: " << file_name.string()) \
    } \

void ObjectLoader::ValidateConfig(const AppConfig& config) const
{
    // validate input config
    ASSERT_PATH(config.camera_file);
    ASSERT_PATH(config.light_file);
    ASSERT_PATH(config.spp_file);
    ASSERT_PATH(config.scene_file);
    ASSERT_PATH(config.output_dir);

    // validate extensions
    ASSERT_XML(config.camera_file)
    ASSERT_XML(config.light_file)
    ASSERT_XML(config.spp_file)

    // validate that files really exists
    ASSERT_FILE_EXISTS(config.camera_file)
    ASSERT_FILE_EXISTS(config.light_file)
    ASSERT_FILE_EXISTS(config.spp_file)
    ASSERT_FILE_EXISTS(config.scene_file)

    if (!std::filesystem::is_directory(config.output_dir))
    {
        THROW_EX("Not a directory: " << config.output_dir.string())
    }
}

const std::unordered_map<std::string, LightObject::Type> kLightTypesMap =
{
    { "point", LightObject::Type::kPointLight },
    { "spot", LightObject::Type::kSpotLight },
    { "direct", LightObject::Type::kDirectionalLight },
    { "ibl", LightObject::Type::kEnvironmentLight },
};

ObjectLoader::ObjectLoader(const AppConfig& config) : m_app_config(config)
{
    ValidateConfig(config);

    LoadCameras();

    if ((config.split_num == 0) || (config.split_num > m_cameras.size()))
    {
        THROW_EX("'split_num' should be positive and less than camera states number");
    }

    if (config.split_idx >= config.split_num)
    {
        THROW_EX("'split_idx' must be less than split_num");
    }

    m_cameras = GetSplitByIdx(m_cameras, config.split_num, config.split_idx);

    LoadLights();
    LoadSpp();
}

DataGeneratorParams ObjectLoader::GetDataGeneratorParams() const
{
    DataGeneratorParams params;

    params.width = static_cast<unsigned>(m_app_config.width);
    params.height = static_cast<unsigned>(m_app_config.height);

    params.output_dir = m_app_config.output_dir.string().c_str();

    return params;
}

void ObjectLoader::LoadCameras()
{
    tinyxml2::XMLDocument doc;
    doc.LoadFile(m_app_config.camera_file.string().c_str());

    auto root = doc.FirstChildElement("cam_list");

    if (!root)
    {
        THROW_EX("Failed to open cameras set file: " << m_app_config.camera_file.string())
    }

    tinyxml2::XMLElement* elem = root->FirstChildElement("camera");

    while (elem)
    {
        RadeonRays::float3 eye;
        eye.x = elem->FloatAttribute("cpx");
        eye.y = elem->FloatAttribute("cpy");
        eye.z = elem->FloatAttribute("cpz");

        RadeonRays::float3 at;
        at.x = elem->FloatAttribute("tpx");
        at.y = elem->FloatAttribute("tpy");
        at.z = elem->FloatAttribute("tpz");

        RadeonRays::float3 up;
        up.x = elem->FloatAttribute("upx");
        up.y = elem->FloatAttribute("upy");
        up.z = elem->FloatAttribute("upz");

        if (up.sqnorm() == 0.f)
        {
            up = RadeonRays::float3(0.f, 1.f, 0.f);
        }

        auto camera = new CameraObject();
        camera->LookAt(eye, at, up);

        // default sensor width
        float sensor_width = 0.036f;
        float sensor_height = static_cast<float>(m_app_config.height) /
                static_cast<float>(m_app_config.width) * sensor_width;

        camera->SetSensorSize(RadeonRays::float2(0.036f, sensor_height));
        camera->SetFocalLength(elem->FloatAttribute("focal_length"));
        camera->SetFocusDistance(elem->FloatAttribute("focus_dist"));
        camera->SetAperture(elem->FloatAttribute("aperture"));

        m_cameras.push_back(*camera);

        elem = elem->NextSiblingElement("camera");
    }
}

void ObjectLoader::LoadLights()
{
    tinyxml2::XMLDocument doc;
    doc.LoadFile(m_app_config.light_file.string().c_str());
    auto root = doc.FirstChildElement("light_list");

    if (!root)
    {
        THROW_EX("Failed to open lights set file: " << m_app_config.light_file.string())
    }

    tinyxml2::XMLElement* elem = root->FirstChildElement("light");

    while (elem)
    {
        LightObject* light;

        std::string type_name = elem->Attribute("type");
        try
        {
            auto light_type = kLightTypesMap.at(elem->Attribute("type"));
            light = new LightObject(light_type);
        }
        catch (std::out_of_range& ex)
        {
            THROW_EX("Unsupported light type: " << elem->Attribute("type"))
        }

        switch (light->GetType())
        {
            case LightObject::Type::kSpotLight:
            {
                //this option available only for spot light
                auto csx = elem->FloatAttribute("csx");
                auto csy = elem->FloatAttribute("csy");

                light->SetSpotConeShape(RadeonRays::float2(csx, csy));
                break;
            }
            case LightObject::Type::kEnvironmentLight:
            {
                //this options available only for ibl
                auto multiplier = elem->FloatAttribute("mul");
                std::filesystem::path texture_path = elem->Attribute("tex");
                // find texture path and check that it exists
                if (texture_path.is_relative())
                {
                    auto lights_dir = m_app_config.light_file.parent_path();
                    texture_path = lights_dir / texture_path;
                }
                if (!std::filesystem::exists(texture_path))
                {
                    THROW_EX("Texture image not found: " << texture_path.string())
                }

                auto texture = MaterialObject::CreateImage(texture_path.string());
                light->SetEnvTexture(texture);
                light->SetEnvMultiplier(multiplier);
                break;
            }
            default:
                break;
        }

        RadeonRays::float3 position;
        position.x = elem->FloatAttribute("posx");
        position.y = elem->FloatAttribute("posy");
        position.z = elem->FloatAttribute("posz");

        RadeonRays::float3 direction;
        direction.x = elem->FloatAttribute("dirx");
        direction.y = elem->FloatAttribute("diry");
        direction.z = elem->FloatAttribute("dirz");

        RadeonRays::float3 radiance;
        radiance.x = elem->FloatAttribute("radx");
        radiance.y = elem->FloatAttribute("rady");
        radiance.z = elem->FloatAttribute("radz");

        light->SetPosition(position);
        light->SetDirection(direction);
        light->SetRadiantPower(radiance);

        m_lights.push_back(*light);

        elem = elem->NextSiblingElement("light");
    }
}

void ObjectLoader::LoadSpp()
{
    tinyxml2::XMLDocument doc;
    doc.LoadFile(m_app_config.spp_file.string().c_str());
    auto root = doc.FirstChildElement("spp_list");

    if (!root)
    {
        THROW_EX("Failed to open SPP file: " << m_app_config.spp_file.string())
    }

    tinyxml2::XMLElement* elem = root->FirstChildElement("spp");

    while (elem)
    {
        auto spp = static_cast<size_t>(elem->Int64Attribute("iter_num"));
        m_spp.push_back(spp);
        elem = elem->NextSiblingElement("spp");
    }
}

void ObjectLoader::LoadScene()
{
//    m_context = std::make_unique<CLWContext>(CLWContext::Create(devices[device_idx]));
//
//    m_factory = std::make_unique<ClwRenderFactory>(*m_context, "cache");
//
//    m_renderer.reset(dynamic_cast<MonteCarloRenderer*>(
//                             m_factory->CreateRenderer(ClwRenderFactory::RendererType::kUnidirectionalPathTracer).release()));
//
//    m_controller = m_factory->CreateSceneController();
//
//    for (auto& output_info : kMultipleIteratedOutputs)
//    {
//        m_outputs.push_back(m_factory->CreateOutput(m_width, m_height));
//        m_renderer->SetOutput(output_info.type, m_outputs.back().get());
//    }
//    for (auto& output_info : kSingleIteratedOutputs)
//    {
//        m_outputs.push_back(m_factory->CreateOutput(m_width, m_height));
//        m_renderer->SetOutput(output_info.type, m_outputs.back().get());
//    }
//
//    m_renderer->SetMaxBounces(m_num_bounces);


    // workaround to avoid issues with tiny_object_loader
    auto scene_dir = m_app_config.scene_file.parent_path().string();

    if (scene_dir.back() != '/' || scene_dir.back() != '\\')
    {
#ifdef WIN32
        scene_dir.append("\\");
#else
        scene_dir.append("/");
#endif
    }

    m_scene = Baikal::SceneIo::LoadScene(m_app_config.scene_file.string(), scene_dir);

    // load materials.xml if it exists
    auto materials_file = m_app_config.scene_file.parent_path() / "materials.xml";
    auto mapping_file = m_app_config.scene_file.parent_path() / "mapping.xml";

    if (std::filesystem::exists(materials_file) &&
        std::filesystem::exists(mapping_file))
    {
        auto material_io = MaterialIo::CreateMaterialIoXML();
        auto materials = material_io->LoadMaterials(materials_file.string());
        auto mapping = material_io->LoadMaterialMapping(mapping_file.string());

        material_io->ReplaceSceneMaterials(*m_scene, *materials, mapping);
    }
    else
    {
        std::cout << "WARNING: materials.xml or mapping.xml is missed" << std::endl;
    }
}

const std::vector<CameraObject>& ObjectLoader::Cameras() const
{
    return m_cameras;
}


const std::vector<LightObject>& ObjectLoader::Lights() const
{
    return m_lights;
}

const std::vector<size_t>& ObjectLoader::Spp() const
{
    return m_spp;
}

const std::filesystem::path& ObjectLoader::LightsDir() const
{
    return m_lights_dir;
}


