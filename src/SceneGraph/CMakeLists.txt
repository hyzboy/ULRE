set(SG_INCLUDE_PATH     ${ROOT_INCLUDE_PATH}/hgl/graph)

SET(CM_CAMERA_SOURCE    ${SG_INCLUDE_PATH}/camera/Camera.h
                        ${SG_INCLUDE_PATH}/camera/CameraControl.h
                        ${SG_INCLUDE_PATH}/camera/LookAtCameraControl.h
                        ${SG_INCLUDE_PATH}/camera/FirstPersonCameraControl.h
                        ${SG_INCLUDE_PATH}/camera/ViewModelCameraControl.h
                        camera/Camera.cpp
                        camera/FirstPersonCameraControl.cpp)

SOURCE_GROUP("Camera" FILES ${CM_CAMERA_SOURCE})

SET(SG_TEXTURE_SOURCE   ${SG_INCLUDE_PATH}/TextureLoader.h
                        ${SG_INCLUDE_PATH}/Bitmap2DLoader.h
                        TextureLoader.cpp
                        Bitmap2DLoader.cpp)

SOURCE_GROUP("Texture" FILES ${SG_TEXTURE_SOURCE})

SET(SG_VDM_SOURCE   ${SG_INCLUDE_PATH}/VertexAttribDataAccess.h
                    ${SG_INCLUDE_PATH}/VertexDataManager.h
                    VertexDataManager.cpp)

SOURCE_GROUP("VertexDataManager" FILES ${SG_VDM_SOURCE})

SET(SG_PRIMITIVE_SOURCE ${SG_INCLUDE_PATH}/VKPrimitive.h
                        Vulkan/VKPrimitive.cpp
                        Vulkan/VKPrimitiveData.cpp
                        Vulkan/VKPrimitiveData.h
                        ${SG_INCLUDE_PATH}/PrimitiveCreater.h
                        PrimitiveCreater.cpp)

SOURCE_GROUP("Mesh\\Primitive" FILES ${SG_PRIMITIVE_SOURCE})

SET(VK_MESH_SOURCE  ${SG_INCLUDE_PATH}/mesh/MeshDataBuffer.h
                    ${SG_INCLUDE_PATH}/mesh/MeshRenderData.h
                    ${SG_INCLUDE_PATH}/mesh/Mesh.h
                    ${SG_INCLUDE_PATH}/mesh/MeshNode.h
                    ${SG_INCLUDE_PATH}/mesh/StaticMesh.h
                    mesh/Mesh.cpp
                    mesh/MeshNode.cpp
                    mesh/StaticMesh.cpp)

SOURCE_GROUP("Mesh\\Mesh" FILES ${VK_MESH_SOURCE})


SET(TILE_SOURCE ${SG_INCLUDE_PATH}/TileData.h
                TileData.cpp
                Vulkan/VKTileData.cpp
                Vulkan/VKTileFont.cpp)

SOURCE_GROUP("Tile" FILES ${TILE_SOURCE})

SET(GEOMETRY_FILES  ${SG_INCLUDE_PATH}/geo/InlineGeometry.h
                    ${SG_INCLUDE_PATH}/geo/Extruded.h
                    ${SG_INCLUDE_PATH}/geo/Wall.h
                    ${SG_INCLUDE_PATH}/geo/Revolution.h
                    #${SG_INCLUDE_PATH}/Mesh.h
                    #${SG_INCLUDE_PATH}/Spline.h
                    InlineGeometry/AxisBoundingBoxSquareArray.cpp
                    InlineGeometry/Circle.cpp
                    InlineGeometry/Cone.cpp
                    InlineGeometry/Cube.cpp
                    InlineGeometry/Cylinder.cpp
                    InlineGeometry/CylinderConeTorusIndices.cpp
                    InlineGeometry/ExtrudedPolygon.cpp
                    InlineGeometry/HexSphere.cpp
                    InlineGeometry/HollowCylinder.cpp
                    InlineGeometry/InlineGeometryCommon.h
                    InlineGeometry/PlaneAndSquare.cpp
                    InlineGeometry/Rectangle.cpp
                    InlineGeometry/SphereDomeTorus.cpp
                    InlineGeometry/Capsule.cpp
                    InlineGeometry/TaperedCapsule.cpp
                    InlineGeometry/Wall.cpp
                    InlineGeometry/Revolution.cpp
)

SOURCE_GROUP("Geometry" FILES ${GEOMETRY_FILES})

