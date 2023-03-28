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

SET(GEOMETRY_FILES  ${SG_INCLUDE_PATH}/InlineGeometry.h
                    #${SG_INCLUDE_PATH}/Mesh.h
                    #${SG_INCLUDE_PATH}/Spline.h
                    InlineGeometry.cpp
                    #Mesh.cpp
                    ${SG_INCLUDE_PATH}/PrimitiveCreater.h
                    PrimitiveCreater.cpp
)

SOURCE_GROUP("Geometry" FILES ${GEOMETRY_FILES})

SET(LIGHT_FILES ${SG_INCLUDE_PATH}/Light.h)

source_group("Light" FILES ${LIGHT_FILES})

SET(SCENE_GRAPH_HEADER  ${SG_INCLUDE_PATH}/SceneInfo.h
                        ${SG_INCLUDE_PATH}/SceneNode.h
                        ${SG_INCLUDE_PATH}/RenderNode.h
                        ${SG_INCLUDE_PATH}/SceneOrient.h
                        ${SG_INCLUDE_PATH}/RenderList.h
                        ${SG_INCLUDE_PATH}/RenderList2D.h
                        )

SET(SCENE_GRAPH_SOURCE  
                        #RenderList.cpp
				        RenderList2D.cpp
				        #RenderList3D.cpp
                        SceneNode.cpp
                        SceneOrient.cpp)

SOURCE_GROUP("Scene Graph" FILES ${SCENE_GRAPH_HEADER} ${SCENE_GRAPH_SOURCE})

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

SET(TEXT_RENDERABLE_SOURCE  ${SG_INCLUDE_PATH}/font/TextPrimitive.h
                            ${SG_INCLUDE_PATH}/font/TextRender.h
                            font/TextRender.cpp
                            font/TextPrimitive.cpp)

SOURCE_GROUP("Font"             FILES ${FONT_MANAGE_SOURCE})
SOURCE_GROUP("Font\\Source"     FILES ${FONT_SOURCE})
SOURCE_GROUP("Font\\TileFont"   FILES ${TILE_FONT_SOURCE})
SOURCE_GROUP("Font\\Layout"     FILES ${FONT_LAYOUT_SOURCE})
SOURCE_GROUP("Font\\Render"     FILES ${TEXT_RENDERABLE_SOURCE})

IF(WIN32)
    SET(FONT_SOURCE_OS  font/FontSourceWin.cpp
                        font/FontSourceWin.h)

    SOURCE_GROUP("Font\\Source\\Windows" FILES ${FONT_SOURCE_OS})
ENDIF(WIN32)

SET(VK_RR_SOURCE        ${SG_INCLUDE_PATH}/VKRenderResource.h
                        ${SG_INCLUDE_PATH}/VKRenderablePrimitiveCreater.h
                        Vulkan/VKRenderResource.cpp
                        Vulkan/VKRenderResourceMaterial.cpp)

SOURCE_GROUP("Vulkan\\RenderResource" FILES ${VK_RR_SOURCE})

SET(VK_RR_SHADER_FILES  ${SG_INCLUDE_PATH}/VKShaderResource.h
                        Vulkan/VKShaderResource.cpp)

SOURCE_GROUP("Vulkan\\RenderResource\\Shader" FILES ${VK_RR_SHADER_FILES})

SET(VK_RR_MATERIAL_FILES    ${SG_INCLUDE_PATH}/VKMaterialDescriptorManager.h
                            Vulkan/VKMaterialDescriptorManager.cpp)

SOURCE_GROUP("Vulkan\\RenderResource\\Material" FILES   ${VK_RR_MATERIAL_FILES})

SET(VK_INST_SOURCE      ${SG_INCLUDE_PATH}/VKInstance.h
                        Vulkan/VKInstance.cpp)

SET(VK_DEBUG_SOURCE     ${SG_INCLUDE_PATH}/VKDebugOut.h
                        ${SG_INCLUDE_PATH}/VKDebugMaker.h
                        ${SG_INCLUDE_PATH}/VKDebugUtils.h
                        Vulkan/Debug/VKDebugOut.cpp
                        Vulkan/Debug/VKDebugMaker.cpp
                        Vulkan/Debug/VKDebugUtils.cpp)

SET(VK_MEMORY_SOURCE    ${SG_INCLUDE_PATH}/VKMemory.h
                        ${SG_INCLUDE_PATH}/VKMemoryAllocator.h
                        ${SG_INCLUDE_PATH}/VKBuffer.h
                        ${SG_INCLUDE_PATH}/VKIndexBuffer.h
                        ${SG_INCLUDE_PATH}/VKArrayBuffer.h
                        Vulkan/VKMemory.cpp
                        Vulkan/VKMemoryAllocator.cpp
                        Vulkan/VKBuffer.cpp
                        Vulkan/VKArrayBuffer.cpp
                        )

SET(VK_DEVICE_TEXTURE_SOURCE    Vulkan/Texture/BufferImageCopy2D.h
                                Vulkan/Texture/GenMipmaps.cpp
                                Vulkan/Texture/VKDeviceTexture.cpp
                                Vulkan/Texture/VKDeviceTexture2D.cpp
                                Vulkan/Texture/VKDeviceTextureCube.cpp)

