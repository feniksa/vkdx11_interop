#include "HybridProRenderer.hpp"
#include "rpr_helper.hpp"
#include <RadeonProRender_VK.h>
#include <set>

inline constexpr int FramesInFlight = 3;

HybridProRenderer::HybridProRenderer(const Paths& paths,
    HANDLE sharedTextureHandle,
    rpr_uint width,
    rpr_uint height,
    GpuIndices gpuIndices)
{
    std::cout << "[HybridProRenderer] HybridProRenderer()" << std::endl;

    std::vector<rpr_context_properties> properties;
    rpr_creation_flags creation_flags = RPR_CREATION_FLAGS_ENABLE_GPU0;
    m_postProcessing = std::move(PostProcessing(paths, sharedTextureHandle, true, width, height, gpuIndices));

    // std::vector<VkSemaphore> releaseSemaphores;
    // releaseSemaphores.reserve(FramesInFlight);
    // m_semaphores.resize(FramesInFlight);
    // for (size_t i = 0; i < FramesInFlight; i++) {
    //     VkSemaphoreCreateInfo semaphoreInfo {};
    //     semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    //     VkFenceCreateInfo fenceInfo {};
    //     fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    //     fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    //     VK_CHECK(vkCreateSemaphore(m_postProcessing->getVkDevice(), &semaphoreInfo, nullptr, &m_semaphores[i].ready));
    //     VK_CHECK(vkCreateSemaphore(m_postProcessing->getVkDevice(), &semaphoreInfo, nullptr, &m_semaphores[i].release));
    //     VK_CHECK(vkCreateFence(m_postProcessing->getVkDevice(), &fenceInfo, nullptr, &m_semaphores[i].fence));
    //     releaseSemaphores.push_back(m_semaphores[i].release);
    // }

    // VkInteropInfo::VkInstance instance = {
    //     m_postProcessing->getVkPhysicalDevice(),
    //     m_postProcessing->getVkDevice()
    // };

    // VkInteropInfo interopInfo = {
    //     .instance_count = 1,
    //     .main_instance_index = 0,
    //     .frames_in_flight = FramesInFlight,
    //     .framebuffers_release_semaphores = releaseSemaphores.data(),
    //     .instances = &instance,
    // };

    // creation_flags |= RPR_CREATION_FLAGS_ENABLE_VK_INTEROP;
    // properties.push_back((rpr_context_properties)RPR_CONTEXT_CREATEPROP_VK_INTEROP_INFO);
    // properties.push_back((rpr_context_properties)&interopInfo);

    properties.push_back(0);

    rpr_int pluginId = rprRegisterPlugin(paths.hybridproDll.string().c_str());
    rpr_int plugins[] = { pluginId };
    RPR_CHECK(rprCreateContext(
        RPR_API_VERSION,
        plugins,
        sizeof(plugins) / sizeof(plugins[0]),
        creation_flags,
        properties.data(),
        paths.hybridproCacheDir.string().c_str(),
        &m_context));
    RPR_CHECK(rprContextSetActivePlugin(m_context, plugins[0]));
    RPR_CHECK(rprContextSetParameterByKey1u(m_context, RPR_CONTEXT_Y_FLIP, RPR_TRUE));
    RPR_CHECK(rprContextSetParameterByKey1u(m_context, RPR_CONTEXT_IBL_DISPLAY, RPR_FALSE));
    RPR_CHECK(rprContextSetParameterByKey1u(m_context, RPR_CONTEXT_MAX_RECURSION, 10));
    RPR_CHECK(rprContextSetParameterByKey1f(m_context, RPR_CONTEXT_DISPLAY_GAMMA, 1.0f));

    RPR_CHECK(rprContextCreateScene(m_context, &m_scene));
    RPR_CHECK(rprContextSetScene(m_context, m_scene));
    RPR_CHECK(rprContextCreateMaterialSystem(m_context, 0, &m_matsys));

    // Camera
    {
        RPR_CHECK(rprContextCreateCamera(m_context, &m_camera));
        RPR_CHECK(rprSceneSetCamera(m_scene, m_camera));
        RPR_CHECK(rprObjectSetName(m_camera, (rpr_char*)"camera"));
        RPR_CHECK(rprCameraSetMode(m_camera, RPR_CAMERA_MODE_PERSPECTIVE));
        RPR_CHECK(rprCameraLookAt(m_camera,
            8.0f, 2.0f, 8.0f, // pos
            0.0f, 0.0f, 0.0f, // at
            0.0f, 1.0f, 0.0f // up
            ));
        const float sensorHeight = 24.0f;
        float aspectRatio = (float)width / height;
        RPR_CHECK(rprCameraSetSensorSize(m_camera, sensorHeight * aspectRatio, sensorHeight));
    }

    // teapot material
    {
        RPR_CHECK(rprMaterialSystemCreateNode(m_matsys, RPR_MATERIAL_NODE_UBERV2, &m_teapotMaterial));
        RPR_CHECK(rprMaterialNodeSetInputFByKey(m_teapotMaterial, RPR_MATERIAL_INPUT_UBER_DIFFUSE_COLOR, 0.7f, 0.2f, 0.0f, 1.0f));
        RPR_CHECK(rprMaterialNodeSetInputFByKey(m_teapotMaterial, RPR_MATERIAL_INPUT_UBER_DIFFUSE_WEIGHT, 1.0f, 1.0f, 1.0f, 1.0f));
    }

    // teapot
    {
        auto teapotShapePath = paths.assetsDir / "teapot.obj";
        m_teapot = ImportOBJ(teapotShapePath.string(), m_context);
        RPR_CHECK(rprSceneAttachShape(m_scene, m_teapot));
        RPR_CHECK(rprShapeSetMaterial(m_teapot, m_teapotMaterial));
        rpr_float transform[16] = {
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, -1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.3f, 0.0f, 1.0f,
        };
        RPR_CHECK(rprShapeSetTransform(m_teapot, false, transform));
    }

    // shadow reflection catcher material
    {
        RPR_CHECK(rprMaterialSystemCreateNode(m_matsys, RPR_MATERIAL_NODE_UBERV2, &m_shadowReflectionCatcherMaterial));
        RPR_CHECK(rprMaterialNodeSetInputFByKey(m_shadowReflectionCatcherMaterial, RPR_MATERIAL_INPUT_UBER_DIFFUSE_COLOR, 0.3f, 0.3f, 0.3f, 1.0f));
        RPR_CHECK(rprMaterialNodeSetInputFByKey(m_shadowReflectionCatcherMaterial, RPR_MATERIAL_INPUT_UBER_DIFFUSE_WEIGHT, 1.0f, 1.0f, 1.0f, 1.0f));

        //RPR_CHECK(rprMaterialNodeSetInputFByKey(m_shadowReflectionCatcherMaterial, RPR_MATERIAL_INPUT_UBER_REFLECTION_COLOR, 0.3f, 0.3f, 0.3f, 1.0f));
        RPR_CHECK(rprMaterialNodeSetInputFByKey(m_shadowReflectionCatcherMaterial, RPR_MATERIAL_INPUT_UBER_REFLECTION_WEIGHT, 1.0f, 1.0f, 1.0f, 1.0f));
        RPR_CHECK(rprMaterialNodeSetInputFByKey(m_shadowReflectionCatcherMaterial, RPR_MATERIAL_INPUT_UBER_REFLECTION_IOR, 50.0f, 50.0f, 50.0f, 50.0f));
        RPR_CHECK(rprMaterialNodeSetInputFByKey(m_shadowReflectionCatcherMaterial, RPR_MATERIAL_INPUT_UBER_REFLECTION_ROUGHNESS, 0.0f, 0.0f, 0.0f, 0.0f));
    }

    // shadow reflection catcher plane
    {
        std::vector<rpr_float> vertices = {
            -1.0f, 0.0f, -1.0f,
            -1.0f, 0.0f,  1.0f,
            1.0f, 0.0f,  1.0f,
            1.0f, 0.0f, -1.0f,
        };

        std::vector<rpr_float> normals = {
            0.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
        };

        std::vector<rpr_float> uvs = {
            0.0f, 1.0f,
            0.0f, 0.0f,
            1.0f, 0.0f,
            1.0f, 1.0f,
        };

        std::vector<rpr_int> indices = {
            3,1,0,
            2,1,3
        };

        std::vector<rpr_int> numFaceVertices;
        numFaceVertices.resize(indices.size() / 3, 3);

        RPR_CHECK(rprContextCreateMesh(m_context,
            (rpr_float const*)&vertices[0], vertices.size() / 3, 3 * sizeof(rpr_float),
            (rpr_float const*)&normals[0], normals.size() / 3, 3 * sizeof(rpr_float),
            (rpr_float const*)&uvs[0], uvs.size() / 2, 2 * sizeof(rpr_float),
            (rpr_int const*)&indices[0], sizeof(rpr_int),
            (rpr_int const*)&indices[0], sizeof(rpr_int),
            (rpr_int const*)&indices[0], sizeof(rpr_int),
            (rpr_int const*)&numFaceVertices[0], numFaceVertices.size(), &m_shadowReflectionCatcherPlane));
        RPR_CHECK(rprSceneAttachShape(m_scene, m_shadowReflectionCatcherPlane));
        RPR_CHECK(rprShapeSetMaterial(m_shadowReflectionCatcherPlane, m_shadowReflectionCatcherMaterial));
        rpr_float transform[16] = {
            8.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 8.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 8.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f,
        };
        RPR_CHECK(rprShapeSetTransform(m_shadowReflectionCatcherPlane, false, transform));
        RPR_CHECK(rprShapeSetShadowCatcher(m_shadowReflectionCatcherPlane, true));
        RPR_CHECK(rprShapeSetReflectionCatcher(m_shadowReflectionCatcherPlane, true));
    }

    // ibl image
    {
        auto iblPath = paths.assetsDir / "envLightImage.exr";
        RPR_CHECK(rprContextCreateImageFromFile(m_context, iblPath.string().c_str(), &m_iblimage));
        RPR_CHECK(rprObjectSetName(m_iblimage, "iblimage"));
    }

    // env light
    {
        RPR_CHECK(rprContextCreateEnvironmentLight(m_context, &m_light));
        RPR_CHECK(rprObjectSetName(m_light, "light"));
        RPR_CHECK(rprEnvironmentLightSetImage(m_light, m_iblimage));
        rpr_float float_P16_4[] = {
            -1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f
        };
        RPR_CHECK(rprLightSetTransform(m_light, false, (rpr_float*)&float_P16_4));
        RPR_CHECK(rprSceneSetEnvironmentLight(m_scene, m_light));
    }

    // aovs
    {
        std::set<rpr_aov> aovs = {
            RPR_AOV_COLOR,
            RPR_AOV_OPACITY,
            RPR_AOV_SHADOW_CATCHER,
            RPR_AOV_REFLECTION_CATCHER,
            RPR_AOV_MATTE_PASS,
            RPR_AOV_BACKGROUND,
        };
        const rpr_framebuffer_desc desc = { width, height };
        const rpr_framebuffer_format fmt = { 4, RPR_COMPONENT_TYPE_FLOAT32 };
        for (auto aov : aovs) {
            rpr_framebuffer fb;
            RPR_CHECK(rprContextCreateFrameBuffer(m_context, fmt, &desc, &fb));
            RPR_CHECK(rprContextSetAOV(m_context, aov, fb));
            m_aovs[aov] = fb;
        }
    }
}