SET(LIGHT_FILES ${SG_INCLUDE_PATH}/Light.h
                ${SG_INCLUDE_PATH}/ShadowPolicy.h)

source_group("Light" FILES ${LIGHT_FILES})

SET(PIPELINE_HEADER_PATH   ${SG_INCLUDE_PATH}/pipeline)
SET(PIPELINE_SOURCE_PATH   Vulkan/pipeline)

SET(PIPELINE_HEADER_FILES   ${PIPELINE_HEADER_PATH}/VKPipeline.h
                            ${PIPELINE_HEADER_PATH}/VKPipelineData.h
                            ${PIPELINE_HEADER_PATH}/VKPipelineHash.h
                            ${PIPELINE_HEADER_PATH}/VKInlinePipeline.h)

SET(PIPELINE_SOURCE_FILES   ${PIPELINE_SOURCE_PATH}/VKPipelineCompare.cpp
                            ${PIPELINE_SOURCE_PATH}/VKPipelineData.cpp
                            ${PIPELINE_SOURCE_PATH}/VKPipelineDataManage.cpp
                            ${PIPELINE_SOURCE_PATH}/VKPipelineDataSaveLoad.cpp
                            ${PIPELINE_SOURCE_PATH}/VKPipelineHash.cpp
                            ${PIPELINE_SOURCE_PATH}/VKPipeline.cpp
                            ${PIPELINE_SOURCE_PATH}/VKPipelineCache.cpp
                            ${PIPELINE_SOURCE_PATH}/VKPipelineLayoutData.cpp
                            ${PIPELINE_SOURCE_PATH}/VKPipelineLayoutData.h)

SOURCE_GROUP("Pipeline\\Header" FILES ${PIPELINE_HEADER_FILES})
SOURCE_GROUP("Pipeline\\Source" FILES ${PIPELINE_SOURCE_FILES})

SET(SGM_HEADER_PATH ${SG_INCLUDE_PATH}/module)
SET(SGM_SOURCE_PATH module)

SET(GRAPH_MODULE_FILES  ${SGM_HEADER_PATH}/GraphModule.h
                        ${SGM_HEADER_PATH}/GraphModuleManager.h
                        ${SGM_HEADER_PATH}/RenderPassManager.h
                        ${SGM_HEADER_PATH}/TextureManager.h
                        ${SGM_HEADER_PATH}/SamplerManager.h
                        ${SGM_HEADER_PATH}/MaterialManager.h
                        ${SGM_HEADER_PATH}/BufferManager.h
                        ${SGM_HEADER_PATH}/RenderTargetManager.h
                        ${SGM_HEADER_PATH}/PrimitiveManager.h
                        ${SGM_HEADER_PATH}/MeshManager.h

                        ${SGM_HEADER_PATH}/RenderModule.h
                        ${SGM_HEADER_PATH}/SwapchainModule.h

                        ${SGM_SOURCE_PATH}/GraphModule.cpp
                        ${SGM_SOURCE_PATH}/GraphModuleManager.cpp
                        ${SGM_SOURCE_PATH}/RenderPassManager.cpp
                        ${SGM_SOURCE_PATH}/TextureManager.cpp
                        ${SGM_SOURCE_PATH}/SamplerManager.cpp
                        ${SGM_SOURCE_PATH}/MaterialManager.cpp
                        ${SGM_SOURCE_PATH}/BufferManager.cpp
                        ${SGM_SOURCE_PATH}/RenderTargetManager.cpp
                        ${SGM_SOURCE_PATH}/PrimitiveManager.cpp
                        ${SGM_SOURCE_PATH}/MeshManager.cpp

                        ${SGM_SOURCE_PATH}/SwapChainModule.cpp
)

source_group("Framework\\Module" FILES ${GRAPH_MODULE_FILES})

SET(SG_LINE_RENDER_MANAGER  ${SG_INCLUDE_PATH}/geo/line/SharedLineBackup.h
                            ${SG_INCLUDE_PATH}/geo/line/LineWidthBatch.h
                            ${SG_INCLUDE_PATH}/geo/line/LineRenderManager.h
                            render/line/LineWidthBatch.cpp
                            render/line/LineRenderManager.cpp)

SOURCE_GROUP("Framework\\LineRenderManager" FILES ${SG_LINE_RENDER_MANAGER})

SET(GRAPH_FRAMEWORK_FILES   ${SG_INCLUDE_PATH}/RenderFramework.h
                            RenderFramework.cpp
                            CameraControl.cpp)

