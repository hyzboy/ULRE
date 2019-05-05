SET(RENDER_DEVICE_VULKAN_HEADER ${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VK.h
								${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKBuffer.h
								${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKBufferData.h
								${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKCommandBuffer.h
								${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKDescriptorSets.h
								${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKDevice.h
								${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKDeviceAttribute.h
								${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKFence.h
								${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKFormat.h
								${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKFramebuffer.h
								${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKImageView.h
								${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKInstance.h
								${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKMaterial.h
								${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKPhysicalDevice.h
								${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKPipeline.h
								${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKPrimivate.h
								${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKRenderable.h
								${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKRenderPass.h
								${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKSemaphore.h
								${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKShaderModule.h
								${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKShaderModuleManage.h
								${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKShaderParse.h
								${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKSurfaceExtensionName.h
								${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKVertexAttributeBinding.h)

SET(RENDER_DEVICE_VULKAN_SOURCE VKFormat.cpp
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
								VKDescriptorSetLayoutCreater.cpp
								VKDescriptorSetLayoutCreater.h
                                VKRenderPass.cpp
                                VKShaderModule.cpp
                                VKShaderModuleManage.cpp
                                VKVertexAttributeBinding.cpp
                                VKPipeline.cpp
                                VKSemaphore.cpp
                                VKFramebuffer.cpp
                                VKFence.cpp
                                VKMaterial.cpp
                                VKRenderable.cpp)

#SET(RENDER_DEVICE_VULKAN_TOML_SOURCE toml/VKPipelineCreateInfo.TOML.cpp)
#SET(RENDER_DEVICE_VULKAN_JSON_SOURCE json/VKPipelineCreateInfo.JSON.cpp)

SOURCE_GROUP("Header Files" FILES ${RENDER_DEVICE_VULKAN_HEADER})
SOURCE_GROUP("Source Files" FILES ${RENDER_DEVICE_VULKAN_SOURCE})
#SOURCE_GROUP("JSON Source Files" FILES ${RENDER_DEVICE_VULKAN_JSON_SOURCE})

add_library(ULRE.RenderDevice.Vulkan STATIC ${RENDER_DEVICE_VULKAN_HEADER} 
											${RENDER_DEVICE_VULKAN_SOURCE}
											${RENDER_DEVICE_VULKAN_JSON_SOURCE})
