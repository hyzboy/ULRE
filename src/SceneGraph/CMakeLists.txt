# ============================================================================
# ULRE.SceneGraph
#
# 目录约定：
#   · 头文件在 inc/hgl/graph/<子系统>/，实现在 src/SceneGraph/<子系统>/
#   · 本文件的 SOURCE_GROUP 编号与两者一致，子目录按“子系统\区块”两级分组
#   · 纯头文件（UBO/SSBO 布局、Spec、.inl）也登记进来，只为让 IDE 有完整视图
#
# 分组编号（两位数字便于排序）：
#   00.Foundation  数据/类型/GPU 布局/派生产物，不依赖任何子系统
#   01.Core        GraphicsContext 与 GraphModule 模块管理
#   02.Scene       场景级数据（环境、变换）
#   03.Camera      相机与投影
#   04.Render      渲染上下文、着色器程序、阴影
#   05.Gizmo       交互手柄
#   06.Geometry    几何体生成与网格
#   07.Asset       资产（URI/工厂/各类资产对象与注册表）
#   08.Texture     纹理加载（平台无关 + Vulkan 后端）
#   09.Tile        瓦片数据
#   10.Font        字体与文字排版
#
# 新增文件时请登记到对应分组，不要为“能编译”而散落到别的组里。
# ============================================================================

set(SG_INCLUDE_PATH ${ROOT_INCLUDE_PATH}/hgl/graph)

# ============================================================================
# 00.Foundation
# ============================================================================

set(SG_DATA_FILES   ${SG_INCLUDE_PATH}/data/BitmapData.h
                    ${SG_INCLUDE_PATH}/data/ImageRegion.h)

source_group("00.Foundation\\Data" FILES ${SG_DATA_FILES})

# --- 顶层类型：图中转声明、缓冲结构注解、shader 缓冲来源列表、根地址下发 ---
set(SG_TYPE_FILES   ${SG_INCLUDE_PATH}/GraphTypes.h
                    ${SG_INCLUDE_PATH}/BufferAnnotation.h
                    ${SG_INCLUDE_PATH}/ShaderBufferSource.h
                    ${SG_INCLUDE_PATH}/ShaderBufferSources.h
                    ${SG_INCLUDE_PATH}/RootAddressPush.h)

source_group("00.Foundation\\Types" FILES ${SG_TYPE_FILES})

# --- GPU 结构布局：UBO/SSBO 的 C++ 唯一真源（GLSL 派生物由 AssetGen 生成）---
# 与环境/阴影强相关的 ubo 头文件归入各自子系统分组，见 02.Scene / 04.Render。
set(SG_GPU_LAYOUT_FILES ${SG_INCLUDE_PATH}/ubo/ColorPalette.h
                        ${SG_INCLUDE_PATH}/ubo/GlobalAddresses.h
                        ${SG_INCLUDE_PATH}/ubo/ViewportInfo.h
                        ${SG_INCLUDE_PATH}/ubo/WorldAddresses.h
                        ${SG_INCLUDE_PATH}/ssbo/SSBOTypes.h
                        ${SG_INCLUDE_PATH}/ssbo/GlobalSSBOTypes.h
                        ${SG_INCLUDE_PATH}/ssbo/LitMaterialData.h
                        ${SG_INCLUDE_PATH}/ssbo/MaterialDataRows.h
                        ${SG_INCLUDE_PATH}/ssbo/MaterialSSBOLayout.h)

source_group("00.Foundation\\GpuLayout" FILES ${SG_GPU_LAYOUT_FILES})

# --- 派生产物（由 scripts/run-assetgen.ps1 生成，不得手改）---
set(SG_GENERATED_FILES  ${SG_INCLUDE_PATH}/generated/buffer_schema.fbs
                        ${SG_INCLUDE_PATH}/generated/BufferToml.gen.h)

source_group("00.Foundation\\Generated" FILES ${SG_GENERATED_FILES})

set(SG_FOUNDATION_FILES ${SG_DATA_FILES}
                        ${SG_TYPE_FILES}
                        ${SG_GPU_LAYOUT_FILES}
                        ${SG_GENERATED_FILES})

