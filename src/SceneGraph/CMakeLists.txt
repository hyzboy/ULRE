set(SG_INCLUDE_PATH     ${ROOT_INCLUDE_PATH}/hgl/graph)

# ============================================================================
# 1. Core Framework - 核心框架
# ============================================================================

SET(GRAPH_FRAMEWORK_FILES   ${SG_INCLUDE_PATH}/RenderFramework.h
                            RenderFramework.cpp
                            CameraControl.cpp)

SOURCE_GROUP("1.Core\\Framework" FILES ${GRAPH_FRAMEWORK_FILES})

SET(GRAPH_MODULE_FILES  ${SG_INCLUDE_PATH}/module/GraphModule.h
                        ${SG_INCLUDE_PATH}/module/GraphModuleManager.h
                        ${SG_INCLUDE_PATH}/module/RenderPassManager.h
                        ${SG_INCLUDE_PATH}/module/TextureManager.h
                        ${SG_INCLUDE_PATH}/module/SamplerManager.h
                        ${SG_INCLUDE_PATH}/module/MaterialManager.h
                        ${SG_INCLUDE_PATH}/module/BufferManager.h
                        ${SG_INCLUDE_PATH}/module/RenderTargetManager.h
                        ${SG_INCLUDE_PATH}/module/GeometryManager.h
                        ${SG_INCLUDE_PATH}/module/PrimitiveManager.h
                        ${SG_INCLUDE_PATH}/module/RenderModule.h
                        ${SG_INCLUDE_PATH}/module/SwapchainModule.h
                        module/GraphModule.cpp
                        module/GraphModuleManager.cpp
                        module/RenderPassManager.cpp
                        module/TextureManager.cpp
                        module/SamplerManager.cpp
                        module/MaterialManager.cpp
                        module/BufferManager.cpp
                        module/RenderTargetManager.cpp
                        module/GeometryManager.cpp
                        module/PrimitiveManager.cpp
                        module/SwapChainModule.cpp)

SOURCE_GROUP("1.Core\\Module" FILES ${GRAPH_MODULE_FILES})

# ============================================================================
# 2. Scene Management - 场景管理系统
# ============================================================================

SET(SCENE_CORE_FILES ${SG_INCLUDE_PATH}/World.h
                     ${SG_INCLUDE_PATH}/SceneNodeAttributes.h
                     ${SG_INCLUDE_PATH}/SceneNode.h
                     ${SG_INCLUDE_PATH}/TransformState.h
                     Scene/SceneNode.cpp
                     Scene/NodeTransform.cpp
                     Scene/TransformState.cpp
                     Scene/World.cpp)

SOURCE_GROUP("2.Scene\\Core" FILES ${SCENE_CORE_FILES})

SET(SCENE_ENVIRONMENT_FILES ${SG_INCLUDE_PATH}/env/Sky.h
                            env/Sky.cpp)

SOURCE_GROUP("2.Scene\\Environment" FILES ${SCENE_ENVIRONMENT_FILES})

# ============================================================================
# 3. Camera System - 相机系统
# ============================================================================

SET(CAMERA_FILES    ${SG_INCLUDE_PATH}/camera/Camera.h
                    ${SG_INCLUDE_PATH}/camera/CameraControl.h
                    ${SG_INCLUDE_PATH}/camera/LookAtCameraControl.h
                    ${SG_INCLUDE_PATH}/camera/FirstPersonCameraControl.h
                    ${SG_INCLUDE_PATH}/camera/ViewModelCameraControl.h
                    camera/Camera.cpp
                    camera/FirstPersonCameraControl.cpp)

SOURCE_GROUP("3.Camera" FILES ${CAMERA_FILES})

# ============================================================================
# 4. Rendering System - 渲染系统
# ============================================================================

