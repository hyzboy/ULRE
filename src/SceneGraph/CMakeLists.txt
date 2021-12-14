set(SG_INCLUDE_PATH     ${ROOT_INCLUDE_PATH}/hgl/graph)

SET(SG_TEXTURE_SOURCE   ${SG_INCLUDE_PATH}/TextureLoader.h
                        ${SG_INCLUDE_PATH}/Bitmap2DLoader.h
                        TextureLoader.cpp
                        Bitmap2DLoader.cpp)

SOURCE_GROUP("Texture" FILES ${SG_TEXTURE_SOURCE})

SET(SG_VAD_SOURCE   ${SG_INCLUDE_PATH}/VertexAttribData.h
                    ${SG_INCLUDE_PATH}/VertexAttribDataAccess.h
                    VertexAttribData.cpp)

SOURCE_GROUP("VertexAttribData" FILES ${SG_VAD_SOURCE})

SET(TILE_SOURCE ${SG_INCLUDE_PATH}/TileData.h
                TileData.cpp)

SOURCE_GROUP("Tile" FILES ${TILE_SOURCE})

SET(SCENE_GRAPH_HEADER  ${SG_INCLUDE_PATH}/Light.h
                        ${SG_INCLUDE_PATH}/SceneInfo.h
                        ${SG_INCLUDE_PATH}/SceneNode.h
                        ${SG_INCLUDE_PATH}/RenderNode.h
                        ${SG_INCLUDE_PATH}/SceneOrient.h
                        ${SG_INCLUDE_PATH}/RenderList.h
                        ${SG_INCLUDE_PATH}/InlineGeometry.h
                        #${SG_INCLUDE_PATH}/Mesh.h
                        #${SG_INCLUDE_PATH}/Material.h
                        #${SG_INCLUDE_PATH}/Spline.h
                        )

SET(SCENE_GRAPH_SOURCE  RenderList.cpp
                        SceneNode.cpp
                        SceneOrient.cpp
                        InlineGeometry.cpp
                        #InlinePipeline.cpp
                        #Material.cpp
                        #Mesh.cpp
                        #SceneFile.cpp
                        )

SET(RENDERABLE_FILES    ${SG_INCLUDE_PATH}/RenderableCreater.h
                        RenderableCreater.cpp)

SOURCE_GROUP("Renderable" FILES ${RENDERABLE_FILES})

SET(FONT_MANAGE_SOURCE  ${SG_INCLUDE_PATH}/font/Font.h
                        ${SG_INCLUDE_PATH}/font/FontManage.h
                        font/Font.cpp)

SET(FONT_SOURCE ${SG_INCLUDE_PATH}/font/FontSource.h
                font/FontSource.cpp
                font/FontSourceSingle.cpp
                font/FontSourceMulti.cpp
                font/FontSourceManage.cpp)

SET(TILE_FONT_SOURCE    ${SG_INCLUDE_PATH}/font/TileFont.h
                        font/TileFont.cpp)

SET(FONT_LAYOUT_SOURCE  ${SG_INCLUDE_PATH}/font/TextLayout.h
                        font/TextLayout.cpp)

SET(TEXT_RENDERABLE_SOURCE  ${SG_INCLUDE_PATH}/font/TextRenderable.h
                            font/TextRenderable.cpp)

SOURCE_GROUP("Font"             FILES ${FONT_MANAGE_SOURCE})
SOURCE_GROUP("Font\\Source"     FILES ${FONT_SOURCE})
SOURCE_GROUP("Font\\TileFont"   FILES ${TILE_FONT_SOURCE})
SOURCE_GROUP("Font\\Layout"     FILES ${FONT_LAYOUT_SOURCE} ${TEXT_RENDERABLE_SOURCE})

IF(WIN32)
    SET(FONT_SOURCE_OS  font/FontSourceWin.cpp
                        font/FontSourceWin.h)

    SOURCE_GROUP("Font\\Source\\Windows" FILES ${FONT_SOURCE_OS})
ENDIF(WIN32)

SOURCE_GROUP("Header Files" FILES ${SCENE_GRAPH_HEADER})
SOURCE_GROUP("Source Files" FILES ${SCENE_GRAPH_SOURCE})

SET(VK_RR_SOURCE        ${SG_INCLUDE_PATH}/VKRenderResource.h
                        Vulkan/VKRenderResource.cpp
                        Vulkan/VKRenderResourceMaterial.cpp)

SOURCE_GROUP("Vulkan\\RenderResource" FILES ${VK_RR_SOURCE})

SET(VK_RR_SHADER_FILES  ${SG_INCLUDE_PATH}/VKShaderResource.h
                        Vulkan/VKShaderResource.cpp)

