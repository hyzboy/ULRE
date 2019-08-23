file(GLOB RENDER_DEVICE_VULKAN_HEADER ${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/*.h)

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

set_property(TARGET ULRE.RenderDevice.Vulkan PROPERTY FOLDER "ULRE")