SET(RENDERING_CORE_FILES    ${SG_INCLUDE_PATH}/DrawNode.h
                            ${SG_INCLUDE_PATH}/RenderCollector.h
                            ${SG_INCLUDE_PATH}/PipelineMaterialBatch.h
                            ${SG_INCLUDE_PATH}/PipelineMaterialRenderer.h
                            ${SG_INCLUDE_PATH}/RenderBatchMap.h
                            ${SG_INCLUDE_PATH}/RenderTask.h
                            ${SG_INCLUDE_PATH}/RenderContext.h
                            ${SG_INCLUDE_PATH}/SceneRenderer.h
                            render/RenderContext.cpp
                            render/SceneRenderer.cpp
                            render/DrawNode.cpp
                            render/RenderTask.cpp
                            render/RenderCollector.cpp
                            render/PipelineMaterialBatch.cpp
                            render/PipelineMaterialRenderer.cpp
                            render/InstanceAssignmentBuffer.h
                            render/InstanceAssignmentBuffer.cpp)

SOURCE_GROUP("4.Rendering\\Core" FILES ${RENDERING_CORE_FILES})

SET(RENDERING_LINE_FILES    ${SG_INCLUDE_PATH}/geo/line/SharedLineBackup.h
                            ${SG_INCLUDE_PATH}/geo/line/LineWidthBatch.h
                            ${SG_INCLUDE_PATH}/geo/line/LineRenderManager.h
                            render/line/LineWidthBatch.cpp
                            render/line/LineRenderManager.cpp)

SOURCE_GROUP("4.Rendering\\Line" FILES ${RENDERING_LINE_FILES})

SET(RENDERING_LIGHTING_FILES ${SG_INCLUDE_PATH}/Light.h
                             ${SG_INCLUDE_PATH}/ShadowPolicy.h)

SOURCE_GROUP("4.Rendering\\Lighting" FILES ${RENDERING_LIGHTING_FILES})

# ============================================================================
# 5. Geometry & Mesh System - 几何体与网格系统
# ============================================================================

SET(GEOMETRY_CORE_FILES ${SG_INCLUDE_PATH}/VKGeometry.h
                        Vulkan/VKGeometry.cpp
                        Vulkan/VKGeometryData.cpp
                        Vulkan/VKGeometryData.h
                        ${SG_INCLUDE_PATH}/GeometryCreater.h
                        GeometryCreater.cpp)

SOURCE_GROUP("5.Geometry\\Core" FILES ${GEOMETRY_CORE_FILES})

SET(GEOMETRY_MESH_FILES ${SG_INCLUDE_PATH}/mesh/GeometryDataBuffer.h
                        ${SG_INCLUDE_PATH}/mesh/GeometryDrawRange.h
                        ${SG_INCLUDE_PATH}/mesh/Primitive.h
                        ${SG_INCLUDE_PATH}/mesh/StaticMesh.h
                        mesh/Primitive.cpp
                        mesh/StaticMesh.cpp)

SOURCE_GROUP("5.Geometry\\Mesh" FILES ${GEOMETRY_MESH_FILES})

SET(GEOMETRY_VERTEX_DATA_FILES  ${SG_INCLUDE_PATH}/VertexAttribDataAccess.h
                                ${SG_INCLUDE_PATH}/VertexDataManager.h
                                VertexDataManager.cpp)

SOURCE_GROUP("5.Geometry\\VertexData" FILES ${GEOMETRY_VERTEX_DATA_FILES})

SET(GEOMETRY_PRIMITIVES_BASIC   InlineGeometry/InlineGeometryCommon.h
                                ${SG_INCLUDE_PATH}/geo/InlineGeometry.h
                                InlineGeometry/PlaneAndSquare.cpp
                                InlineGeometry/Rectangle.cpp
                                InlineGeometry/Cube.cpp
                                InlineGeometry/Circle.cpp
                                InlineGeometry/AxisBoundingBoxSquareArray.cpp)

SET(GEOMETRY_PRIMITIVES_SPHERE  InlineGeometry/SphereDomeTorus.cpp
                                InlineGeometry/HexSphere.cpp)