SOURCE_GROUP("Vulkan\\RenderResource\\Shader" FILES ${VK_RR_SHADER_FILES})

SET(VK_RR_MATERIAL_FILES    ${SG_INCLUDE_PATH}/VKMaterialDescriptorSets.h
                            Vulkan/VKMaterialDescriptorSets.cpp)

SOURCE_GROUP("Vulkan\\RenderResource\\Material" FILES   ${VK_RR_MATERIAL_FILES})

SET(VK_INST_SOURCE      ${SG_INCLUDE_PATH}/VKInstance.h
                        Vulkan/VKInstance.cpp)

SET(VK_DEBUG_SOURCE     ${SG_INCLUDE_PATH}/VKDebugOut.h
                        ${SG_INCLUDE_PATH}/VKDebugMaker.h
                        Vulkan/VKDebugOut.cpp
                        Vulkan/VKDebugMaker.cpp)

SET(VK_MEMORY_SOURCE    ${SG_INCLUDE_PATH}/VKMemory.h
                        ${SG_INCLUDE_PATH}/VKMemoryAllocator.h
                        ${SG_INCLUDE_PATH}/VKBuffer.h
                        ${SG_INCLUDE_PATH}/VKIndexBuffer.h
                        ${SG_INCLUDE_PATH}/VKArrayBuffer.h
                        Vulkan/VKMemory.cpp
                        Vulkan/VKMemoryAllocator.cpp
                        Vulkan/VKBuffer.cpp
                        )
                        
SET(VK_DEVICE_SOURCE    ${SG_INCLUDE_PATH}/VKDevice.h
                        ${SG_INCLUDE_PATH}/VKDeviceAttribute.h
                        Vulkan/VKDeviceAttribute.cpp
                        Vulkan/VKDeviceCreater.cpp
                        Vulkan/VKDevice.cpp
                        Vulkan/VKDeviceBuffer.cpp
                        Vulkan/VKDeviceImage.cpp
                        Vulkan/VKDeviceTexture.cpp
                        Vulkan/VKDeviceMaterial.cpp
                        Vulkan/VKDeviceFramebuffer.cpp
                        Vulkan/VKDeviceSwapchain.cpp
                        Vulkan/VKDeviceRenderPass.cpp
                        Vulkan/VKDeviceRenderPassManage.cpp
                        Vulkan/VKDeviceRenderTarget.cpp)

SET(VK_PHYSICAL_DEVICE_SOURCE ${SG_INCLUDE_PATH}/VKPhysicalDevice.h
                            Vulkan/VKPhysicalDevice.cpp
                            Vulkan/VKProperties.cpp)

SOURCE_GROUP("Vulkan\\Device"                   FILES ${VK_DEVICE_SOURCE})
SOURCE_GROUP("Vulkan\\Device\\Debug"            FILES ${VK_DEBUG_SOURCE})
SOURCE_GROUP("Vulkan\\Device\\Instance"         FILES ${VK_INST_SOURCE})
SOURCE_GROUP("Vulkan\\Device\\Physical Device"  FILES ${VK_PHYSICAL_DEVICE_SOURCE})
SOURCE_GROUP("Vulkan\\Device\\Memory"           FILES ${VK_MEMORY_SOURCE})

SET(VK_DESCRIPTOR_SETS_SOURCE   ${SG_INCLUDE_PATH}/VKDescriptorSets.h
                                Vulkan/VKDescriptorSets.cpp
                                Vulkan/VKPipelineLayoutData.h                                
                                Vulkan/VKPipelineLayoutData.cpp)

SOURCE_GROUP("Vulkan\\Descriptor Sets" FILES ${VK_DESCRIPTOR_SETS_SOURCE})

SET(VK_SHADER_SOURCE    ${SG_INCLUDE_PATH}/VKShaderModule.h
                        ${SG_INCLUDE_PATH}/VKShaderModuleMap.h
                        Vulkan/VKShaderModuleMap.cpp
                        Vulkan/VKShaderModule.cpp)

SET(VK_TEXTURE_SOURCE   ${SG_INCLUDE_PATH}/VKImageView.h
                        ${SG_INCLUDE_PATH}/VKTexture.h
                        ${SG_INCLUDE_PATH}/VKTextureCreateInfo.h
                        ${SG_INCLUDE_PATH}/VKSampler.h
                        Vulkan/VKSampler.cpp
                        Vulkan/VKImageView.cpp
                        Vulkan/VKTexture.cpp
                        Vulkan/VKTexture2DLoader.cpp)

