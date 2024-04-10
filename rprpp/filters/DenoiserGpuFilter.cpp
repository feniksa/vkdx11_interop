#include "DenoiserGpuFilter.h"
#include "rprpp/Error.h"
#include <cassert>

#include <boost/log/trivial.hpp>

namespace rprpp::filters {

DenoiserGpuFilter::DenoiserGpuFilter(Context* context, oidn::DeviceRef& device)
    : DenoiserFilter(context, device)
{
}

vk::Semaphore DenoiserGpuFilter::run(std::optional<vk::Semaphore> waitSemaphore)
{
    validateInputsAndOutput();

    if (m_dirty) {
        deviceContext().queue.waitIdle();
        m_filter.release();
        m_colorBuffer.release();
        m_albedoBuffer.release();
        m_normalBuffer.release();
        m_stagingColorBuffer.reset();
        m_stagingAlbedoBuffer.reset();
        m_stagingNormalBuffer.reset();
        initialize();
        m_dirty = false;
    }

    vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eAllCommands;
    vk::SubmitInfo submitInfo;
    submitInfo.setCommandBuffers(*m_copyInputsCommands.get());
    if (waitSemaphore.has_value()) {
        submitInfo.setWaitDstStageMask(waitStage);
        submitInfo.setWaitSemaphores(waitSemaphore.value());
    }

    deviceContext().queue.submit(submitInfo);
    deviceContext().queue.waitIdle();

    m_filter.execute();
    const char* errorMessage;
    if (m_device.getError(errorMessage) != oidn::Error::None) {
        BOOST_LOG_TRIVIAL(error) << errorMessage;
        throw InternalError(std::string("oidn error: ") + errorMessage);
    }

    submitInfo = vk::SubmitInfo();
    submitInfo.setCommandBuffers(*m_copyOutputCommand.get());
    submitInfo.setSignalSemaphores(*m_finishedSemaphore);
    deviceContext().queue.submit(submitInfo);

    return *m_finishedSemaphore;
}

std::unique_ptr<Buffer> DenoiserGpuFilter::createStagingBufferFor(Image* image)
{
    auto usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer;
    auto props = vk::MemoryPropertyFlagBits::eDeviceLocal;
    bool win32Exportable = true;
    size_t size = image->description().width * image->description().height * to_pixel_size(image->description().format);
    return std::make_unique<Buffer>(image->context(), size, usage, props, win32Exportable);
}

void DenoiserGpuFilter::initialize()
{
    m_filter = m_device.newFilter("RT"); // generic ray tracing filter
    m_stagingColorBuffer = std::move(createStagingBufferFor(m_input));

    const uint32_t width = m_input->description().width;
    const uint32_t height = m_input->description().height;
    vk::MemoryGetWin32HandleInfoKHR handleInfo(m_stagingColorBuffer->memory(), vk::ExternalMemoryHandleTypeFlagBits::eOpaqueWin32);
    HANDLE win32hadnle = deviceContext().device.getMemoryWin32HandleKHR(handleInfo);
    m_colorBuffer = m_device.newBuffer(oidn::ExternalMemoryTypeFlag::OpaqueWin32, win32hadnle, nullptr, m_stagingColorBuffer->size());
    m_filter.setImage("color", m_colorBuffer, oidn::Format::Float3, width, height, 0, to_pixel_size(m_input->description().format), 0);
    m_filter.setImage("output", m_colorBuffer, oidn::Format::Float3, width, height, 0, to_pixel_size(m_output->description().format), 0);
    if (m_stagingAlbedoBuffer.get() && m_stagingNormalBuffer.get()) {
        handleInfo.memory = m_stagingAlbedoBuffer->memory();
        win32hadnle = deviceContext().device.getMemoryWin32HandleKHR(handleInfo);
        m_albedoBuffer = m_device.newBuffer(oidn::ExternalMemoryTypeFlag::OpaqueWin32, win32hadnle, nullptr, m_stagingAlbedoBuffer->size());
        m_filter.setImage("albedo", m_albedoBuffer, oidn::Format::Float3, width, height, 0, to_pixel_size(m_albedo->description().format), 0);

        handleInfo.memory = m_stagingNormalBuffer->memory();
        win32hadnle = deviceContext().device.getMemoryWin32HandleKHR(handleInfo);
        m_normalBuffer = m_device.newBuffer(oidn::ExternalMemoryTypeFlag::OpaqueWin32, win32hadnle, nullptr, m_stagingNormalBuffer->size());
        m_filter.setImage("normal", m_normalBuffer, oidn::Format::Float3, width, height, 0, to_pixel_size(m_normal->description().format), 0);
    }
    m_filter.set("hdr", true); // beauty image is HDR
    m_filter.commit();

    m_copyOutputCommand.get().begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eSimultaneousUse));
    copyBufferToImage(m_copyOutputCommand, m_stagingColorBuffer.get(), m_output);
    m_copyOutputCommand.get().end();

    m_copyInputsCommands.get().begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eSimultaneousUse));
    copyImageToBuffer(m_copyInputsCommands, m_input, m_stagingColorBuffer.get());
    if (m_albedo && m_normal) {
        m_stagingAlbedoBuffer = std::move(createStagingBufferFor(m_albedo));
        copyImageToBuffer(m_copyInputsCommands, m_albedo, m_stagingAlbedoBuffer.get());

        m_stagingNormalBuffer = std::move(createStagingBufferFor(m_normal));
        copyImageToBuffer(m_copyInputsCommands, m_normal, m_stagingNormalBuffer.get());
    }
    m_copyInputsCommands.get().end();
}

}