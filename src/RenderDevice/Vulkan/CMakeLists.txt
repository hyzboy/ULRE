SET(RENDER_DEVICE_VULKAN_HEADER ${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VK.h
								${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKMemory.h
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
                                ${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKRenderTarget.h
                                ${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKSemaphore.h
                                ${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKShaderResource.h
                                ${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKShaderModule.h
                                ${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKShaderModuleManage.h
                                ${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKSurfaceExtensionName.h
								${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKSwapchain.h
                                ${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKVertexAttributeBinding.h
                                ${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKTexture.h
                                ${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/VKSampler.h)

SET(RENDER_DEVICE_VULKAN_SOURCE VKFormat.cpp
                                VKMemory.cpp
                                VKInstance.cpp
                                VKPhysicalDevice.cpp
                                VKImageView.cpp
                                VKCommandBuffer.cpp
                                VKDeviceAttribute.cpp
                                VKDeviceCreater.cpp
                                VKDevice.cpp
                                VKDeviceBuffer.cpp
                                VKDeviceTexture.cpp
								VKDeviceRenderPass.cpp
                                VKBuffer.cpp
                                VKDescriptorSets.cpp
                                VKDescriptorSetLayoutCreater.cpp
                                VKDescriptorSetLayoutCreater.h
                                VKRenderPass.cpp
								VKRenderTarget.cpp
                                VKShaderParse.h
                                VKShaderModule.cpp
                                VKShaderModuleManage.cpp
                                VKVertexAttributeBinding.cpp
                                VKPipeline.cpp
                                VKSemaphore.cpp
                                VKFramebuffer.cpp
                                VKFence.cpp
                                VKMaterial.cpp
                                VKRenderable.cpp
                                VKTexture.cpp
                                VKSampler.cpp
								VKSwapchain.cpp
								)

SET(RENDER_DEVICE_VULKAN_POD_SOURCE pod/VKPipelineCreateInfo.POD.cpp)

SOURCE_GROUP("Header Files" FILES ${RENDER_DEVICE_VULKAN_HEADER})
SOURCE_GROUP("Source Files" FILES ${RENDER_DEVICE_VULKAN_SOURCE})
SOURCE_GROUP("POD Source Files" FILES ${RENDER_DEVICE_VULKAN_POD_SOURCE})

add_library(ULRE.RenderDevice.Vulkan STATIC ${RENDER_DEVICE_VULKAN_HEADER}
                                            ${RENDER_DEVICE_VULKAN_SOURCE}
                                            ${RENDER_DEVICE_VULKAN_POD_SOURCE})