HybridProRenderer::~HybridProRenderer()
{
    std::cout << "[HybridProRenderer] ~HybridProRenderer()" << std::endl;
    for (auto& s : m_semaphores) {
        vkDestroySemaphore(m_postProcessing->getVkDevice(), s.ready, nullptr);
        vkDestroySemaphore(m_postProcessing->getVkDevice(), s.release, nullptr);
        vkDestroyFence(m_postProcessing->getVkDevice(), s.fence, nullptr);
    }

    RPR_CHECK(rprSceneDetachLight(m_scene, m_light));
    RPR_CHECK(rprSceneDetachShape(m_scene, m_teapot));
    for (auto& aov : m_aovs) {
        RPR_CHECK(rprObjectDelete(aov.second));
    }
    RPR_CHECK(rprObjectDelete(m_light));
    m_light = nullptr;
    RPR_CHECK(rprObjectDelete(m_iblimage));
    m_iblimage = nullptr;
    RPR_CHECK(rprObjectDelete(m_teapot));
    m_teapot = nullptr;
    RPR_CHECK(rprObjectDelete(m_shadowReflectionCatcherPlane));
    m_shadowReflectionCatcherPlane = nullptr;
    RPR_CHECK(rprObjectDelete(m_camera));
    m_camera = nullptr;
    RPR_CHECK(rprObjectDelete(m_scene));
    m_scene = nullptr;
    RPR_CHECK(rprObjectDelete(m_shadowReflectionCatcherMaterial));
    m_shadowReflectionCatcherMaterial = nullptr;
    RPR_CHECK(rprObjectDelete(m_teapotMaterial));
    m_teapotMaterial = nullptr;
    RPR_CHECK(rprObjectDelete(m_matsys));
    m_matsys = nullptr;
    CheckNoLeak(m_context);
    RPR_CHECK(rprObjectDelete(m_context));
    m_context = nullptr;
}