source_group("Framework" FILES ${GRAPH_FRAMEWORK_FILES})

SET(ENV_SKY_FILES   ${SG_INCLUDE_PATH}/env/Sky.h
                    env/Sky.cpp)

SOURCE_GROUP("Scene Graph\\Environment\\Sky" FILES ${ENV_SKY_FILES})

SET(SG_SCENE_SOURCE ${SG_INCLUDE_PATH}/Scene.h
                    ${SG_INCLUDE_PATH}/SceneNodeAttributes.h
                    ${SG_INCLUDE_PATH}/SceneNode.h
                    ${SG_INCLUDE_PATH}/TransformState.h
                    ${SG_INCLUDE_PATH}/NodeTransform.h
                    Scene/SceneNode.cpp
                    Scene/NodeTransform.cpp
                    Scene/TransformState.cpp
                    Scene/Scene.cpp
)

SET(SG_RENDER_SOURCE    ${SG_INCLUDE_PATH}/DrawNode.h
                        ${SG_INCLUDE_PATH}/RenderCollector.h
                        ${SG_INCLUDE_PATH}/PipelineMaterialBatch.h
                        ${SG_INCLUDE_PATH}/RenderBatchMap.h
#                        ${SG_INCLUDE_PATH}/ShadowPolicy.h
                        ${SG_INCLUDE_PATH}/RenderTask.h
                        ${SG_INCLUDE_PATH}/RenderContext.h
                        ${SG_INCLUDE_PATH}/SceneRenderer.h
                        render/RenderContext.cpp
                        render/SceneRenderer.cpp
                        render/DrawNode.cpp
                        render/RenderTask.cpp
                        render/RenderCollector.cpp
                        render/PipelineMaterialBatch.cpp
                        render/InstanceAssignmentBuffer.h
                        render/InstanceAssignmentBuffer.cpp
                        )

SOURCE_GROUP("Scene Graph\\Scene" FILES ${SG_SCENE_SOURCE})
SOURCE_GROUP("Scene Graph\\Render" FILES ${SG_RENDER_SOURCE})

SET(SCENE_GRAPH_FILES  ${SG_SCENE_SOURCE} ${SG_RENDER_SOURCE} ${ENV_SKY_FILES})

SET(FONT_MANAGE_SOURCE  ${SG_INCLUDE_PATH}/font/Font.h
                        font/Font.cpp)

SET(FONT_SOURCE ${SG_INCLUDE_PATH}/font/FontSource.h
                font/FontDataSource.cpp
                font/FontBitmapDataSource.cpp
                font/FontDataSourceManage.cpp
                font/FontSource.cpp)

SET(TILE_FONT_SOURCE    ${SG_INCLUDE_PATH}/font/TileFont.h
                        font/TileFont.cpp)

SET(FONT_LAYOUT_SOURCE  ${SG_INCLUDE_PATH}/font/TextLayout.h
                        font/TextLayoutEngine.h
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
    SET(FONT_SOURCE_OS  font/FontDataSourceWin.cpp
                        font/FontDataSourceWin.h)

    SOURCE_GROUP("Font\\Source\\Windows" FILES ${FONT_SOURCE_OS})
ELSE()
    # Find FreeType for non-Windows platforms
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FREETYPE REQUIRED freetype2)

    SET(FONT_SOURCE_OS  font/FontDataSourceFreeType.cpp
                        font/FontDataSourceFreeType.h)

    SOURCE_GROUP("Font\\Source\\FreeType" FILES ${FONT_SOURCE_OS})
ENDIF(WIN32)

SET(TEXT_SOURCE ${FONT_MANAGE_SOURCE}
                ${FONT_SOURCE}
                ${FONT_SOURCE_OS}
                ${TILE_FONT_SOURCE}
                ${FONT_LAYOUT_SOURCE}
                ${TEXT_RENDERABLE_SOURCE})

SET(VK_RR_MATERIAL_FILES    ${SG_INCLUDE_PATH}/VKMaterialDescriptorManager.h
                            Vulkan/VKMaterialDescriptorManager.cpp)

SOURCE_GROUP("Vulkan\\Material" FILES   ${VK_RR_MATERIAL_FILES})

SET(VK_INST_SOURCE      ${SG_INCLUDE_PATH}/VKInstance.h
                        Vulkan/VKInstance.cpp)

