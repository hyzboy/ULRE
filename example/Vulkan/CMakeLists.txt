IF(UNIX)
    SET(RENDER_WINDOW_SOURCE  XCBWindow.cpp)
    SET(RENDER_WINDOW_LIBRARY xcb)
ELSEIF(WIN32)
    SET(RENDER_WINDOW_SOURCE  WinWindow.cpp)
ENDIF()

SET(VULKAN_TEST_SOURCE_FILES main.cpp
                             ${RENDER_WINDOW_SOURCE}
                             VKInstance.cpp
                             VKCommandBuffer.cpp
                             RenderSurfaceAttribute.cpp
                             RenderSurfaceCreater.cpp
							 RenderSurface.cpp
							 VKBuffer.cpp)

SET(VULKAN_TEST_HEADER_FILES	VK.h
								VKInstance.h
								VKSurfaceExtensionName.h
								RenderSurfaceAttribute.h
								RenderSurface.h
								VKBuffer.h
								Window.h)

SOURCE_GROUP("Header Files" FILES ${VULKAN_TEST_HEADER_FILES})
SOURCE_GROUP("Source Files" FILES ${VULKAN_TEST_SOURCE_FILES})

add_executable(VulkanTest ${VULKAN_TEST_HEADER_FILES} ${VULKAN_TEST_SOURCE_FILES})

target_link_libraries(VulkanTest PRIVATE ${ULRE} ${VULKAN_LIB} ${RENDER_WINDOW_LIBRARY})
