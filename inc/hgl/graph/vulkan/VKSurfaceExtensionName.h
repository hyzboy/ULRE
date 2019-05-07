#pragma once

#include<hgl/graph/vulkan/VK.h>

#ifdef __ANDROID__
    #include<vulkan/vulkan_android.h>
    #define HGL_VK_SURFACE_EXTENSION_NAME VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
#elif defined(_WIN32)
    #include<Windows.h>
    #include<vulkan/vulkan_win32.h>
    #define HGL_VK_SURFACE_EXTENSION_NAME VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#elif defined(VK_USE_PLATFORM_IOS_MVK)
    #include<vulkan/vulkan_ios.h>
    #define HGL_VK_SURFACE_EXTENSION_NAME VK_MVK_IOS_SURFACE_EXTENSION_NAME
#elif defined(VK_USE_PLATFORM_MACOS_MVK)
    #include<vulkan/vulkan_macos.h>
    #define HGL_VK_SURFACE_EXTENSION_NAME VK_MVK_MACOS_SURFACE_EXTENSION_NAME
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    #include<vulkan/vulkan_wayland.h>
    #define HGL_VK_SURFACE_EXTENSION_NAME VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME
#else
    #include<xcb/xcb.h>
    #include<xcb/xcb_atom.h>
    #include<vulkan/vulkan_xcb.h>
    #define HGL_VK_SURFACE_EXTENSION_NAME VK_KHR_XCB_SURFACE_EXTENSION_NAME
#endif
