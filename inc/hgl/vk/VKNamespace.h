#pragma once

#if defined(__has_include)
    #if __has_include(<vulkan/vulkan.h>)
        #include <vulkan/vulkan.h>
        #define HGL_VULKAN_AVAILABLE 1
    #else
        /* Vulkan header not found on include path. Disable Vulkan-related code. */
        #define HGL_VULKAN_AVAILABLE 0
    #endif
#else
    /* __has_include not supported: assume Vulkan is unavailable. */
    #define HGL_VULKAN_AVAILABLE 0
#endif

