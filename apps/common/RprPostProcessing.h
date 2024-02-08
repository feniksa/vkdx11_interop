#pragma once

#include "rpr_helper.h"
#include <filesystem>
#include <rprpp.h>

class RprPostProcessing {
private:
    RprPpContext m_context = nullptr;

public:
    RprPostProcessing& operator=(RprPostProcessing&&) = delete;
    RprPostProcessing(RprPostProcessing&) = delete;
    RprPostProcessing& operator=(const RprPostProcessing&) = delete;
    RprPostProcessing(bool enableValidationLayer, uint32_t deviceId, const std::filesystem::path& postprocessingGlsl)
    {
        RPRPP_CHECK(rprppCreateContext(enableValidationLayer ? RPRPP_TRUE : RPRPP_FALSE, deviceId, postprocessingGlsl.string().c_str(), &m_context));
    }

    RprPostProcessing(RprPostProcessing&& other)
    {
        m_context = other.m_context;
        other.m_context = nullptr;
    }

    ~RprPostProcessing()
    {
        RPRPP_CHECK(rprppDestroyContext(m_context));
    }

    inline void* mapStagingBuffer(size_t size)
    {
        void* data = nullptr;
        RPRPP_CHECK(rprppContextMapStagingBuffer(m_context, size, &data));
        return data;
    }

    inline void unmapStagingBuffer()
    {
        RPRPP_CHECK(rprppContextUnmapStagingBuffer(m_context));
    }

    inline void resize(uint32_t width, uint32_t height, RprPpImageFormat format, RprPpDx11Handle sharedDx11TextureHandle = nullptr)
    {
        RPRPP_CHECK(rprppContextResize(m_context, width, height, format, sharedDx11TextureHandle));
    }

    inline void getOutput(uint8_t* dst, size_t size, size_t* retSize)
    {
        RPRPP_CHECK(rprppContextGetOutput(m_context, dst, size, retSize));
    }

    inline void run()
    {
        RPRPP_CHECK(rprppContextRun(m_context));
    }

    inline RprPpVkHandle getVkPhysicalDevice() const noexcept
    {
        RprPpVkHandle vkhandle = nullptr;
        RPRPP_CHECK(rprppContextGetVkPhysicalDevice(m_context, &vkhandle));
        return vkhandle;
    }

    inline RprPpVkHandle getVkDevice() const noexcept
    {
        RprPpVkHandle vkhandle = nullptr;
        RPRPP_CHECK(rprppContextGetVkDevice(m_context, &vkhandle));
        return vkhandle;
    }

    inline void copyStagingBufferToAovColor()
    {
        RPRPP_CHECK(rprppContextCopyStagingBufferToAovColor(m_context));
    }

    inline void copyStagingBufferToAovOpacity()
    {
        RPRPP_CHECK(rprppContextCopyStagingBufferToAovOpacity(m_context));
    }

    inline void copyStagingBufferToAovShadowCatcher()
    {
        RPRPP_CHECK(rprppContextCopyStagingBufferToAovShadowCatcher(m_context));
    }

    inline void copyStagingBufferToAovReflectionCatcher()
    {
        RPRPP_CHECK(rprppContextCopyStagingBufferToAovReflectionCatcher(m_context));
    }

    inline void copyStagingBufferToAovMattePass()
    {
        RPRPP_CHECK(rprppContextCopyStagingBufferToAovMattePass(m_context));
    }

    inline void copyStagingBufferToAovBackground()
    {
        RPRPP_CHECK(rprppContextCopyStagingBufferToAovBackground(m_context));
    }

    inline void setGamma(float gamma)
    {
        RPRPP_CHECK(rprppContextSetGamma(m_context, gamma));
    }

    inline void setShadowIntensity(float shadowIntensity)
    {
        RPRPP_CHECK(rprppContextSetShadowIntensity(m_context, shadowIntensity));
    }

    inline void setToneMapWhitepoint(float x, float y, float z)
    {
        RPRPP_CHECK(rprppContextSetToneMapWhitepoint(m_context, x, y, z));
    }

    inline void setToneMapVignetting(float vignetting)
    {
        RPRPP_CHECK(rprppContextSetToneMapVignetting(m_context, vignetting));
    }

    inline void setToneMapCrushBlacks(float crushBlacks)
    {
        RPRPP_CHECK(rprppContextSetToneMapCrushBlacks(m_context, crushBlacks));
    }

    inline void setToneMapBurnHighlights(float burnHighlights)
    {
        RPRPP_CHECK(rprppContextSetToneMapBurnHighlights(m_context, burnHighlights));
    }

    inline void setToneMapSaturation(float saturation)
    {
        RPRPP_CHECK(rprppContextSetToneMapSaturation(m_context, saturation));
    }

    inline void setToneMapCm2Factor(float cm2Factor)
    {
        RPRPP_CHECK(rprppContextSetToneMapCm2Factor(m_context, cm2Factor));
    }

    inline void setToneMapFilmIso(float filmIso)
    {
        RPRPP_CHECK(rprppContextSetToneMapFilmIso(m_context, filmIso));
    }

    inline void setToneMapCameraShutter(float cameraShutter)
    {
        RPRPP_CHECK(rprppContextSetToneMapCameraShutter(m_context, cameraShutter));
    }

    inline void setToneMapFNumber(float fNumber)
    {
        RPRPP_CHECK(rprppContextSetToneMapFNumber(m_context, fNumber));
    }

    inline void setToneMapFocalLength(float focalLength)
    {
        RPRPP_CHECK(rprppContextSetToneMapFocalLength(m_context, focalLength));
    }

    inline void setToneMapAperture(float aperture)
    {
        RPRPP_CHECK(rprppContextSetToneMapAperture(m_context, aperture));
    }

    inline void setBloomRadius(float radius)
    {
        RPRPP_CHECK(rprppContextSetBloomRadius(m_context, radius));
    }

    inline void setBloomBrightnessScale(float brightnessScale)
    {
        RPRPP_CHECK(rprppContextSetBloomBrightnessScale(m_context, brightnessScale));
    }

    inline void setBloomThreshold(float threshold)
    {
        RPRPP_CHECK(rprppContextSetBloomThreshold(m_context, threshold));
    }

    inline void setBloomEnabled(bool enabled)
    {
        RPRPP_CHECK(rprppContextSetBloomEnabled(m_context, enabled ? RPRPP_TRUE : RPRPP_FALSE));
    }

    inline void setDenoiserEnabled(bool enabled)
    {
        RPRPP_CHECK(rprppContextSetDenoiserEnabled(m_context, enabled ? RPRPP_TRUE : RPRPP_FALSE));
    }
};