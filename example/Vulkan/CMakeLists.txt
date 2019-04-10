IF(UNIX)
    SET(RENDER_WINDOW_SOURCE  XCBWindow.cpp)
    SET(RENDER_WINDOW_LIBRARY xcb)
ELSEIF(WIN32)
    SET(RENDER_WINDOW_SOURCE  WinWindow.cpp)
ENDIF()

add_executable(VulkanTest   main.cpp
                            ${RENDER_WINDOW_SOURCE}
                            VKInstance.cpp
                            VKPhysicalDevice.cpp
                            VKDevice.cpp
                            VKCommandBuffer.cpp)

target_link_libraries(VulkanTest PRIVATE ${ULRE} ${VULKAN_LIB} ${RENDER_WINDOW_LIBRARY})