const std::vector<uint8_t>& HybridProRenderer::readAovBuff(rpr_aov aovKey)
{
    rpr_framebuffer aov = m_aovs[aovKey];
    size_t size;
    RPR_CHECK(rprFrameBufferGetInfo(aov, RPR_FRAMEBUFFER_DATA, 0, nullptr, &size));

    if (m_tmpAovBuff.size() != size) {
        m_tmpAovBuff.resize(size);
    }

    uint8_t* buff = nullptr;
    RPR_CHECK(rprFrameBufferGetInfo(aov, RPR_FRAMEBUFFER_DATA, size, m_tmpAovBuff.data(), nullptr));
    return m_tmpAovBuff;
}

void HybridProRenderer::render(rpr_uint iterations)
{
    RPR_CHECK(rprContextSetParameterByKey1u(m_context, RPR_CONTEXT_ITERATIONS, iterations));
    RPR_CHECK(rprContextRender(m_context));

    m_postProcessing->updateAovColor(m_aovs[RPR_AOV_COLOR]);
    m_postProcessing->updateAovOpacity(m_aovs[RPR_AOV_OPACITY]);
    m_postProcessing->updateAovShadowCatcher(m_aovs[RPR_AOV_SHADOW_CATCHER]);
    m_postProcessing->updateAovReflectionCatcher(m_aovs[RPR_AOV_REFLECTION_CATCHER]);
    m_postProcessing->updateAovMattePass(m_aovs[RPR_AOV_MATTE_PASS]);
    m_postProcessing->updateAovBackground(m_aovs[RPR_AOV_BACKGROUND]);
    m_postProcessing->run();
}

void HybridProRenderer::saveResultTo(const char* path, rpr_aov aov)
{
    RPR_CHECK(rprFrameBufferSaveToFile(m_aovs[aov], path));
}