SET(VK_TEXTURE_LOADER_SOURCE    ${SG_INCLUDE_PATH}/VKTextureCreateInfo.h
                                Vulkan/Texture/VKTextureLoader.h
                                Vulkan/Texture/VKTexture2DLoader.cpp
                                Vulkan/Texture/VKTextureCubeLoader.cpp)
                        
SET(VK_DEVICE_SOURCE    ${SG_INCLUDE_PATH}/VKDevice.h
                        ${SG_INCLUDE_PATH}/VKDeviceAttribute.h
                        Vulkan/VKDeviceAttribute.cpp
                        Vulkan/VKDeviceCreater.cpp
                        Vulkan/VKDevice.cpp
                        Vulkan/VKDeviceMemory.cpp
                        Vulkan/VKDeviceBuffer.cpp
                        Vulkan/VKDeviceImage.cpp
                        Vulkan/VKDeviceSampler.cpp
                        Vulkan/VKDeviceMaterial.cpp
                        Vulkan/VKDeviceFramebuffer.cpp
                        Vulkan/VKDeviceSwapchain.cpp
                        Vulkan/VKDeviceRenderPass.cpp
                        Vulkan/VKDeviceRenderPassManage.cpp
                        Vulkan/VKDeviceRenderTarget.cpp)

SET(VK_PHYSICAL_DEVICE_SOURCE ${SG_INCLUDE_PATH}/VKPhysicalDevice.h
                            Vulkan/VKPhysicalDevice.cpp
                            Vulkan/VKProperties.cpp
                            Vulkan/DebugOutProperties.h)

SOURCE_GROUP("Vulkan\\Device"                   FILES ${VK_DEVICE_SOURCE})
SOURCE_GROUP("Vulkan\\Device\\Debug"            FILES ${VK_DEBUG_SOURCE})
SOURCE_GROUP("Vulkan\\Device\\Instance"         FILES ${VK_INST_SOURCE})
SOURCE_GROUP("Vulkan\\Device\\Physical Device"  FILES ${VK_PHYSICAL_DEVICE_SOURCE})
SOURCE_GROUP("Vulkan\\Device\\Memory"           FILES ${VK_MEMORY_SOURCE})
SOURCE_GROUP("Vulkan\\Device\\Texture"          FILES ${VK_DEVICE_TEXTURE_SOURCE})
SOURCE_GROUP("Vulkan\\Device\\Texture\\Loader"  FILES ${VK_TEXTURE_LOADER_SOURCE})

SET(VK_DESCRIPTOR_SETS_SOURCE   ${SG_INCLUDE_PATH}/VKDescriptorSet.h
                                Vulkan/VKDescriptorSet.cpp
                                Vulkan/VKPipelineLayoutData.h                                
                                Vulkan/VKPipelineLayoutData.cpp)

SOURCE_GROUP("Vulkan\\Descriptor Sets" FILES ${VK_DESCRIPTOR_SETS_SOURCE})

SET(VK_SHADER_SOURCE    ${SG_INCLUDE_PATH}/VKShaderModule.h
                        ${SG_INCLUDE_PATH}/VKShaderModuleMap.h
                        Vulkan/VKShaderModuleMap.cpp
                        Vulkan/VKShaderModule.cpp)

SET(VK_TEXTURE_SOURCE   ${SG_INCLUDE_PATH}/VKImageView.h
                        ${SG_INCLUDE_PATH}/VKTexture.h
                        ${SG_INCLUDE_PATH}/VKSampler.h
                        Vulkan/VKSampler.cpp
                        Vulkan/VKImageView.cpp
                        Vulkan/VKTexture.cpp)

SET(VK_MATERIAL_SOURCE  ${SG_INCLUDE_PATH}/VKMaterial.h
                        ${SG_INCLUDE_PATH}/VKMaterialParameters.h
                        ${SG_INCLUDE_PATH}/VKMaterialInstance.h
                        ${SG_INCLUDE_PATH}/VKDescriptorBindingManage.h
                        ${SG_INCLUDE_PATH}/VKUBODynamic.h
                        Vulkan/VKDescriptorBindingManage.cpp
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

SET(VK_RENDERABLE_SOURCE    ${SG_INCLUDE_PATH}/VKPrimitive.h
                            ${SG_INCLUDE_PATH}/VKRenderable.h
                            Vulkan/VKPrimitive.cpp
                            Vulkan/VKRenderable.cpp
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
                            ${VK_DEVICE_TEXTURE_SOURCE}
                            ${VK_TEXTURE_LOADER_SOURCE}
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
                                        ${GEOMETRY_FILES}
                                        ${LIGHT_FILES}

                                        ${SG_TEXTURE_SOURCE}
                                        ${TILE_SOURCE}
                                        ${SG_VAD_SOURCE}

                                        ${FONT_MANAGE_SOURCE}
                                        ${FONT_SOURCE}
                                        ${FONT_SOURCE_OS}
                                        ${TILE_FONT_SOURCE}
                                        ${FONT_LAYOUT_SOURCE}
                                        ${TEXT_RENDERABLE_SOURCE}

                                        ${VULKAN_RENDER_SOURCE})