SET(VK_DEBUG_SOURCE     ${SG_INCLUDE_PATH}/VKDebugOut.h
                        ${SG_INCLUDE_PATH}/VKDebugUtils.h
                        Vulkan/Debug/VKDebugOut.cpp
                        Vulkan/Debug/VKDebugUtils.cpp)

SET(VK_MEMORY_SOURCE    ${SG_INCLUDE_PATH}/VKMemory.h
                        ${SG_INCLUDE_PATH}/VKMemoryAllocator.h
                        ${SG_INCLUDE_PATH}/VKBuffer.h
                        ${SG_INCLUDE_PATH}/VKIndexBuffer.h
                        ${SG_INCLUDE_PATH}/VKArrayBuffer.h
                        ${SG_INCLUDE_PATH}/VKBufferMap.h
                        Vulkan/VKMemory.cpp
                        Vulkan/VKMemoryAllocator.cpp
                        Vulkan/VKBuffer.cpp
                        Vulkan/VKArrayBuffer.cpp
                        )

SET(VK_DEVICE_TEXTURE_SOURCE    Vulkan/Texture/BufferImageCopy2D.h
                                Vulkan/Texture/GenMipmaps.cpp
                                Vulkan/Texture/VKDeviceTexture.cpp
                                Vulkan/Texture/VKDeviceTexture2D.cpp
                                Vulkan/Texture/VKDeviceTexture2DArray.cpp
                                Vulkan/Texture/VKDeviceTextureCube.cpp)

SET(VK_TEXTURE_LOADER_SOURCE    ${SG_INCLUDE_PATH}/VKTextureCreateInfo.h
                                Vulkan/Texture/VKTextureLoader.h
                                Vulkan/Texture/VKTexture2DLoader.cpp
                                Vulkan/Texture/VKTexture2DArrayLoader.cpp
                                Vulkan/Texture/VKTextureCubeLoader.cpp)

SET(VK_DEVICE_CREATER_SOURCE    ${SG_INCLUDE_PATH}/VKDevice.h
                            ${SG_INCLUDE_PATH}/VKDeviceAttribute.h
                            ${SG_INCLUDE_PATH}/VKDeviceCreater.h
                            Vulkan/VKDeviceAttribute.cpp
                            Vulkan/VKDeviceCreater.cpp
                            Vulkan/VKDevice.cpp)

SET(VK_DEVICE_SOURCE    Vulkan/VKDeviceMemory.cpp
                        Vulkan/VKDeviceBuffer.cpp
                        Vulkan/VKDeviceImage.cpp
                        Vulkan/VKDeviceSampler.cpp
                        Vulkan/VKDeviceMaterial.cpp
                        Vulkan/VKDeviceFramebuffer.cpp)

SET(VK_PHYSICAL_DEVICE_SOURCE   ${SG_INCLUDE_PATH}/VKPhysicalDevice.h
                                ${SG_INCLUDE_PATH}/VKSurface.h
                                Vulkan/VKPhysicalDevice.cpp
                                Vulkan/VKProperties.cpp
                                Vulkan/DebugOutProperties.h
                                Vulkan/Debug/VKPhysicalDeviceCapsOutput.cpp
                                Vulkan/VKSurface.cpp)

SOURCE_GROUP("Vulkan\\Device"                   FILES ${VK_DEVICE_SOURCE})
SOURCE_GROUP("Vulkan\\Device\\Creater"          FILES ${VK_DEVICE_CREATER_SOURCE})
SOURCE_GROUP("Vulkan\\Device\\Debug"            FILES ${VK_DEBUG_SOURCE})
SOURCE_GROUP("Vulkan\\Device\\Instance"         FILES ${VK_INST_SOURCE})
SOURCE_GROUP("Vulkan\\Device\\Physical Device"  FILES ${VK_PHYSICAL_DEVICE_SOURCE})
SOURCE_GROUP("Vulkan\\Device\\Memory"           FILES ${VK_MEMORY_SOURCE})
SOURCE_GROUP("Vulkan\\Device\\Texture"          FILES ${VK_DEVICE_TEXTURE_SOURCE})
SOURCE_GROUP("Vulkan\\Device\\Texture\\Loader"  FILES ${VK_TEXTURE_LOADER_SOURCE})

