set(SG_INCLUDE_PATH     ${ROOT_INCLUDE_PATH}/hgl/graph)

SET(CM_SG_HEADER    ${SG_INCLUDE_PATH}/data/BitmapData.h
                    ${SG_INCLUDE_PATH}/data/ImageRegion.h
                    ${SG_INCLUDE_PATH}/camera/TransformFaceToCamera.h
                    ${SG_INCLUDE_PATH}/camera/ViewportInfo.h)

source_group("SceneGraph" FILES ${CM_SG_HEADER})

# ============================================================================
# 1. Core Framework - 核心框架
# ============================================================================

SET(GRAPHICS_CONTEXT_FILES   ${SG_INCLUDE_PATH}/core/GraphicsContext.h
                            render/GraphicsContext.cpp)

SOURCE_GROUP("1.Core\\GraphicsContext" FILES ${GRAPHICS_CONTEXT_FILES})

SET(GRAPH_MODULE_FILES  ${SG_INCLUDE_PATH}/module/GraphModule.h
                        ${SG_INCLUDE_PATH}/module/GraphModuleManager.h
                        ${SG_INCLUDE_PATH}/module/RenderPassManager.h
                        ${SG_INCLUDE_PATH}/module/TextureManager.h
                        ${SG_INCLUDE_PATH}/module/SamplerManager.h
                        ${SG_INCLUDE_PATH}/module/MaterialManager.h
                        ${SG_INCLUDE_PATH}/module/MaterialCreatePrecheckAdapter.h
                        ${SG_INCLUDE_PATH}/module/MaterialFinalizeFlowAdapter.h
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
                        module/MaterialCreatePrecheckAdapter.cpp
                        module/MaterialFinalizeFlowAdapter.cpp
                        module/BufferManager.cpp
                        module/RenderTargetManager.cpp
                        module/GeometryManager.cpp
                        module/PrimitiveManager.cpp
                        module/SwapChainModule.cpp)

SOURCE_GROUP("1.Core\\Module" FILES ${GRAPH_MODULE_FILES})

# ============================================================================
# 2. Scene Management - 场景管理系统
# ============================================================================

SET(SCENE_ENVIRONMENT_FILES ${SG_INCLUDE_PATH}/env/SkyInfo.h
                            env/SkyInfo.cpp)

SOURCE_GROUP("2.Scene\\Environment" FILES ${SCENE_ENVIRONMENT_FILES})

# ============================================================================
# 3. Camera System - 相机系统
# ============================================================================

SET(CAMERA_FILES    ${SG_INCLUDE_PATH}/camera/Camera.h
                    ${SG_INCLUDE_PATH}/camera/RealityCamera.h
                    ${SG_INCLUDE_PATH}/camera/ReversedZProj.h
                    camera/Camera.cpp
                    camera/ReversedZProj.cpp)

SOURCE_GROUP("3.Camera" FILES ${CAMERA_FILES})

# ============================================================================
# 4. Rendering System - 渲染系统
# ============================================================================

SET(RENDERING_CORE_FILES    ${SG_INCLUDE_PATH}/render/RenderContext.h
                            render/RenderContext.cpp
)

SOURCE_GROUP("4.Rendering\\Core" FILES ${RENDERING_CORE_FILES})

SET(RENDERING_LIGHTING_FILES ${SG_INCLUDE_PATH}/render/lighting/Light.h
                             ${SG_INCLUDE_PATH}/render/lighting/ShadowPolicy.h)

SOURCE_GROUP("4.Rendering\\Lighting" FILES ${RENDERING_LIGHTING_FILES})

# ============================================================================
# 5. Gizmo System - Gizmo交互系统
# ============================================================================

SET(GIZMO_PUBLIC_HEADERS    ${SG_INCLUDE_PATH}/gizmo/GizmoTypes.h
                            ${SG_INCLUDE_PATH}/gizmo/TransformGizmoSystem.h
                            ${SG_INCLUDE_PATH}/gizmo/SunDirectionControlSystem.h)

SET(GIZMO_INTERNAL_FILES    gizmo/Gizmo.h
                            gizmo/GizmoInternal.h
                            gizmo/GizmoResource.h
                            gizmo/GizmoResource.cpp
                            gizmo/GizmoUnified.AssetCore.inl
                            gizmo/GizmoUnified.AssetVisual.inl
                            gizmo/GizmoUnified.AssetUpdate.inl
                            gizmo/modes/GizmoModeRuntime.h
                            gizmo/modes/MoveGizmoMode.h
                            gizmo/modes/MoveGizmoMode.Visual.inl
                            gizmo/modes/MoveGizmoMode.Input.inl
                            gizmo/modes/RotateGizmoMode.h
                            gizmo/modes/RotateGizmoMode.Visual.inl
                            gizmo/modes/RotateGizmoMode.Input.inl
                            gizmo/modes/ScaleGizmoMode.h
                            gizmo/modes/ScaleGizmoMode.Visual.inl
                            gizmo/modes/ScaleGizmoMode.Input.inl
                            gizmo/GizmoUnified.cpp
                            gizmo/TransformGizmoSystem.cpp
                            gizmo/SunDirectionControlSystem.cpp)

