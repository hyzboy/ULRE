IF(UNIX)
    SET(RENDER_WINDOW_SOURCE  XCBWindow.cpp)
    SET(RENDER_WINDOW_LIBRARY xcb)
ELSEIF(WIN32)
    SET(RENDER_WINDOW_SOURCE  WinWindow.cpp)
ENDIF()

SET(VULKAN_TEST_SOURCE_FILES	main.cpp
								AssetsManage.cpp
								${RENDER_WINDOW_SOURCE}
								VKFormat.cpp
								VKInstance.cpp
								VKPhysicalDevice.cpp
								VKImageView.cpp
								VKCommandBuffer.cpp
								VKDeviceAttribute.cpp
								VKDeviceCreater.cpp
								VKDevice.cpp
								VKDeviceBuffer.cpp
								VKBuffer.cpp
								VKDescriptorSets.cpp
								VKPipelineLayout.cpp
								VKRenderPass.cpp
								VKShaderModule.cpp
								VKShaderModuleManage.cpp
								VKVertexAttributeBinding.cpp
								VKPipeline.cpp
								VKSemaphore.cpp
								VKFramebuffer.cpp
								VKFence.cpp
								VKMaterial.cpp
								VKRenderable.cpp
							 )

SET(VULKAN_TEST_HEADER_FILES	AssetsManage.h
								VK.h
                                VKFormat.h
								VKPrimivate.h
								VKInstance.h
								VKPhysicalDevice.h
								VKImageView.h
								VKCommandBuffer.h
								VKSurfaceExtensionName.h
								VKDeviceAttribute.h
								VKDevice.h
								VKBuffer.h
								VKDescriptorSets.h
								VKPipelineLayout.h
								VKRenderPass.h
								VKShaderModule.h
								VKShaderModuleManage.h
								VKVertexAttributeBinding.h
								VKSemaphore.h
								VKPipeline.h
								VKFramebuffer.h
								VKFence.h
								VKMaterial.h
								VKRenderable.h
								Window.h)

SET(SHADER_FILES	shader_compile.bat
					FlatColor.vert
					FlatColor.frag)

SOURCE_GROUP("Header Files" FILES ${VULKAN_TEST_HEADER_FILES})
SOURCE_GROUP("Source Files" FILES ${VULKAN_TEST_SOURCE_FILES})
SOURCE_GROUP("Shader Files" FILES ${SHADER_FILES})

add_executable(VulkanTest ${VULKAN_TEST_HEADER_FILES} ${VULKAN_TEST_SOURCE_FILES} ${SHADER_FILES})

target_link_libraries(VulkanTest PRIVATE ${ULRE} ${VULKAN_LIB} ${RENDER_WINDOW_LIBRARY} spirv-cross-core)
