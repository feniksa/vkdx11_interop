#include "Dx11App.hpp"

Dx11App::Dx11App(int width, int height, Paths paths, GpuIndices gpuIndices)
    : m_width(width)
    , m_height(height)
    , m_paths(paths)
    , m_gpuIndices(gpuIndices)
{
}

Dx11App::~Dx11App()
{
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

void Dx11App::run()
{
    initWindow();
    findAdapter();
    intiDx11();
    mainLoop();
}

void Dx11App::initWindow()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    m_window = glfwCreateWindow(m_width, m_height, "VkDx11 Interop", nullptr, nullptr);
    m_hWnd = glfwGetWin32Window(m_window);
    glfwSetFramebufferSizeCallback(m_window, Dx11App::onResize);
    glfwSetWindowUserPointer(m_window, this);
}

void Dx11App::findAdapter()
{
    ComPtr<IDXGIFactory4> factory;
    DX_CHECK(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)));
    ComPtr<IDXGIFactory6> factory6;
    DX_CHECK(factory->QueryInterface(IID_PPV_ARGS(&factory6)));

    std::cout << "[dx11app.hpp] "
              << "All Adapters:" << std::endl;

    ComPtr<IDXGIAdapter1> tmpAdapter;
    DXGI_ADAPTER_DESC selectedAdapterDesc;
    int adapterCount = 0;
    for (UINT adapterIndex = 0; SUCCEEDED(factory6->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&tmpAdapter))); ++adapterIndex) {
        adapterCount++;

        DXGI_ADAPTER_DESC desc;
        tmpAdapter->GetDesc(&desc);

        std::wcout << "[dx11app.hpp] "
                   << "\t" << adapterIndex << ". " << desc.Description << std::endl;

        if (adapterIndex == m_gpuIndices.dx11) {
            m_adapter = tmpAdapter;
            selectedAdapterDesc = desc;
        }
    }

    if (adapterCount <= m_gpuIndices.dx11) {
        throw std::runtime_error("[dx11app.hpp] could not find a IDXGIAdapter1, gpuIndices.dx11 is out of range");
    }

    std::wcout << "[dx11app.hpp] "
               << "Selected adapter: " << selectedAdapterDesc.Description << std::endl;
}

void Dx11App::intiDx11()
{
    DXGI_SWAP_CHAIN_DESC scd = {};
    ZeroMemory(&scd, sizeof(DXGI_SWAP_CHAIN_DESC));
    scd.BufferDesc.Width = 0; // use window width
    scd.BufferDesc.Height = 0; // use window height
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.SampleDesc.Count = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 1;
    scd.OutputWindow = m_hWnd;
    scd.Windowed = TRUE;
    auto featureLevel = D3D_FEATURE_LEVEL_11_1;
    DX_CHECK(D3D11CreateDeviceAndSwapChain(
        m_adapter.Get(),
        D3D_DRIVER_TYPE_UNKNOWN,
        nullptr,
        0,
        &featureLevel,
        1,
        D3D11_SDK_VERSION,
        &scd,
        &m_swapChain,
        &m_device,
        nullptr,
        &m_deviceContex));
    resize(m_width, m_height);
}

