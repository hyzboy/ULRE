IF(UNIX)
    SET(RENDER_WINDOW_SOURCE  XCBWindow.cpp)
    SET(RENDER_WINDOW_LIBRARY xcb)
ELSEIF(WIN32)
    SET(RENDER_WINDOW_SOURCE  WinWindow.cpp)
ENDIF()

SET(VULKAN_TEST_SOURCE_FILES	main.cpp
								${RENDER_WINDOW_SOURCE}
								VkFormat.cpp
								VKInstance.cpp
								VKPhysicalDevice.cpp
								VKCommandBuffer.cpp
								RenderSurfaceAttribute.cpp
								RenderSurfaceCreater.cpp
								RenderSurface.cpp
								VKBuffer.cpp
#							 VKPipelineLayout.cpp
#							 VKDescriptorSet.cpp
								VKRenderPass.cpp
								VKShader.cpp
								VKVertexInput.cpp
								VKSemaphore.cpp
							 )

SET(VULKAN_TEST_HEADER_FILES	VK.h
								VKInstance.h
								VKPhysicalDevice.h
								VKCommandBuffer.h
								VKSurfaceExtensionName.h
								RenderSurfaceAttribute.h
								RenderSurface.h
								VKBuffer.h
#								VKPipelineLayout.h
#								VKDescriptorSet.h
								VKRenderPass.h
								VKShader.h
								VKVertexInput.h
								VKSemaphore.h
								Window.h)

SET(SHADER_FILES	shader_compile.bat
					FlatColor.vert 
					FlatColor.frag)

SOURCE_GROUP("Header Files" FILES ${VULKAN_TEST_HEADER_FILES})
SOURCE_GROUP("Source Files" FILES ${VULKAN_TEST_SOURCE_FILES})
SOURCE_GROUP("Shader Files" FILES ${SHADER_FILES})

add_executable(VulkanTest ${VULKAN_TEST_HEADER_FILES} ${VULKAN_TEST_SOURCE_FILES} ${SHADER_FILES})

target_link_libraries(VulkanTest PRIVATE ${ULRE} ${VULKAN_LIB} ${RENDER_WINDOW_LIBRARY})
