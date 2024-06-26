#pragma once

#include <assert.h>
#include <filesystem>
#include <iostream>
#include <winerror.h>

#define DX_CHECK(f)                                                                         \
    {                                                                                       \
        HRESULT res = (f);                                                                  \
        if (!SUCCEEDED(res)) {                                                              \
            printf("Fatal : DX HRESULT is %d in %s at line %d\n", res, __FILE__, __LINE__); \
            _com_error err(res);                                                            \
            printf("messsage : %s\n", err.ErrorMessage());                                  \
            assert(false);                                                                  \
        }                                                                                   \
    }

struct DeviceInfo {
    int index;
    std::string name;
    std::vector<uint8_t> LUID;
    bool supportHardwareRT;
    bool supportGpuDenoiser;
};

struct Paths {
    std::filesystem::path hybridproDll;
    std::filesystem::path hybridproCacheDir;
    std::filesystem::path assetsDir;
};