void Dx11App::resize(int width, int height)
{
    if (m_width != width || m_height != height || m_sharedTextureHandle == nullptr) {
        m_deviceContex->OMSetRenderTargets(0, nullptr, nullptr);
        m_sharedTextureHandle = nullptr;
        m_sharedTextureResource.Reset();
        m_sharedTexture.Reset();
        m_renderTargetView.Reset();
        m_backBuffer.Reset();
        DX_CHECK(m_swapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0));

        DX_CHECK(m_swapChain->GetBuffer(0, IID_PPV_ARGS(&m_backBuffer)));
        DX_CHECK(m_device->CreateRenderTargetView(m_backBuffer.Get(), nullptr, &m_renderTargetView));
        m_deviceContex->OMSetRenderTargets(1, &m_renderTargetView, nullptr);

        D3D11_TEXTURE2D_DESC sharedTextureDesc = {};
        sharedTextureDesc.Width = width;
        sharedTextureDesc.Height = height;
        sharedTextureDesc.MipLevels = 1;
        sharedTextureDesc.ArraySize = 1;
        sharedTextureDesc.SampleDesc = { 1, 0 };
        sharedTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        sharedTextureDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;
        DX_CHECK(m_device->CreateTexture2D(&sharedTextureDesc, nullptr, &m_sharedTexture));
        DX_CHECK(m_sharedTexture->QueryInterface(__uuidof(IDXGIResource1), (void**)&m_sharedTextureResource));
        DX_CHECK(m_sharedTextureResource->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr, &m_sharedTextureHandle));

        D3D11_VIEWPORT viewport;
        ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));

        viewport.Width = width;
        viewport.Height = height;
        m_deviceContex->RSSetViewports(1, &viewport);

        if (m_postProcessing.has_value()) {
            m_postProcessing->resize(m_sharedTextureHandle, width, height);
        } else {
            m_postProcessing = std::move(PostProcessing(m_sharedTextureHandle, true, width, height, m_gpuIndices.vk, m_paths.postprocessingGlsl));
        }
        if (m_hybridproRenderer.get()) {
            m_hybridproRenderer->resize(width, height);
        } else {
            m_hybridproRenderer = std::make_unique<HybridProRenderer>(m_paths, width, height, m_gpuIndices);
        }
        float focalLength = m_hybridproRenderer->getFocalLength() / 1000.0f;
        m_postProcessing->setToneMapFocalLength(focalLength);
        m_width = width;
        m_height = height;
    }
}

void Dx11App::onResize(GLFWwindow* window, int width, int height)
{
    auto app = static_cast<Dx11App*>(glfwGetWindowUserPointer(window));
    app->resize(width, height);
}

void Dx11App::mainLoop()
{
    clock_t deltaTime = 0;
    unsigned int frames = 0;
    while (!glfwWindowShouldClose(m_window)) {
        const rpr_uint renderedIterations = 1;
        clock_t beginFrame = clock();
        {
            glfwPollEvents();
            m_hybridproRenderer->render(renderedIterations);
            m_postProcessing->updateAovColor(m_hybridproRenderer->getAov(RPR_AOV_COLOR));
            m_postProcessing->updateAovOpacity(m_hybridproRenderer->getAov(RPR_AOV_OPACITY));
            m_postProcessing->updateAovShadowCatcher(m_hybridproRenderer->getAov(RPR_AOV_SHADOW_CATCHER));
            m_postProcessing->updateAovReflectionCatcher(m_hybridproRenderer->getAov(RPR_AOV_REFLECTION_CATCHER));
            m_postProcessing->updateAovMattePass(m_hybridproRenderer->getAov(RPR_AOV_MATTE_PASS));
            m_postProcessing->updateAovBackground(m_hybridproRenderer->getAov(RPR_AOV_BACKGROUND));
            m_postProcessing->run();

            IDXGIKeyedMutex* km;
            DX_CHECK(m_sharedTextureResource->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)&km));
            DX_CHECK(km->AcquireSync(0, INFINITE));
            m_deviceContex->CopyResource(m_backBuffer.Get(), m_sharedTexture.Get());
            DX_CHECK(km->ReleaseSync(0));
            DX_CHECK(m_swapChain->Present(1, 0));
        }
        clock_t endFrame = clock();
        deltaTime += endFrame - beginFrame;
        frames += renderedIterations;
        double deltaTimeInSeconds = (deltaTime / (double)CLOCKS_PER_SEC);
        if (deltaTimeInSeconds > 1.0) { // every second
            std::cout << "Iterations per second = "
                      << frames
                      << ", Time per iteration = "
                      << deltaTimeInSeconds * 1000.0 / frames
                      << "ms"
                      << std::endl;
            frames = 0;
            deltaTime -= CLOCKS_PER_SEC;
        }
    }
}