SET(GIZMO_ALL_FILES ${GIZMO_PUBLIC_HEADERS} ${GIZMO_INTERNAL_FILES})

SOURCE_GROUP("5.Gizmo" FILES ${GIZMO_ALL_FILES})

# ============================================================================
# 6. Geometry & Mesh System - 几何体与网格系统
# ============================================================================

SET(GEOMETRY_CORE_FILES ${SG_INCLUDE_PATH}/geo/VKGeometry.h
                        ${SG_INCLUDE_PATH}/geo/VKGeometryData.h
                        VKGeometry.cpp
                        VKGeometryData.cpp
                        ${SG_INCLUDE_PATH}/geo/GeometryCreater.h
                        geo/GeometryCreater.cpp)

SOURCE_GROUP("5.Geometry\\Core" FILES ${GEOMETRY_CORE_FILES})

SET(GEOMETRY_MESH_FILES ${SG_INCLUDE_PATH}/mesh/GeometryDataBuffer.h
                        ${SG_INCLUDE_PATH}/mesh/GeometryDrawRange.h
                        ${SG_INCLUDE_PATH}/mesh/Primitive.h
                        ${SG_INCLUDE_PATH}/mesh/StaticMesh.h
                        mesh/Primitive.cpp
                        mesh/StaticMesh.cpp)

SOURCE_GROUP("6.Geometry\\Mesh" FILES ${GEOMETRY_MESH_FILES})

SET(ASSET_REGISTRY_FILES    ${SG_INCLUDE_PATH}/asset/AssetWorldDef.h
                            ${SG_INCLUDE_PATH}/asset/AssetWorldRegistry.h
                            asset/AssetWorldRegistry.cpp)

SOURCE_GROUP("6.Asset\\Registry" FILES ${ASSET_REGISTRY_FILES})

# ============================================================================
# 7. Texture System - 纹理系统
# ============================================================================

SET(TEXTURE_LOADER_FILES    ${SG_INCLUDE_PATH}/texture/TextureLoader.h
                            ${SG_INCLUDE_PATH}/texture/Bitmap2DLoader.h
                            texture/TextureLoader.cpp
                            texture/Bitmap2DLoader.cpp)

SOURCE_GROUP("6.Texture\\Loader" FILES ${TEXTURE_LOADER_FILES})

SET(TEXTURE_TILE_FILES  ${SG_INCLUDE_PATH}/tile/TileData.h
                        tile/TileData.cpp
                        VKTileData.cpp
                        VKTileFont.cpp)

SOURCE_GROUP("7.Texture\\Tile" FILES ${TEXTURE_TILE_FILES})

# ============================================================================
# 8. Font System - 字体系统
# ============================================================================

SET(FONT_CORE_FILES ${SG_INCLUDE_PATH}/font/Font.h
                    font/Font.cpp)

SOURCE_GROUP("8.Font\\Core" FILES ${FONT_CORE_FILES})

SET(FONT_DATA_SOURCE_FILES  ${SG_INCLUDE_PATH}/font/FontSource.h
                            font/FontDataSource.cpp
                            font/FontBitmapDataSource.cpp
                            font/FontDataSourceManage.cpp
                            font/FontSource.cpp)

SOURCE_GROUP("8.Font\\DataSource" FILES ${FONT_DATA_SOURCE_FILES})

SET(FONT_TILE_FILES ${SG_INCLUDE_PATH}/font/TileFont.h
                    font/TileFont.cpp)

SOURCE_GROUP("8.Font\\TileFont" FILES ${FONT_TILE_FILES})

SET(FONT_LAYOUT_FILES   ${SG_INCLUDE_PATH}/font/TextLayout.h
                        ${SG_INCLUDE_PATH}/font/TextLayoutEngine.h
                        font/TextLayout.cpp)

SOURCE_GROUP("8.Font\\Layout" FILES ${FONT_LAYOUT_FILES})

SET(FONT_RENDERING_FILES    ${SG_INCLUDE_PATH}/font/TextGeometry.h
                            ${SG_INCLUDE_PATH}/font/TextRender.h
                            font/TextRender.cpp
                            font/TextGeometry.cpp)

SOURCE_GROUP("8.Font\\Rendering" FILES ${FONT_RENDERING_FILES})

IF(WIN32)
    SET(FONT_PLATFORM_FILES font/FontDataSourceWin.cpp
                            font/FontDataSourceWin.h)

    SOURCE_GROUP("8.Font\\Platform\\Windows" FILES ${FONT_PLATFORM_FILES})