SET(VK_MATERIAL_SOURCE  ${SG_INCLUDE_PATH}/VKMaterial.h
                        ${SG_INCLUDE_PATH}/VKMaterialParameters.h
                        ${SG_INCLUDE_PATH}/VKMaterialInstance.h
                        Vulkan/VKMaterial.cpp
                        Vulkan/VKMaterialParameters.cpp
                        Vulkan/VKMaterialInstance.cpp)

SOURCE_GROUP("Vulkan\\Material" FILES ${VK_MATERIAL_SOURCE})
SOURCE_GROUP("Vulkan\\Material\\Texture" FILES ${VK_TEXTURE_SOURCE})
SOURCE_GROUP("Vulkan\\Material\\Shader" FILES ${VK_SHADER_SOURCE})

SET(VK_RENDER_CONTEXT_SOURCE    ${SG_INCLUDE_PATH}/VKRenderContext.h
                                ${SG_INCLUDE_PATH}/VKSubpass.h
                                Vulkan/VKSubpass.cpp
                                Vulkan/VKRenderContext.cpp)

SET(VK_RENDER_PASS_SOURCE   ${SG_INCLUDE_PATH}/VKFramebuffer.h
                            ${SG_INCLUDE_PATH}/VKPipeline.h
                            ${SG_INCLUDE_PATH}/VKRenderPass.h
                            ${SG_INCLUDE_PATH}/VKRenderTarget.h
                            ${SG_INCLUDE_PATH}/VKSwapchain.h
                            ${SG_INCLUDE_PATH}/VKSemaphore.h
                            ${SG_INCLUDE_PATH}/VKFence.h
                            Vulkan/VKQueue.cpp
                            Vulkan/VKSemaphore.cpp
                            Vulkan/VKFence.cpp
                            Vulkan/VKFramebuffer.cpp
                            Vulkan/VKPipeline.cpp
                            Vulkan/VKPipelineCache.cpp
                            #Vulkan/VKSubpass.cpp
                            Vulkan/VKRenderPass.cpp
                            Vulkan/VKRenderTarget.cpp
                            Vulkan/VKSwapchainRenderTarget.cpp
                            Vulkan/VKSwapchain.cpp
                            )

SOURCE_GROUP("Vulkan\\Render Pass" FILES ${VK_RENDER_PASS_SOURCE})

SOURCE_GROUP("Vulkan\\Render Context" FILES ${VK_RENDER_CONTEXT_SOURCE})

SET(VK_CMD_BUFFER_SOURCE    ${SG_INCLUDE_PATH}/VKCommandBuffer.h
                            Vulkan/VKCommandBuffer.cpp
                            Vulkan/VKCommandBufferRender.cpp)

SOURCE_GROUP("Vulkan\\Command Buffer" FILES ${VK_CMD_BUFFER_SOURCE})

SET(VK_RENDERABLE_SOURCE    ${SG_INCLUDE_PATH}/VKRenderable.h
                            ${SG_INCLUDE_PATH}/VKRenderableInstance.h
                            Vulkan/VKRenderable.cpp
                            Vulkan/VKRenderableInstance.cpp
                            Vulkan/VKTileData.cpp
                            Vulkan/VKTileFont.cpp)

SOURCE_GROUP("Vulkan\\Renderable" FILES ${VK_RENDERABLE_SOURCE})

IF(WIN32)
    OPTION(FORCE_DISCETE_GPU    "Force Discrete GPU" OFF)

    IF(FORCE_DISCETE_GPU)
        SET(RENDER_DEVICE_SOURCE ForceDiscreteGPU.c)
    ENDIF()
ENDIF(WIN32)

SET(VULKAN_RENDER_SOURCE    ${VK_RR_SOURCE}
                            ${VK_RR_SHADER_FILES}
                            ${VK_RR_MATERIAL_FILES}
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
                            #${VK_RENDER_CONTEXT_SOURCE}
                            ${VK_CMD_BUFFER_SOURCE}
                            ${VK_RENDERABLE_SOURCE}
                            ${VK_RENDER_DEVICE_SOURCE})

add_cm_library(ULRE.SceneGraph "ULRE"   ${SCENE_GRAPH_HEADER}
                                        ${SCENE_GRAPH_SOURCE}

                                        ${SG_TEXTURE_SOURCE}
                                        ${TILE_SOURCE}
                                        ${SG_VAD_SOURCE}

                                        ${RENDERABLE_FILES}

                                        ${FONT_MANAGE_SOURCE}
                                        ${FONT_SOURCE}
                                        ${FONT_SOURCE_OS}
                                        ${TILE_FONT_SOURCE}
                                        ${FONT_LAYOUT_SOURCE}
                                        ${TEXT_RENDERABLE_SOURCE}

                                        ${VULKAN_RENDER_SOURCE})