SET(VK_DESCRIPTOR_SETS_SOURCE   ${SG_INCLUDE_PATH}/VKDescriptorSet.h
                                Vulkan/VKDescriptorSet.cpp)

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
                        ${SG_INCLUDE_PATH}/VKDynamicBufferAccess.h
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
                            ${SG_INCLUDE_PATH}/VKRenderPass.h
                            ${SG_INCLUDE_PATH}/VKSwapchain.h
                            ${SG_INCLUDE_PATH}/VKSemaphore.h
                            ${SG_INCLUDE_PATH}/VKFence.h
                            Vulkan/VKQueue.cpp
                            Vulkan/VKSemaphore.cpp
                            Vulkan/VKFence.cpp
                            Vulkan/VKFramebuffer.cpp
                            #Vulkan/VKSubpass.cpp
                            Vulkan/VKRenderPass.cpp
                            Vulkan/VKSwapchain.cpp
                            )

SOURCE_GROUP("Vulkan\\Render Pass" FILES ${VK_RENDER_PASS_SOURCE})

SET(VK_RENDER_TARGET_SOURCE ${SG_INCLUDE_PATH}/VKRenderTarget.h
                            ${SG_INCLUDE_PATH}/VKRenderTargetData.h
                            ${SG_INCLUDE_PATH}/VKRenderTargetSingle.h
                            ${SG_INCLUDE_PATH}/VKRenderTargetMultiFrame.h
                            ${SG_INCLUDE_PATH}/VKRenderTargetSwapchain.h
                            Vulkan/VKRenderTarget.cpp
                            Vulkan/VKRenderTargetData.cpp
                            Vulkan/VKSwapchainRenderTarget.cpp
)

SOURCE_GROUP("Vulkan\\Render Target" FILES ${VK_RENDER_TARGET_SOURCE})

SOURCE_GROUP("Vulkan\\Render Context" FILES ${VK_RENDER_CONTEXT_SOURCE})

SET(VK_CMD_BUFFER_SOURCE    ${SG_INCLUDE_PATH}/VKCommandBuffer.h
                            ${SG_INCLUDE_PATH}/VKIndirectCommandBuffer.h
                            Vulkan/VKIndirectCommandBuffer.cpp
                            Vulkan/VKCommandBuffer.cpp
                            Vulkan/VKCommandBufferRender.cpp)

SOURCE_GROUP("Vulkan\\Command Buffer" FILES ${VK_CMD_BUFFER_SOURCE})

set(COMPONENT_INCLUDE_PATH     ${ROOT_INCLUDE_PATH}/hgl/component)
SET(COLLISION_COMPONENT_INCLUDE_PATH    ${COMPONENT_INCLUDE_PATH}/collision)

SET(COMPONENT_FILES     ${COMPONENT_INCLUDE_PATH}/Component.h
                        ${COMPONENT_INCLUDE_PATH}/SceneComponent.h
                        ${COMPONENT_INCLUDE_PATH}/PrimitiveComponent.h
                        ${COMPONENT_INCLUDE_PATH}/RenderComponent.h
                        ${COMPONENT_INCLUDE_PATH}/MeshComponent.h
                        ${COMPONENT_INCLUDE_PATH}/StaticMeshComponent.h
                        component/Component.cpp
                        component/ComponentManager.cpp
                        component/SceneComponent.cpp
                        component/MeshComponentManager.cpp
                        component/StaticMeshComponentManager.cpp
)

SET(COLLISION_COMPONENT_FILES   ${COLLISION_COMPONENT_INCLUDE_PATH}/CollisionComponent.h
                                ${COLLISION_COMPONENT_INCLUDE_PATH}/CollisionComponentManager.h
                                ${COLLISION_COMPONENT_INCLUDE_PATH}/PrimitiveCollisionComponent.h
                                ${COLLISION_COMPONENT_INCLUDE_PATH}/SphereCollisionComponent.h
                                ${COLLISION_COMPONENT_INCLUDE_PATH}/BoxCollisionComponent.h
                                ${COLLISION_COMPONENT_INCLUDE_PATH}/CapsuleCollisionComponent.h
                                ${COLLISION_COMPONENT_INCLUDE_PATH}/PlaneCollisionComponent.h
#                                component/collision/CollisionComponent.cpp
#                                component/collision/CollisionComponentManager.cpp
#                                component/collision/PrimitiveCollisionComponent.cpp
#                                component/collision/SphereCollisionComponent.cpp
#                                component/collision/BoxCollisionComponent.cpp
#                                component/collision/CapsuleCollisionComponent.cpp
#                                component/collision/PlaneCollisionComponent.cpp
)

SOURCE_GROUP("Component" FILES ${COMPONENT_FILES})
SOURCE_GROUP("Component\\Collision" FILES ${COLLISION_COMPONENT_FILES})