# ============================================================================
# 01.Core
# ============================================================================

set(SG_GRAPHICS_CONTEXT_FILES   ${SG_INCLUDE_PATH}/core/GraphicsContext.h
                                render/GraphicsContext.cpp)

source_group("01.Core\\GraphicsContext" FILES ${SG_GRAPHICS_CONTEXT_FILES})

set(SG_MODULE_FILES ${SG_INCLUDE_PATH}/module/GraphModule.h
                    ${SG_INCLUDE_PATH}/module/GraphModuleManager.h
                    ${SG_INCLUDE_PATH}/module/RenderPassManager.h
                    ${SG_INCLUDE_PATH}/module/OffscreenSlotBands.h
                    ${SG_INCLUDE_PATH}/module/OffscreenWorld.h
                    ${SG_INCLUDE_PATH}/module/RenderTargetManager.h
                    ${SG_INCLUDE_PATH}/module/SwapchainModule.h
                    ${SG_INCLUDE_PATH}/module/TextureManager.h
                    ${SG_INCLUDE_PATH}/module/TextureUploadTask.h
                    ${SG_INCLUDE_PATH}/module/TextureUploadQueue.h
                    ${SG_INCLUDE_PATH}/module/TextureLoadCommitter.h
                    ${SG_INCLUDE_PATH}/module/SamplerManager.h
                    ${SG_INCLUDE_PATH}/module/ShaderProgramManager.h
                    ${SG_INCLUDE_PATH}/module/ShaderProgramLinkCache.h
                    ${SG_INCLUDE_PATH}/module/ShaderStageModuleCache.h
                    ${SG_INCLUDE_PATH}/module/ShaderProgramCreatePrecheckAdapter.h
                    ${SG_INCLUDE_PATH}/module/BufferManager.h
                    ${SG_INCLUDE_PATH}/module/GeometryManager.h
                    ${SG_INCLUDE_PATH}/module/SSBOBufferRegistry.h
                    ${SG_INCLUDE_PATH}/module/GlobalSSBOBufferRegistry.h
                    ${SG_INCLUDE_PATH}/module/MaterialTextureReferencePool.h
                    ${SG_INCLUDE_PATH}/module/EnvironmentManager.h
                    module/GraphModule.cpp
                    module/GraphModuleManager.cpp
                    module/RenderPassManager.cpp
                    module/OffscreenWorld.cpp
                    module/RenderTargetManager.cpp
                    module/SwapChainModule.cpp
                    module/TextureManager.cpp
                    module/TextureUploadQueue.cpp
                    module/TextureLoadCommitter.cpp
                    module/SamplerManager.cpp
                    module/ShaderProgramManager.cpp
                    module/ShaderProgramCreatePrecheckAdapter.cpp
                    module/BufferManager.cpp
                    module/GeometryManager.cpp
                    module/SSBOBufferRegistry.cpp
                    module/GlobalSSBOBufferRegistry.cpp
                    module/MaterialTextureReferencePool.cpp
                    module/EnvironmentManager.cpp)

source_group("01.Core\\Module" FILES ${SG_MODULE_FILES})

set(SG_CORE_FILES   ${SG_GRAPHICS_CONTEXT_FILES}
                    ${SG_MODULE_FILES})

# ============================================================================
# 02.Scene
# ============================================================================

set(SG_SCENE_ENVIRONMENT_FILES  ${SG_INCLUDE_PATH}/ubo/EnvironmentInfo.h
                                ${SG_INCLUDE_PATH}/ubo/LightRow.h
                                ${SG_INCLUDE_PATH}/ubo/AmbientEnvInfo.h
                                ${SG_INCLUDE_PATH}/sky/SkySH9.h)

source_group("02.Scene\\Environment" FILES ${SG_SCENE_ENVIRONMENT_FILES})

set(SG_SCENE_TRANSFORM_FILES    ${SG_INCLUDE_PATH}/transform/TransformAction.h)

source_group("02.Scene\\Transform" FILES ${SG_SCENE_TRANSFORM_FILES})

