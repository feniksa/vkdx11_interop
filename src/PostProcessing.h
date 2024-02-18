#pragma once

#include "ImageFormat.h"
#include "ShaderManager.h"
#include "vk_helper.h"

#include <optional>
#include <variant>
#include <vector>
#include <memory>

namespace rprpp {

struct AovsVkInteropInfo {
    VkImage color;
    VkImage opacity;
    VkImage shadowCatcher;
    VkImage reflectionCatcher;
    VkImage mattePass;
    VkImage background;

    friend bool operator==(const AovsVkInteropInfo&, const AovsVkInteropInfo&) = default;
    friend bool operator!=(const AovsVkInteropInfo&, const AovsVkInteropInfo&) = default;
};

struct InteropAovs {
    vk::raii::ImageView color;
    vk::raii::ImageView opacity;
    vk::raii::ImageView shadowCatcher;
    vk::raii::ImageView reflectionCatcher;
    vk::raii::ImageView mattePass;
    vk::raii::ImageView background;
    vk::raii::Sampler sampler;
};

struct Aovs {
    vk::helper::Image color;
    vk::helper::Image opacity;
    vk::helper::Image shadowCatcher;
    vk::helper::Image reflectionCatcher;
    vk::helper::Image mattePass;
    vk::helper::Image background;
};

struct ToneMap {
    float whitepoint[3] = { 1.0f, 1.0f, 1.0f };
    float vignetting = 0.0f;
    float crushBlacks = 0.0f;
    float burnHighlights = 1.0f;
    float saturation = 1.0f;
    float cm2Factor = 1.0f;
    float filmIso = 100.0f;
    float cameraShutter = 8.0f;
    float fNumber = 2.0f;
    float focalLength = 1.0f;
    float aperture = 0.024f; // hardcoded in swviz
    // paddings, needed to make a correct memory allignment
    float _padding0; // int enabled; TODO: probably we won't need this field
    float _padding1;
    float _padding2;
};

struct Bloom {
    float radius;
    float brightnessScale;
    float threshold;
    int enabled = 0;
};

struct UniformBufferObject {
    ToneMap tonemap;
    Bloom bloom;
    float shadowIntensity = 1.0f;
    float invGamma = 1.0f;
};

class PostProcessing {
public:
    PostProcessing(vk::helper::DeviceContext&& dctx,
        vk::raii::CommandPool&& commandPool,
        vk::raii::CommandBuffer&& secondaryCommandBuffer,
        vk::raii::CommandBuffer&& computeCommandBuffer,
        vk::helper::Buffer&& uboBuffer) noexcept;

    PostProcessing(PostProcessing&&)            = default;
    PostProcessing& operator=(PostProcessing&&) = default;

    PostProcessing(PostProcessing&)                  = delete;
    PostProcessing& operator=(const PostProcessing&) = delete;

    static std::unique_ptr<PostProcessing> create(uint32_t deviceId);

    void* mapStagingBuffer(size_t size);
    void unmapStagingBuffer();
    void setFramesInFlihgt(uint32_t framesInFlight);
    void resize(uint32_t width, uint32_t height, ImageFormat format, HANDLE outputDx11TextureHandle, std::optional<AovsVkInteropInfo> aovsVkInteropInfo);
    void getOutput(uint8_t* dst, size_t size, size_t* retSize);
    void run(VkSemaphore aovsReadySemaphore, VkSemaphore toSignalAfterProcessingSemaphore);
    void waitQueueIdle();

    VkPhysicalDevice getVkPhysicalDevice() const noexcept;
    VkDevice getVkDevice() const noexcept;
    VkQueue getVkQueue() const noexcept;

    void copyStagingBufferToAovColor();
    void copyStagingBufferToAovOpacity();
    void copyStagingBufferToAovShadowCatcher();
    void copyStagingBufferToAovReflectionCatcher();
    void copyStagingBufferToAovMattePass();
    void copyStagingBufferToAovBackground();

    void setGamma(float gamma) noexcept;
    void setShadowIntensity(float shadowIntensity) noexcept;
    void setToneMapWhitepoint(float x, float y, float z) noexcept;
    void setToneMapVignetting(float vignetting) noexcept;
    void setToneMapCrushBlacks(float crushBlacks) noexcept;
    void setToneMapBurnHighlights(float burnHighlights) noexcept;
    void setToneMapSaturation(float saturation) noexcept;
    void setToneMapCm2Factor(float cm2Factor) noexcept;
    void setToneMapFilmIso(float filmIso) noexcept;
    void setToneMapCameraShutter(float cameraShutter) noexcept;
    void setToneMapFNumber(float fNumber) noexcept;
    void setToneMapFocalLength(float focalLength) noexcept;
    void setToneMapAperture(float aperture) noexcept;
    void setBloomRadius(float radius) noexcept;
    void setBloomBrightnessScale(float brightnessScale) noexcept;
    void setBloomThreshold(float threshold) noexcept;
    void setBloomEnabled(bool enabled) noexcept;
    void setDenoiserEnabled(bool enabled) noexcept;

private:
    void createShaderModule(ImageFormat outputFormat, bool sampledAovs);
    void createDescriptorSet();
    void createImages(uint32_t width, uint32_t height, ImageFormat outputFormat, HANDLE outputDx11TextureHandle, std::optional<AovsVkInteropInfo> aovsVkInteropInfo);
    void createComputePipeline();
    void recordComputeCommandBuffer(uint32_t width, uint32_t height);
    void transitionImageLayout(vk::helper::Image& image,
        vk::AccessFlags dstAccess,
        vk::ImageLayout dstLayout,
        vk::PipelineStageFlags dstStage);
    void copyStagingBufferToAov(vk::helper::Image& image);
    void updateUbo();

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_fenceIndex = 0;
    uint32_t m_framesInFlight = 0;
    HANDLE m_outputDx11TextureHandle = nullptr;
    ImageFormat m_outputImageFormat = ImageFormat::eR32G32B32A32Sfloat;
    std::optional<AovsVkInteropInfo> m_aovsVkInteropInfo;
    bool m_uboDirty = true;
    UniformBufferObject m_ubo;
    std::vector<const char*> m_enabledLayers;
    ShaderManager m_shaderManager;
    vk::helper::DeviceContext m_dctx;
    vk::raii::CommandPool m_commandPool;
    vk::raii::CommandBuffer m_secondaryCommandBuffer;
    vk::raii::CommandBuffer m_computeCommandBuffer;
    vk::helper::Buffer m_uboBuffer;
    std::optional<vk::raii::ShaderModule> m_shaderModule;
    std::optional<vk::helper::Buffer> m_stagingBuffer;
    std::optional<vk::helper::Image> m_outputImage;
    std::optional<std::variant<Aovs, InteropAovs>> m_aovs;
    std::optional<vk::raii::DescriptorSetLayout> m_descriptorSetLayout;
    std::optional<vk::raii::DescriptorPool> m_descriptorPool;
    std::optional<vk::raii::DescriptorSet> m_descriptorSet;
    std::optional<vk::raii::PipelineLayout> m_pipelineLayout;
    std::optional<vk::raii::Pipeline> m_computePipeline;
    std::vector<vk::raii::Fence> m_fences;
};

} // namespace rprpp