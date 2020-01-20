file(GLOB RENDER_DEVICE_VULKAN_HEADER ${ROOT_INCLUDE_PATH}/hgl/graph/vulkan/*.h)

SET(VK_DEVICE_SOURCE    VKDeviceAttribute.cpp
                        VKDeviceCreater.cpp
                        VKDevice.cpp
                        VKDeviceBuffer.cpp
                        VKDeviceImage.cpp
                        VKDeviceTexture.cpp
                        VKDeviceSwapchain.cpp
                        VKDeviceRenderPass.cpp)

SET(VK_DESCRIPTOR_SETS_SOURCE   VKDescriptorSets.cpp
                                VKDescriptorSetLayoutCreater.cpp
                                VKDescriptorSetLayoutCreater.h)

SET(VK_SHADER_SOURCE    VKShaderParse.h
                        VKShaderModule.cpp
                        VKShaderModuleManage.cpp)

SET(VK_MATERIAL_SOURCE  VKImageView.cpp
                        VKTexture.cpp
                        VKMaterial.cpp
                        VKMaterialInstance.cpp)

SET(VK_RENDER_PASS_SOURCE   VKFramebuffer.cpp
                            VKPipeline.cpp
                            VKRenderPass.cpp
                            VKRenderTarget.cpp
                            VKSwapchain.cpp
                            VKCommandBuffer.cpp)

SET(RENDER_DEVICE_VULKAN_SOURCE VKFormat.cpp
                                VKMemory.cpp
                                VKProperties.cpp
                                VKDebugOut.cpp
                                VKInstance.cpp
                                VKPhysicalDevice.cpp
                                VKBuffer.cpp
                                VKVertexAttributeBinding.cpp
                                VKSemaphore.cpp
                                VKFence.cpp
                                VKRenderable.cpp
                                VKSampler.cpp)

SET(RENDER_DEVICE_VULKAN_POD_SOURCE pod/VKPipelineCreateInfo.POD.cpp
                                    pod/VKTextureLoader.cpp)

SOURCE_GROUP("Device" FILES ${VK_DEVICE_SOURCE})
SOURCE_GROUP("Descriptor Sets" FILES ${VK_DESCRIPTOR_SETS_SOURCE})

SOURCE_GROUP("Material\\Shader" FILES ${VK_SHADER_SOURCE})
SOURCE_GROUP("Material" FILES ${VK_MATERIAL_SOURCE})

SOURCE_GROUP("Render Pass" FILES ${VK_RENDER_PASS_SOURCE})

SOURCE_GROUP("Header Files" FILES ${RENDER_DEVICE_VULKAN_HEADER})
SOURCE_GROUP("Source Files" FILES ${RENDER_DEVICE_VULKAN_SOURCE})
SOURCE_GROUP("POD Source Files" FILES ${RENDER_DEVICE_VULKAN_POD_SOURCE})

IF(WIN32)
    OPTION(FORCE_DISCETE_GPU    "Force Discrete GPU" OFF)

    IF(FORCE_DISCETE_GPU)
        SET(RENDER_DEVICE_SOURCE ForceDiscreteGPU.c)
    ENDIF()
ENDIF(WIN32)

add_cm_library(ULRE.RenderDevice.Vulkan "ULRE"  ${RENDER_DEVICE_SOURCE}
                                                ${RENDER_DEVICE_VULKAN_HEADER}
                                                ${RENDER_DEVICE_VULKAN_SOURCE}
                                                ${RENDER_DEVICE_VULKAN_POD_SOURCE}

                                                ${VK_DEVICE_SOURCE}
                                                ${VK_DESCRIPTOR_SETS_SOURCE}
                                                ${VK_SHADER_SOURCE}
                                                ${VK_MATERIAL_SOURCE}
                                                ${VK_RENDER_PASS_SOURCE}
                                                ${RENDER_DEVICE_VULKAN_SOURCE})
