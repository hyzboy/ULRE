IF(UNIX)
    SET(RENDER_WINDOW_SOURCE  XCBWindow.cpp)
    SET(RENDER_WINDOW_LIBRARY   xcb)
ENDIF()
if(WIN32)
    SET(RENDER_WINDOW_SOURCE  Win32Window.cpp)
ENDIF()
if(APPLE)
ENDIF()


add_executable(VulkanTest   main.cpp
                            ${RENDER_WINDOW_SOURCE}
                            VKInstance.cpp
                            VKPhysicalDevice.cpp
                            VKDevice.cpp
                            VKCommandBuffer.cpp)

target_link_libraries(VulkanTest PRIVATE ${ULRE} ${VULKAN_LIB} ${RENDER_WINDOW_LIBRARY})