set(SG_SCENE_FILES  ${SG_SCENE_ENVIRONMENT_FILES}
                    ${SG_SCENE_TRANSFORM_FILES})

# ============================================================================
# 03.Camera
# ============================================================================

set(SG_CAMERA_FILES ${SG_INCLUDE_PATH}/camera/Camera.h
                    ${SG_INCLUDE_PATH}/camera/ReversedZProj.h
                    camera/Camera.cpp
                    camera/ReversedZProj.cpp)

source_group("03.Camera" FILES ${SG_CAMERA_FILES})

# ============================================================================
# 04.Render
# ============================================================================

set(SG_RENDER_CORE_FILES    ${SG_INCLUDE_PATH}/render/RenderContext.h
                            ${SG_INCLUDE_PATH}/render/RenderItemDescriptor.h
                            ${SG_INCLUDE_PATH}/render/RenderTargetDesc.h
                            ${ROOT_INCLUDE_PATH}/hgl/vk/VKShaderProgram.h
                            render/RenderContext.cpp
                            render/VKShaderProgram.cpp)

source_group("04.Render\\Core" FILES ${SG_RENDER_CORE_FILES})

set(SG_RENDER_SHADOW_FILES  ${SG_INCLUDE_PATH}/render/shadow/CascadedShadowController.h
                            ${SG_INCLUDE_PATH}/ubo/ShadowInfo.h
                            render/shadow/CascadedShadowController.cpp)

source_group("04.Render\\Shadow" FILES ${SG_RENDER_SHADOW_FILES})

set(SG_RENDER_FILES ${SG_RENDER_CORE_FILES}
                    ${SG_RENDER_SHADOW_FILES})

# ============================================================================
# 05.Gizmo
# ============================================================================

set(SG_GIZMO_FILES  ${SG_INCLUDE_PATH}/gizmo/GizmoTypes.h
                    ${SG_INCLUDE_PATH}/gizmo/TransformGizmoSystem.h
                    ${SG_INCLUDE_PATH}/gizmo/SunDirectionControlSystem.h
                    gizmo/Gizmo.h
                    gizmo/GizmoInternal.h
                    gizmo/GizmoResource.h
                    gizmo/GizmoUnified.AssetCore.inl
                    gizmo/GizmoUnified.AssetVisual.inl
                    gizmo/GizmoUnified.AssetUpdate.inl
                    gizmo/GizmoResource.cpp
                    gizmo/GizmoUnified.cpp
                    gizmo/TransformGizmoSystem.cpp
                    gizmo/SunDirectionControlSystem.cpp)

source_group("05.Gizmo\\Core" FILES ${SG_GIZMO_FILES})

set(SG_GIZMO_MODE_FILES gizmo/modes/GizmoModeRuntime.h
                        gizmo/modes/MoveGizmoMode.h
                        gizmo/modes/MoveGizmoMode.Visual.inl
                        gizmo/modes/MoveGizmoMode.Input.inl
                        gizmo/modes/RotateGizmoMode.h
                        gizmo/modes/RotateGizmoMode.Visual.inl
                        gizmo/modes/RotateGizmoMode.Input.inl
                        gizmo/modes/ScaleGizmoMode.h
                        gizmo/modes/ScaleGizmoMode.Visual.inl
                        gizmo/modes/ScaleGizmoMode.Input.inl)

source_group("05.Gizmo\\Modes" FILES ${SG_GIZMO_MODE_FILES})

set(SG_GIZMO_ALL_FILES  ${SG_GIZMO_FILES}
                        ${SG_GIZMO_MODE_FILES})

# ============================================================================
# 06.Geometry
# ============================================================================