SET(GEOMETRY_PRIMITIVES_CYLINDER InlineGeometry/CylinderConeTorusIndices.cpp
                                 InlineGeometry/Cylinder.cpp
                                 InlineGeometry/HollowCylinder.cpp
                                 InlineGeometry/Cone.cpp)

SET(GEOMETRY_PRIMITIVES_CAPSULE InlineGeometry/Capsule.cpp
                                InlineGeometry/TaperedCapsule.cpp)

SET(GEOMETRY_PRIMITIVES_ADVANCED ${SG_INCLUDE_PATH}/geo/Extruded.h
                                 ${SG_INCLUDE_PATH}/geo/Wall.h
                                 ${SG_INCLUDE_PATH}/geo/Revolution.h
                                 InlineGeometry/ExtrudedPolygon.cpp
                                 InlineGeometry/Wall.cpp
                                 InlineGeometry/Revolution.cpp)

SOURCE_GROUP("5.Geometry\\Primitives\\Basic" FILES ${GEOMETRY_PRIMITIVES_BASIC})
SOURCE_GROUP("5.Geometry\\Primitives\\Sphere" FILES ${GEOMETRY_PRIMITIVES_SPHERE})
SOURCE_GROUP("5.Geometry\\Primitives\\Cylinder" FILES ${GEOMETRY_PRIMITIVES_CYLINDER})
SOURCE_GROUP("5.Geometry\\Primitives\\Capsule" FILES ${GEOMETRY_PRIMITIVES_CAPSULE})
SOURCE_GROUP("5.Geometry\\Primitives\\Advanced" FILES ${GEOMETRY_PRIMITIVES_ADVANCED})

SET(GEOMETRY_FILES  ${GEOMETRY_PRIMITIVES_BASIC}
                    ${GEOMETRY_PRIMITIVES_SPHERE}
                    ${GEOMETRY_PRIMITIVES_CYLINDER}
                    ${GEOMETRY_PRIMITIVES_CAPSULE}
                    ${GEOMETRY_PRIMITIVES_ADVANCED})

# ============================================================================
# 6. Texture System - 纹理系统
# ============================================================================

SET(TEXTURE_LOADER_FILES    ${SG_INCLUDE_PATH}/TextureLoader.h
                            ${SG_INCLUDE_PATH}/Bitmap2DLoader.h
                            TextureLoader.cpp
                            Bitmap2DLoader.cpp)

SOURCE_GROUP("6.Texture\\Loader" FILES ${TEXTURE_LOADER_FILES})

SET(TEXTURE_TILE_FILES  ${SG_INCLUDE_PATH}/TileData.h
                        TileData.cpp
                        Vulkan/VKTileData.cpp
                        Vulkan/VKTileFont.cpp)

SOURCE_GROUP("6.Texture\\Tile" FILES ${TEXTURE_TILE_FILES})

# ============================================================================
# 7. Font System - 字体系统
# ============================================================================

SET(FONT_CORE_FILES ${SG_INCLUDE_PATH}/font/Font.h
                    font/Font.cpp)

SOURCE_GROUP("7.Font\\Core" FILES ${FONT_CORE_FILES})

SET(FONT_DATA_SOURCE_FILES  ${SG_INCLUDE_PATH}/font/FontSource.h
                            font/FontDataSource.cpp
                            font/FontBitmapDataSource.cpp
                            font/FontDataSourceManage.cpp
                            font/FontSource.cpp)

SOURCE_GROUP("7.Font\\DataSource" FILES ${FONT_DATA_SOURCE_FILES})

SET(FONT_TILE_FILES ${SG_INCLUDE_PATH}/font/TileFont.h
                    font/TileFont.cpp)

SOURCE_GROUP("7.Font\\TileFont" FILES ${FONT_TILE_FILES})

SET(FONT_LAYOUT_FILES   ${SG_INCLUDE_PATH}/font/TextLayout.h
                        font/TextLayoutEngine.h
                        font/TextLayout.cpp)

