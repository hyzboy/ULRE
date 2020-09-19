set(RD_INCLUDE_PATH     ${ROOT_INCLUDE_PATH}/hgl/graph/vulkan)

SET(VK_DB_SOURCE        ${RD_INCLUDE_PATH}/VKDatabase.h
                        VKDatabase.cpp
                        VKDatabaseMaterial.cpp)

SOURCE_GROUP("Database" FILES ${VK_DB_SOURCE})

SET(VK_INST_SOURCE      ${RD_INCLUDE_PATH}/VKInstance.h
                        VKInstance.cpp)

SET(VK_DEBUG_SOURCE     ${RD_INCLUDE_PATH}/VKDebugOut.h
                        VKDebugOut.cpp)

SET(VK_MEMORY_SOURCE    ${RD_INCLUDE_PATH}/VKMemory.h
                        ${RD_INCLUDE_PATH}/VKBuffer.h
                        VKMemory.cpp
                        VKBuffer.cpp)

SET(VK_DEVICE_SOURCE    ${RD_INCLUDE_PATH}/VKDevice.h
                        ${RD_INCLUDE_PATH}/VKDeviceAttribute.h
                        VKDeviceAttribute.cpp
                        VKDeviceCreater.cpp
                        VKDevice.cpp
                        VKDeviceBuffer.cpp
                        VKDeviceImage.cpp
                        VKDeviceTexture.cpp
                        VKDeviceSwapchain.cpp
                        VKDeviceRenderPass.cpp)

SET(VK_PHYSICAL_DEVICE_SOURCE ${RD_INCLUDE_PATH}/VKPhysicalDevice.h
                              VKPhysicalDevice.cpp
                              VKProperties.cpp)

SOURCE_GROUP("Device"                   FILES ${VK_DEVICE_SOURCE})
SOURCE_GROUP("Device\\Debug"            FILES ${VK_DEBUG_SOURCE})
SOURCE_GROUP("Device\\Instance"         FILES ${VK_INST_SOURCE})
SOURCE_GROUP("Device\\Physical Device"  FILES ${VK_PHYSICAL_DEVICE_SOURCE})
SOURCE_GROUP("Device\\Memory"           FILES ${VK_MEMORY_SOURCE})

SET(VK_DESCRIPTOR_SETS_SOURCE   ${RD_INCLUDE_PATH}/VKDescriptorSets.h
                                VKDescriptorSets.cpp
                                VKDescriptorSetLayoutCreater.cpp
                                VKDescriptorSetLayoutCreater.h)

SOURCE_GROUP("Descriptor Sets" FILES ${VK_DESCRIPTOR_SETS_SOURCE})

SET(VK_SHADER_SOURCE    ${RD_INCLUDE_PATH}/VKShaderModule.h
                        ${RD_INCLUDE_PATH}/ShaderModuleMap.h
                        ShaderModuleMap.cpp
                        VKShaderModule.cpp)

SET(VK_TEXTURE_SOURCE   ${RD_INCLUDE_PATH}/VKImageView.h
                        ${RD_INCLUDE_PATH}/VKTexture.h
                        ${RD_INCLUDE_PATH}/VKSampler.h
                        VKSampler.cpp
                        VKImageView.cpp
                        VKTexture.cpp
                       	VKTextureLoader.cpp)

SET(VK_MATERIAL_SOURCE  ${RD_INCLUDE_PATH}/VKMaterial.h
                        ${RD_INCLUDE_PATH}/VKMaterialInstance.h
                        VKMaterial.cpp
                        VKMaterialInstance.cpp)

SOURCE_GROUP("Material" FILES ${VK_MATERIAL_SOURCE})
SOURCE_GROUP("Material\\Texture" FILES ${VK_TEXTURE_SOURCE})
SOURCE_GROUP("Material\\Shader" FILES ${VK_SHADER_SOURCE})

SET(VK_RENDER_PASS_SOURCE   ${RD_INCLUDE_PATH}/VKFramebuffer.h
                            ${RD_INCLUDE_PATH}/VKPipeline.h
                            ${RD_INCLUDE_PATH}/VKRenderPass.h
                            ${RD_INCLUDE_PATH}/VKRenderTarget.h
                            ${RD_INCLUDE_PATH}/VKSwapchain.h
                            ${RD_INCLUDE_PATH}/VKCommandBuffer.h
                            ${RD_INCLUDE_PATH}/VKSemaphore.h
                            ${RD_INCLUDE_PATH}/VKFence.h
                            VKSemaphore.cpp
                            VKFence.cpp
                            VKFramebuffer.cpp
                            VKPipeline.cpp
                            VKRenderPass.cpp
                            VKRenderTarget.cpp
                            VKSwapchain.cpp
                            VKCommandBuffer.cpp)

SOURCE_GROUP("Render Pass" FILES ${VK_RENDER_PASS_SOURCE})

SET(VK_RENDERABLE_SOURCE    ${RD_INCLUDE_PATH}/VKVertexAttributeBinding.h
                            ${RD_INCLUDE_PATH}/VKRenderable.h
                            VKVertexAttributeBinding.cpp
                            VKRenderable.cpp
                            VKTileData.cpp
                            VKTileFont.cpp)

SOURCE_GROUP("Renderable" FILES ${VK_RENDERABLE_SOURCE})

IF(WIN32)
    OPTION(FORCE_DISCETE_GPU    "Force Discrete GPU" OFF)

    IF(FORCE_DISCETE_GPU)
        SET(RENDER_DEVICE_SOURCE ForceDiscreteGPU.c)
    ENDIF()
ENDIF(WIN32)

add_cm_library(ULRE.RenderDevice.Vulkan "ULRE"  ${VK_DB_SOURCE}
                                                ${VK_INST_SOURCE}
                                                ${VK_DEBUG_SOURCE}
                                                ${VK_MEMORY_SOURCE}
                                                ${VK_DEVICE_SOURCE}
                                                ${VK_PHYSICAL_DEVICE_SOURCE}
                                                ${VK_DESCRIPTOR_SETS_SOURCE}
                                                ${VK_SHADER_SOURCE}
                                                ${VK_TEXTURE_SOURCE}
                                                ${VK_MATERIAL_SOURCE}
                                                ${VK_RENDER_PASS_SOURCE}
                                                ${VK_RENDERABLE_SOURCE}
                                                ${VK_RENDER_DEVICE_SOURCE})