IF(WIN32)
    OPTION(FORCE_DISCETE_GPU    "Force Discrete GPU" OFF)

    IF(FORCE_DISCETE_GPU)
        SET(VULKAN_RENDER_DEVICE_SOURCE ForceDiscreteGPU.c)
    ENDIF()

    SET(VULKAN_SURFACE_SOURCE   Vulkan/Platform/WinVulkan.cpp)
ENDIF(WIN32)

IF(UNIX)

    IF(ANDROID)
        SET(VULKAN_SURFACE_SOURCE   Vulkan/platform/AndroidVulkan.cpp)
    ELSE()
        IF(APPLE)
            if(CMAKE_SYSTEM_NAME STREQUAL "iOS" OR CMAKE_OSX_SYSROOT MATCHES ".*iPhone.*")
                message(STATUS "Targeting iOS")
                set(VULKAN_SURFACE_SOURCE Vulkan/platform/iOSVulkan.cpp)
            elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_OSX_SYSROOT MATCHES ".*MacOSX.*")
                message(STATUS "Targeting macOS")
                set(VULKAN_SURFACE_SOURCE Vulkan/platform/MacVulkan.cpp)
            else()
                message(FATAL_ERROR "Unknown Apple platform")
            endif()
        ELSE()
            find_package(PkgConfig REQUIRED)
            pkg_check_modules(WAYLAND_CLIENT wayland-client)
            if(WAYLAND_CLIENT_FOUND)
                message(STATUS "Targeting Wayland")
                set(VULKAN_SURFACE_SOURCE Vulkan/platform/WaylandVulkan.cpp)
            else()
                message(STATUS "Targeting XCB")
                set(VULKAN_SURFACE_SOURCE Vulkan/platform/XCBVulkan.cpp)
            endif()
        ENDIF()
    ENDIF()

ENDIF(UNIX)

SOURCE_GROUP("Vulkan\\Surface" FILES ${VULKAN_SURFACE_SOURCE})

SET(VULKAN_RENDER_SOURCE    ${VK_RR_MATERIAL_FILES}
                            ${VK_INST_SOURCE}
                            ${VK_DEBUG_SOURCE}
                            ${VK_MEMORY_SOURCE}
                            ${VK_DEVICE_SOURCE}
                            ${VK_DEVICE_CREATER_SOURCE}
                            ${VK_DEVICE_TEXTURE_SOURCE}
                            ${VK_TEXTURE_LOADER_SOURCE}
                            ${VK_PHYSICAL_DEVICE_SOURCE}
                            ${VK_DESCRIPTOR_SETS_SOURCE}
                            ${VK_SHADER_SOURCE}
                            ${VK_TEXTURE_SOURCE}
                            ${PIPELINE_HEADER_FILES}
                            ${PIPELINE_SOURCE_FILES}
                            ${VK_MATERIAL_SOURCE}
                            ${VK_RENDER_PASS_SOURCE}
                            ${VK_RENDER_TARGET_SOURCE}
                            #${VK_RENDER_CONTEXT_SOURCE}
                            ${VK_CMD_BUFFER_SOURCE}
                            ${VK_MESH_SOURCE}
                            ${VULKAN_RENDER_DEVICE_SOURCE})

add_cm_library(ULRE.SceneGraph "ULRE"   ${SCENE_GRAPH_FILES}
                                        ${SG_PRIMITIVE_SOURCE}
                                        ${GEOMETRY_FILES}
                                        ${LIGHT_FILES}

                                        ${CM_CAMERA_SOURCE}

                                        ${GRAPH_MODULE_FILES}
                                        ${GRAPH_FRAMEWORK_FILES}

                                        ${SG_LINE_RENDER_MANAGER}

                                        ${SG_TEXTURE_SOURCE}
                                        ${TILE_SOURCE}
                                        ${SG_VDM_SOURCE}

                                        ${COMPONENT_FILES}
                                        ${COLLISION_COMPONENT_FILES}

                                        ${TEXT_SOURCE}

                                        ${VULKAN_RENDER_SOURCE}
                                        ${VULKAN_SURFACE_SOURCE})

# Link FreeType on non-Windows platforms
IF(NOT WIN32)
    target_include_directories(ULRE.SceneGraph PRIVATE ${FREETYPE_INCLUDE_DIRS})
    target_link_libraries(ULRE.SceneGraph ${FREETYPE_LIBRARIES})
ENDIF()