SOURCE_GROUP("7.Font\\Layout" FILES ${FONT_LAYOUT_FILES})

SET(FONT_RENDERING_FILES    ${SG_INCLUDE_PATH}/font/TextGeometry.h
                            ${SG_INCLUDE_PATH}/font/TextRender.h
                            font/TextRender.cpp
                            font/TextGeometry.cpp)

SOURCE_GROUP("7.Font\\Rendering" FILES ${FONT_RENDERING_FILES})

IF(WIN32)
    SET(FONT_PLATFORM_FILES font/FontDataSourceWin.cpp
                            font/FontDataSourceWin.h)

    SOURCE_GROUP("7.Font\\Platform\\Windows" FILES ${FONT_PLATFORM_FILES})
ELSE()
    # Find FreeType for non-Windows platforms
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FREETYPE REQUIRED freetype2)

    SET(FONT_PLATFORM_FILES font/FontDataSourceFreeType.cpp
                            font/FontDataSourceFreeType.h)

    SOURCE_GROUP("7.Font\\Platform\\FreeType" FILES ${FONT_PLATFORM_FILES})
ENDIF(WIN32)

SET(FONT_ALL_FILES  ${FONT_CORE_FILES}
                    ${FONT_DATA_SOURCE_FILES}
                    ${FONT_PLATFORM_FILES}
                    ${FONT_TILE_FILES}
                    ${FONT_LAYOUT_FILES}
                    ${FONT_RENDERING_FILES})

# ============================================================================
# 8. Component System - 组件系统
# ============================================================================

set(COMPONENT_INCLUDE_PATH     ${ROOT_INCLUDE_PATH}/hgl/component)

SET(COMPONENT_FILES     ${COMPONENT_INCLUDE_PATH}/Component.h
                        ${COMPONENT_INCLUDE_PATH}/SceneComponent.h
                        ${COMPONENT_INCLUDE_PATH}/GeometryComponent.h
                        ${COMPONENT_INCLUDE_PATH}/RenderComponent.h
                        ${COMPONENT_INCLUDE_PATH}/PrimitiveComponent.h
                        component/Component.cpp
                        component/ComponentManager.cpp
                        component/SceneComponent.cpp
                        component/GeometryComponent.cpp
                        component/PrimitiveComponentManager.cpp)

SOURCE_GROUP("8.Component\\Core" FILES ${COMPONENT_FILES})

SET(COLLISION_COMPONENT_INCLUDE_PATH    ${COMPONENT_INCLUDE_PATH}/collision)

SET(COLLISION_COMPONENT_FILES   ${COLLISION_COMPONENT_INCLUDE_PATH}/CollisionComponent.h
                                ${COLLISION_COMPONENT_INCLUDE_PATH}/CollisionComponentManager.h
                                ${COLLISION_COMPONENT_INCLUDE_PATH}/GeometryCollisionComponent.h
                                ${COLLISION_COMPONENT_INCLUDE_PATH}/SphereCollisionComponent.h
                                ${COLLISION_COMPONENT_INCLUDE_PATH}/BoxCollisionComponent.h
                                ${COLLISION_COMPONENT_INCLUDE_PATH}/CapsuleCollisionComponent.h
                                ${COLLISION_COMPONENT_INCLUDE_PATH}/PlaneCollisionComponent.h
#                                component/collision/CollisionComponent.cpp
#                                component/collision/CollisionComponentManager.cpp
#                                component/collision/GeometryCollisionComponent.cpp
#                                component/collision/SphereCollisionComponent.cpp
#                                component/collision/BoxCollisionComponent.cpp
#                                component/collision/CapsuleCollisionComponent.cpp
#                                component/collision/PlaneCollisionComponent.cpp
)

SOURCE_GROUP("8.Component\\Collision" FILES ${COLLISION_COMPONENT_FILES})

# ============================================================================
# 9. Vulkan Backend - Vulkan后端实现
# ============================================================================

