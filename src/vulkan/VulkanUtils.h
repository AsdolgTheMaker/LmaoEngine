#pragma once
#include <volk.h>
#include "core/Log.h"

#define VK_CHECK(call)                                                         \
    do {                                                                        \
        VkResult _r = (call);                                                   \
        if (_r != VK_SUCCESS) {                                                 \
            LMAO_FATAL("Vulkan error %d at %s:%d", (int)_r, __FILE__, __LINE__);\
        }                                                                       \
    } while (0)

namespace lmao {

inline const char* vkResultStr(VkResult r) {
    switch (r) {
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        default: return "UNKNOWN";
    }
}

} // namespace lmao