set(SG_GEO_CORE_FILES   ${SG_INCLUDE_PATH}/geo/VKGeometry.h
                        ${SG_INCLUDE_PATH}/geo/VKGeometryData.h
                        ${SG_INCLUDE_PATH}/geo/GeometryVertexFormat.h
                        ${SG_INCLUDE_PATH}/geo/AttributeCompatibility.h
                        ${SG_INCLUDE_PATH}/geo/LoadGeometry.h
                        ${SG_INCLUDE_PATH}/geo/GeometryCreater.h
                        ${SG_INCLUDE_PATH}/geo/GeometryBuilder.h
                        ${SG_INCLUDE_PATH}/geo/InlineGeometry.h
                        ${SG_INCLUDE_PATH}/geo/IndexGenerator.h
                        ${SG_INCLUDE_PATH}/geo/Extruded.h
                        ${SG_INCLUDE_PATH}/geo/Wall.h
                        geo/VKGeometry.cpp
                        geo/VKGeometryData.cpp
                        geo/AttributeCompatibilityRegistry.cpp
                        geo/GeometryLoader.cpp
                        geo/GeometryCreater.cpp)

source_group("06.Geometry\\Core" FILES ${SG_GEO_CORE_FILES})

# geo/Spline.cpp 是模板实现文件，不被任何 TU 独立编译，只登记给 IDE 显示。
set(SG_GEO_TEMPLATE_FILES ${SG_INCLUDE_PATH}/geo/Spline.cpp)
set_source_files_properties(${SG_GEO_TEMPLATE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)

source_group("06.Geometry\\Core" FILES ${SG_GEO_TEMPLATE_FILES})

set(SG_MESH_FILES   ${SG_INCLUDE_PATH}/mesh/GeometryDataBuffer.h
                    ${SG_INCLUDE_PATH}/mesh/GeometryDrawRange.h
                    ${SG_INCLUDE_PATH}/mesh/NTB.h
                    ${SG_INCLUDE_PATH}/mesh/Primitive.h
                    ${SG_INCLUDE_PATH}/mesh/StaticMesh.h
                    ${SG_INCLUDE_PATH}/mesh/LoadStaticMesh.h
                    ${SG_INCLUDE_PATH}/mesh/SceneStreaming.h
                    mesh/Primitive.cpp
                    mesh/StaticMesh.cpp
                    mesh/LoadStaticMesh.cpp
                    mesh/SceneStreaming.cpp)

source_group("06.Geometry\\Mesh" FILES ${SG_MESH_FILES})

set(SG_GEOMETRY_FILES   ${SG_GEO_CORE_FILES}
                        ${SG_GEO_TEMPLATE_FILES}
                        ${SG_MESH_FILES})

# ============================================================================
# 07.Asset
# ============================================================================

# AssetURI / AssetFactory 已下沉到 CMAssetsManage（inc/hgl/asset 路径不变），
# 本模块只消费、不再编译。

set(SG_ASSET_TEXTURE_FILES  ${SG_INCLUDE_PATH}/asset/TextureAsset.h
                            ${SG_INCLUDE_PATH}/asset/TextureURI.h
                            ${SG_INCLUDE_PATH}/asset/TextureArraySpec.h
                            ${SG_INCLUDE_PATH}/asset/RenderTargetAsset.h
                            asset/TextureAsset.cpp
                            asset/TextureURI.cpp
                            asset/TextureArraySpec.cpp
                            asset/RenderTargetAsset.cpp)

source_group("07.Asset\\Texture" FILES ${SG_ASSET_TEXTURE_FILES})

set(SG_ASSET_MATERIAL_FILES ${SG_INCLUDE_PATH}/asset/MaterialRecipeAsset.h
                            ${SG_INCLUDE_PATH}/asset/MaterialRecipeSpec.h
                            ${ROOT_INCLUDE_PATH}/hgl/mtl/MaterialRecipeRegistry.h
                            ${SG_INCLUDE_PATH}/asset/MaterialDataBlockAsset.h
                            ${SG_INCLUDE_PATH}/asset/MaterialDataBlockRegistry.h
                            ${SG_INCLUDE_PATH}/asset/MaterialDataBlockObject.h
                            ${SG_INCLUDE_PATH}/asset/MaterialDataBlockSpec.h
                            asset/MaterialRecipeAsset.cpp
                            asset/MaterialRecipeRegistry.cpp
                            asset/MaterialDataBlockAsset.cpp
                            asset/MaterialDataBlockRegistry.cpp
                            asset/MaterialDataBlockObject.cpp)

source_group("07.Asset\\Material" FILES ${SG_ASSET_MATERIAL_FILES})

set(SG_ASSET_ENVIRONMENT_FILES  ${SG_INCLUDE_PATH}/asset/EnvironmentAsset.h
                                ${SG_INCLUDE_PATH}/asset/EnvironmentAssetSpec.h
                                ${SG_INCLUDE_PATH}/asset/EnvironmentAssetRegistry.h
                                ${SG_INCLUDE_PATH}/asset/EnvironmentAssetObject.h
                                asset/EnvironmentAsset.cpp
                                asset/EnvironmentAssetRegistry.cpp
                                asset/EnvironmentAssetObject.cpp)

source_group("07.Asset\\Environment" FILES ${SG_ASSET_ENVIRONMENT_FILES})

set(SG_ASSET_GEOMETRY_FILES ${SG_INCLUDE_PATH}/asset/GeometryAssetSpec.h
                            ${SG_INCLUDE_PATH}/asset/GeometryAssetFile.h
                            ${SG_INCLUDE_PATH}/asset/GeometryAsset.h
                            ${SG_INCLUDE_PATH}/asset/GeometryAssetRegistry.h
                            ${SG_INCLUDE_PATH}/asset/GeometryAssetObject.h
                            ${SG_INCLUDE_PATH}/asset/GeometryPreset.h
                            asset/GeometryAssetFile.cpp
                            asset/GeometryAsset.cpp
                            asset/GeometryAssetRegistry.cpp
                            asset/GeometryAssetObject.cpp
                            asset/GeometryPreset.cpp)

source_group("07.Asset\\Geometry" FILES ${SG_ASSET_GEOMETRY_FILES})

set(SG_ASSET_SCENE_FILES    ${SG_INCLUDE_PATH}/asset/SceneAssetSpec.h
                            ${SG_INCLUDE_PATH}/asset/SceneAssetFile.h
                            ${SG_INCLUDE_PATH}/asset/SceneAssetRegistry.h
                            ${SG_INCLUDE_PATH}/asset/SceneLoader.h
                            asset/SceneAssetFile.cpp
                            asset/SceneAssetRegistry.cpp
                            asset/SceneLoader.cpp)

source_group("07.Asset\\Scene" FILES ${SG_ASSET_SCENE_FILES})

set(SG_ASSET_PRIMITIVE_FILES    ${SG_INCLUDE_PATH}/asset/PrimitiveAsset.h
                                ${SG_INCLUDE_PATH}/asset/PrimitiveAssetSpec.h
                                ${SG_INCLUDE_PATH}/asset/PrimitiveAssetFile.h
                                ${SG_INCLUDE_PATH}/asset/PrimitiveAssetRegistry.h
                                asset/PrimitiveAssetFile.cpp
                                asset/PrimitiveAssetRegistry.cpp)

source_group("07.Asset\\Primitive" FILES ${SG_ASSET_PRIMITIVE_FILES})

set(SG_ASSET_FILES  ${SG_ASSET_TEXTURE_FILES}
                    ${SG_ASSET_MATERIAL_FILES}
                    ${SG_ASSET_ENVIRONMENT_FILES}
                    ${SG_ASSET_GEOMETRY_FILES}
                    ${SG_ASSET_SCENE_FILES}
                    ${SG_ASSET_PRIMITIVE_FILES})

# ============================================================================
# 08.Texture
# ============================================================================

set(SG_TEXTURE_LOADER_FILES ${SG_INCLUDE_PATH}/texture/TextureLoader.h
                            ${SG_INCLUDE_PATH}/texture/TextureMemoryLoader.h
                            ${SG_INCLUDE_PATH}/texture/Bitmap2DLoader.h
                            texture/TextureLoader.cpp
                            texture/Bitmap2DLoader.cpp)

source_group("08.Texture\\Loader" FILES ${SG_TEXTURE_LOADER_FILES})

set(SG_TEXTURE_VK_FILES ${ROOT_INCLUDE_PATH}/hgl/vk/VKTextureLoader.h
                        texture/VKTextureLoader.h
                        texture/VKTexture2DLoader.cpp
                        texture/VKTexture2DArrayLoader.cpp
                        texture/VKTextureCubeLoader.cpp)

source_group("08.Texture\\Vulkan" FILES ${SG_TEXTURE_VK_FILES})

set(SG_TEXTURE_FILES    ${SG_TEXTURE_LOADER_FILES}
                        ${SG_TEXTURE_VK_FILES})

# ============================================================================
# 09.Tile
# ============================================================================

set(SG_TILE_FILES   ${SG_INCLUDE_PATH}/tile/TileData.h
                    ${SG_INCLUDE_PATH}/tile/TileObject.h
                    tile/TileData.cpp
                    tile/VKTileData.cpp)

source_group("09.Tile" FILES ${SG_TILE_FILES})

# ============================================================================
# 10.Font
# ============================================================================

set(SG_FONT_CORE_FILES  ${SG_INCLUDE_PATH}/font/Font.h
                        ${SG_INCLUDE_PATH}/font/CharQuadConfig.h
                        ${SG_INCLUDE_PATH}/font/TextCharSSBO.h
                        font/Font.cpp)

source_group("10.Font\\Core" FILES ${SG_FONT_CORE_FILES})

set(SG_FONT_SOURCE_FILES    ${SG_INCLUDE_PATH}/font/FontSource.h
                            font/FontDataSource.cpp
                            font/FontBitmapDataSource.cpp
                            font/FontDataSourceManage.cpp
                            font/FontSource.cpp)

source_group("10.Font\\DataSource" FILES ${SG_FONT_SOURCE_FILES})

set(SG_FONT_TILE_FILES  ${SG_INCLUDE_PATH}/font/TileFont.h
                        font/TileFont.cpp
                        font/VKTileFont.cpp)

source_group("10.Font\\TileFont" FILES ${SG_FONT_TILE_FILES})

set(SG_FONT_LAYOUT_FILES    ${SG_INCLUDE_PATH}/font/TextLayout.h
                            ${SG_INCLUDE_PATH}/font/TextLayoutEngine.h
                            font/TextLayout.cpp)

source_group("10.Font\\Layout" FILES ${SG_FONT_LAYOUT_FILES})

if(WIN32)
    set(SG_FONT_PLATFORM_FILES  font/FontDataSourceWin.h
                                font/FontDataSourceWin.cpp)

    source_group("10.Font\\Platform\\Windows" FILES ${SG_FONT_PLATFORM_FILES})
else()
    # Find FreeType for non-Windows platforms
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FREETYPE REQUIRED freetype2)

    set(SG_FONT_PLATFORM_FILES  font/FontDataSourceFreeType.h
                                font/FontDataSourceFreeType.cpp)

    source_group("10.Font\\Platform\\FreeType" FILES ${SG_FONT_PLATFORM_FILES})
endif(WIN32)

set(SG_FONT_FILES   ${SG_FONT_CORE_FILES}
                    ${SG_FONT_SOURCE_FILES}
                    ${SG_FONT_PLATFORM_FILES}
                    ${SG_FONT_TILE_FILES}
                    ${SG_FONT_LAYOUT_FILES})

# ============================================================================
# Build Library
# ============================================================================

add_cm_library(ULRE.SceneGraph "ULRE" ${SG_INCLUDE_PATH}

    ${SG_FOUNDATION_FILES}
    ${SG_CORE_FILES}
    ${SG_SCENE_FILES}
    ${SG_CAMERA_FILES}
    ${SG_RENDER_FILES}
    ${SG_GIZMO_ALL_FILES}
    ${SG_GEOMETRY_FILES}
    ${SG_ASSET_FILES}
    ${SG_TEXTURE_FILES}
    ${SG_TILE_FILES}
    ${SG_FONT_FILES}
)

target_include_directories(ULRE.SceneGraph PUBLIC ${ROOT_INCLUDE_PATH})