# --- 9.1 Device & Instance ---
SET(VK_DEVICE_INSTANCE_FILES    ${SG_INCLUDE_PATH}/VKInstance.h
                                Vulkan/VKInstance.cpp)

SOURCE_GROUP("9.Vulkan\\Device\\Instance" FILES ${VK_DEVICE_INSTANCE_FILES})

SET(VK_DEVICE_PHYSICAL_FILES    ${SG_INCLUDE_PATH}/VKPhysicalDevice.h
                                ${SG_INCLUDE_PATH}/VKSurface.h
                                Vulkan/VKPhysicalDevice.cpp
                                Vulkan/VKProperties.cpp
                                Vulkan/DebugOutProperties.h
                                Vulkan/Debug/VKPhysicalDeviceCapsOutput.cpp
                                Vulkan/VKSurface.cpp)

SOURCE_GROUP("9.Vulkan\\Device\\PhysicalDevice" FILES ${VK_DEVICE_PHYSICAL_FILES})

SET(VK_DEVICE_LOGICAL_FILES ${SG_INCLUDE_PATH}/VKDevice.h
                            ${SG_INCLUDE_PATH}/VKDeviceAttribute.h
                            ${SG_INCLUDE_PATH}/VKDeviceCreater.h
                            Vulkan/VKDeviceAttribute.cpp
                            Vulkan/VKDeviceCreater.cpp
                            Vulkan/VKDevice.cpp)

SOURCE_GROUP("9.Vulkan\\Device\\Logical" FILES ${VK_DEVICE_LOGICAL_FILES})

SET(VK_DEVICE_OPERATIONS_FILES  Vulkan/VKDeviceMemory.cpp
                                Vulkan/VKDeviceBuffer.cpp
                                Vulkan/VKDeviceImage.cpp
                                Vulkan/VKDeviceSampler.cpp
                                Vulkan/VKDeviceMaterial.cpp
                                Vulkan/VKDeviceFramebuffer.cpp)

SOURCE_GROUP("9.Vulkan\\Device\\Operations" FILES ${VK_DEVICE_OPERATIONS_FILES})

SET(VK_DEVICE_DEBUG_FILES   ${SG_INCLUDE_PATH}/VKDebugOut.h
                            ${SG_INCLUDE_PATH}/VKDebugUtils.h
                            Vulkan/Debug/VKDebugOut.cpp
                            Vulkan/Debug/VKDebugUtils.cpp)

SOURCE_GROUP("9.Vulkan\\Device\\Debug" FILES ${VK_DEVICE_DEBUG_FILES})

IF(WIN32)
    OPTION(FORCE_DISCETE_GPU    "Force Discrete GPU" OFF)

    IF(FORCE_DISCETE_GPU)
        SET(VULKAN_RENDER_DEVICE_SOURCE ForceDiscreteGPU.c)
    ENDIF()

    SET(VK_DEVICE_PLATFORM_FILES   Vulkan/Platform/WinVulkan.cpp)
ENDIF(WIN32)

IF(UNIX)

    IF(ANDROID)
        SET(VK_DEVICE_PLATFORM_FILES   Vulkan/platform/AndroidVulkan.cpp)
    ELSE()
        IF(APPLE)
            if(CMAKE_SYSTEM_NAME STREQUAL "iOS" OR CMAKE_OSX_SYSROOT MATCHES ".*iPhone.*")
                message(STATUS "Targeting iOS")
                set(VK_DEVICE_PLATFORM_FILES Vulkan/platform/iOSVulkan.cpp)
            elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_OSX_SYSROOT MATCHES ".*MacOSX.*")
                message(STATUS "Targeting macOS")
                set(VK_DEVICE_PLATFORM_FILES Vulkan/platform/MacVulkan.cpp)
            else()
                message(FATAL_ERROR "Unknown Apple platform")
            endif()
        ELSE()
            find_package(PkgConfig REQUIRED)
            pkg_check_modules(WAYLAND_CLIENT wayland-client)
            if(WAYLAND_CLIENT_FOUND)
                message(STATUS "Targeting Wayland")
                set(VK_DEVICE_PLATFORM_FILES Vulkan/platform/WaylandVulkan.cpp)
            else()
                message(STATUS "Targeting XCB")
                set(VK_DEVICE_PLATFORM_FILES Vulkan/platform/XCBVulkan.cpp)
            endif()
        ENDIF()
    ENDIF()