ELSE()
    # Find FreeType for non-Windows platforms
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FREETYPE REQUIRED freetype2)

    SET(FONT_PLATFORM_FILES font/FontDataSourceFreeType.cpp
                            font/FontDataSourceFreeType.h)

    SOURCE_GROUP("8.Font\\Platform\\FreeType" FILES ${FONT_PLATFORM_FILES})
ENDIF(WIN32)

SET(FONT_ALL_FILES  ${FONT_CORE_FILES}
                    ${FONT_DATA_SOURCE_FILES}
                    ${FONT_PLATFORM_FILES}
                    ${FONT_TILE_FILES}
                    ${FONT_LAYOUT_FILES}
                    ${FONT_RENDERING_FILES})

# ============================================================================
# 9. Vulkan - 高层渲染逻辑（几何体、材质、纹理加载等）
# ============================================================================

# --- 8.1 Geometry Management ---
SET(VK_GEOMETRY_FILES   ${SG_INCLUDE_PATH}/geo/VKGeometry.h
                        ${SG_INCLUDE_PATH}/geo/VKGeometryData.h
                        VKGeometry.cpp
                        VKGeometryData.cpp)

SOURCE_GROUP("9.Vulkan\\Geometry" FILES ${VK_GEOMETRY_FILES})

# --- 8.2 Material System ---
SET(VK_SCENEGRAPH_MATERIAL_FILES    ${ROOT_INCLUDE_PATH}/hgl/vk/VKMaterial.h
                                    ${ROOT_INCLUDE_PATH}/hgl/vk/VKMaterialParameters.h
                                    ${ROOT_INCLUDE_PATH}/hgl/vk/VKMaterialInstance.h
                                    ${ROOT_INCLUDE_PATH}/hgl/vk/VKMaterialDescriptorManager.h
                                    ${ROOT_INCLUDE_PATH}/hgl/vk/VKResourceDomain.h
                                    ${ROOT_INCLUDE_PATH}/hgl/vk/VKDomainMaterialBinding.h
                                    VKMaterial.cpp
                                    VKMaterialParameters.cpp
                                    VKMaterialInstance.cpp
                                    VKMaterialDescriptorManager.cpp)

SOURCE_GROUP("9.Vulkan\\Material" FILES ${VK_SCENEGRAPH_MATERIAL_FILES})

# --- 8.4 Texture Loader ---
SET(VK_TEXTURE_LOADER_FILES     ${ROOT_INCLUDE_PATH}/hgl/vk/VKTextureLoader.h
                                ${ROOT_INCLUDE_PATH}/hgl/vk/VKTextureType.h
                                Texture/VKTextureLoader.h
                                Texture/VKTexture2DLoader.cpp
                                Texture/VKTexture2DArrayLoader.cpp
                                Texture/VKTextureCubeLoader.cpp)

SOURCE_GROUP("9.Vulkan\\TextureLoader" FILES ${VK_TEXTURE_LOADER_FILES})

# --- 9. SSBO Vertex Fetch ---
SET(SSBO_VERTEX_FETCH_FILES     ${SG_INCLUDE_PATH}/VertexDataBufferManager.h
                                VertexDataBufferManager.cpp)

SOURCE_GROUP("SSBO VertexFetch" FILES ${SSBO_VERTEX_FETCH_FILES})

# ============================================================================
# Combine all files
# ============================================================================

SET(SCENE_GRAPH_FILES   ${SCENE_ENVIRONMENT_FILES}
                        ${RENDERING_CORE_FILES}
                        ${RENDERING_LIGHTING_FILES}
                        ${GIZMO_ALL_FILES})

# ============================================================================
# Build Library
# ============================================================================

add_cm_library(ULRE.SceneGraph "ULRE"   ${SG_INCLUDE_PATH}

                                        ${SCENE_GRAPH_FILES}
                                        ${GEOMETRY_CORE_FILES}

                                        ${GLTF_SOURCE}

                                        ${CAMERA_FILES}

                                        ${GRAPH_MODULE_FILES}
                                        ${GRAPHICS_CONTEXT_FILES}

                                        ${TEXTURE_LOADER_FILES}
                                        ${TEXTURE_TILE_FILES}
                                        ${GEOMETRY_VERTEX_DATA_FILES}

                                        ${COMPONENT_FILES}
                                        ${COLLISION_COMPONENT_FILES}

                                        ${FONT_ALL_FILES}

                                        ${GEOMETRY_MESH_FILES}
                                        ${ASSET_REGISTRY_FILES}

                                        ${VK_GEOMETRY_FILES}
                                        ${VK_SCENEGRAPH_MATERIAL_FILES}
                                        ${VK_TEXTURE_LOADER_FILES}

                                        ${CM_SG_HEADER}
                                        ${CM_SG_SOURCE}
                                        ${CM_SG_GEO_FILES}
                                        ${CM_CAMERA_SOURCE}

                                        ${SSBO_VERTEX_FETCH_FILES}
)