ENDIF(UNIX)

SOURCE_GROUP("9.Vulkan\\Device\\Platform" FILES ${VK_DEVICE_PLATFORM_FILES})

# --- 9.2 Memory Management ---
SET(VK_MEMORY_FILES ${SG_INCLUDE_PATH}/VKMemory.h
                    ${SG_INCLUDE_PATH}/VKMemoryAllocator.h
                    ${SG_INCLUDE_PATH}/VKBuffer.h
                    ${SG_INCLUDE_PATH}/VKIndexBuffer.h
                    ${SG_INCLUDE_PATH}/VKArrayBuffer.h
                    ${SG_INCLUDE_PATH}/VKBufferMap.h
                    Vulkan/VKMemory.cpp
                    Vulkan/VKMemoryAllocator.cpp
                    Vulkan/VKBuffer.cpp
                    Vulkan/VKArrayBuffer.cpp)

SOURCE_GROUP("9.Vulkan\\Memory" FILES ${VK_MEMORY_FILES})

# --- 9.3 Pipeline ---
SET(PIPELINE_HEADER_PATH   ${SG_INCLUDE_PATH}/pipeline)
SET(PIPELINE_SOURCE_PATH   Vulkan/pipeline)

SET(VK_PIPELINE_FILES   ${PIPELINE_HEADER_PATH}/VKPipeline.h
                        ${PIPELINE_HEADER_PATH}/VKPipelineData.h
                        ${PIPELINE_HEADER_PATH}/VKPipelineHash.h
                        ${PIPELINE_HEADER_PATH}/VKInlinePipeline.h
                        ${PIPELINE_SOURCE_PATH}/VKPipelineCompare.cpp
                        ${PIPELINE_SOURCE_PATH}/VKPipelineData.cpp
                        ${PIPELINE_SOURCE_PATH}/VKPipelineDataManage.cpp
                        ${PIPELINE_SOURCE_PATH}/VKPipelineDataSaveLoad.cpp
                        ${PIPELINE_SOURCE_PATH}/VKPipelineHash.cpp
                        ${PIPELINE_SOURCE_PATH}/VKPipeline.cpp
                        ${PIPELINE_SOURCE_PATH}/VKPipelineCache.cpp
                        ${PIPELINE_SOURCE_PATH}/VKPipelineLayoutData.cpp
                        ${PIPELINE_SOURCE_PATH}/VKPipelineLayoutData.h)

SOURCE_GROUP("9.Vulkan\\Pipeline" FILES ${VK_PIPELINE_FILES})

# --- 9.4 Descriptor Sets ---
SET(VK_DESCRIPTOR_SETS_FILES    ${SG_INCLUDE_PATH}/VKDescriptorSet.h
                                Vulkan/VKDescriptorSet.cpp)

SOURCE_GROUP("9.Vulkan\\DescriptorSets" FILES ${VK_DESCRIPTOR_SETS_FILES})

# --- 9.5 Shader ---
SET(VK_SHADER_FILES ${SG_INCLUDE_PATH}/VKShaderModule.h
                    ${SG_INCLUDE_PATH}/VKShaderModuleMap.h
                    Vulkan/VKShaderModuleMap.cpp
                    Vulkan/VKShaderModule.cpp)

SOURCE_GROUP("9.Vulkan\\Shader" FILES ${VK_SHADER_FILES})

# --- 9.6 Texture ---
SET(VK_TEXTURE_CORE_FILES   ${SG_INCLUDE_PATH}/VKImageView.h
                            ${SG_INCLUDE_PATH}/VKTexture.h
                            ${SG_INCLUDE_PATH}/VKSampler.h
                            Vulkan/VKSampler.cpp
                            Vulkan/VKImageView.cpp
                            Vulkan/VKTexture.cpp)

SOURCE_GROUP("9.Vulkan\\Texture\\Core" FILES ${VK_TEXTURE_CORE_FILES})

SET(VK_TEXTURE_DEVICE_FILES Vulkan/Texture/BufferImageCopy2D.h
                            Vulkan/Texture/GenMipmaps.cpp
                            Vulkan/Texture/VKDeviceTexture.cpp
                            Vulkan/Texture/VKDeviceTexture2D.cpp
                            Vulkan/Texture/VKDeviceTexture2DArray.cpp
                            Vulkan/Texture/VKDeviceTextureCube.cpp)

SOURCE_GROUP("9.Vulkan\\Texture\\Device" FILES ${VK_TEXTURE_DEVICE_FILES})

SET(VK_TEXTURE_LOADER_FILES ${SG_INCLUDE_PATH}/VKTextureCreateInfo.h
                            Vulkan/Texture/VKTextureLoader.h
                            Vulkan/Texture/VKTexture2DLoader.cpp
                            Vulkan/Texture/VKTexture2DArrayLoader.cpp
                            Vulkan/Texture/VKTextureCubeLoader.cpp)

SOURCE_GROUP("9.Vulkan\\Texture\\Loader" FILES ${VK_TEXTURE_LOADER_FILES})

# --- 9.7 Material ---
SET(VK_MATERIAL_CORE_FILES  ${SG_INCLUDE_PATH}/VKMaterial.h
                            ${SG_INCLUDE_PATH}/VKMaterialParameters.h
                            ${SG_INCLUDE_PATH}/VKMaterialInstance.h
                            ${SG_INCLUDE_PATH}/VKDescriptorBindingManage.h
                            ${SG_INCLUDE_PATH}/VKDynamicBufferAccess.h
                            Vulkan/VKDescriptorBindingManage.cpp
                            Vulkan/VKMaterial.cpp
                            Vulkan/VKMaterialParameters.cpp
                            Vulkan/VKMaterialInstance.cpp)

SOURCE_GROUP("9.Vulkan\\Material\\Core" FILES ${VK_MATERIAL_CORE_FILES})

SET(VK_MATERIAL_MANAGER_FILES   ${SG_INCLUDE_PATH}/VKMaterialDescriptorManager.h
                                Vulkan/VKMaterialDescriptorManager.cpp)

SOURCE_GROUP("9.Vulkan\\Material\\Manager" FILES ${VK_MATERIAL_MANAGER_FILES})

# --- 9.8 Render Pass & Target ---
SET(VK_RENDER_PASS_FILES    ${SG_INCLUDE_PATH}/VKFramebuffer.h
                            ${SG_INCLUDE_PATH}/VKRenderPass.h
                            ${SG_INCLUDE_PATH}/VKSwapchain.h
                            ${SG_INCLUDE_PATH}/VKSemaphore.h
                            ${SG_INCLUDE_PATH}/VKFence.h
                            Vulkan/VKQueue.cpp
                            Vulkan/VKSemaphore.cpp
                            Vulkan/VKFence.cpp
                            Vulkan/VKFramebuffer.cpp
                            Vulkan/VKRenderPass.cpp
                            Vulkan/VKSwapchain.cpp)

SOURCE_GROUP("9.Vulkan\\RenderPass" FILES ${VK_RENDER_PASS_FILES})

SET(VK_RENDER_TARGET_FILES  ${SG_INCLUDE_PATH}/VKRenderTarget.h
                            ${SG_INCLUDE_PATH}/VKRenderTargetData.h
                            ${SG_INCLUDE_PATH}/VKRenderTargetSingle.h
                            ${SG_INCLUDE_PATH}/VKRenderTargetMultiFrame.h
                            ${SG_INCLUDE_PATH}/VKRenderTargetSwapchain.h
                            Vulkan/VKRenderTarget.cpp
                            Vulkan/VKRenderTargetData.cpp
                            Vulkan/VKSwapchainRenderTarget.cpp)

SOURCE_GROUP("9.Vulkan\\RenderTarget" FILES ${VK_RENDER_TARGET_FILES})

SET(VK_RENDER_CONTEXT_FILES ${SG_INCLUDE_PATH}/VKRenderContext.h
                            ${SG_INCLUDE_PATH}/VKSubpass.h
                            Vulkan/VKSubpass.cpp
                            Vulkan/VKRenderContext.cpp)

SOURCE_GROUP("9.Vulkan\\RenderContext" FILES ${VK_RENDER_CONTEXT_FILES})

# --- 9.9 Command Buffer ---
SET(VK_COMMAND_BUFFER_FILES ${SG_INCLUDE_PATH}/VKCommandBuffer.h
                            ${SG_INCLUDE_PATH}/VKIndirectCommandBuffer.h
                            Vulkan/VKIndirectCommandBuffer.cpp
                            Vulkan/VKCommandBuffer.cpp
                            Vulkan/VKCommandBufferRender.cpp)

SOURCE_GROUP("9.Vulkan\\CommandBuffer" FILES ${VK_COMMAND_BUFFER_FILES})

# ============================================================================
# Combine all files
# ============================================================================

SET(SCENE_GRAPH_FILES   ${SCENE_CORE_FILES}
                        ${SCENE_ENVIRONMENT_FILES}
                        ${RENDERING_CORE_FILES}
                        ${RENDERING_LINE_FILES}
                        ${RENDERING_LIGHTING_FILES})

SET(VULKAN_ALL_FILES    ${VK_MATERIAL_MANAGER_FILES}
                        ${VK_DEVICE_INSTANCE_FILES}
                        ${VK_DEVICE_DEBUG_FILES}
                        ${VK_MEMORY_FILES}
                        ${VK_DEVICE_OPERATIONS_FILES}
                        ${VK_DEVICE_LOGICAL_FILES}
                        ${VK_TEXTURE_DEVICE_FILES}
                        ${VK_TEXTURE_LOADER_FILES}
                        ${VK_DEVICE_PHYSICAL_FILES}
                        ${VK_DESCRIPTOR_SETS_FILES}
                        ${VK_SHADER_FILES}
                        ${VK_TEXTURE_CORE_FILES}
                        ${VK_PIPELINE_FILES}
                        ${VK_MATERIAL_CORE_FILES}
                        ${VK_RENDER_PASS_FILES}
                        ${VK_RENDER_TARGET_FILES}
                        ${VK_COMMAND_BUFFER_FILES}
                        ${GEOMETRY_MESH_FILES}
                        ${VULKAN_RENDER_DEVICE_SOURCE}
                        ${GLTF_SOURCE})

# ============================================================================
# Build Library
# ============================================================================

add_cm_library(ULRE.SceneGraph "ULRE"   ${SCENE_GRAPH_FILES}
                                        ${GEOMETRY_CORE_FILES}
                                        ${GEOMETRY_FILES}

                                        ${GLTF_SOURCE}

                                        ${CAMERA_FILES}

                                        ${GRAPH_MODULE_FILES}
                                        ${GRAPH_FRAMEWORK_FILES}

                                        ${TEXTURE_LOADER_FILES}
                                        ${TEXTURE_TILE_FILES}
                                        ${GEOMETRY_VERTEX_DATA_FILES}

                                        ${COMPONENT_FILES}
                                        ${COLLISION_COMPONENT_FILES}

                                        ${FONT_ALL_FILES}

                                        ${VULKAN_ALL_FILES}
                                        ${VK_DEVICE_PLATFORM_FILES}
)
