set(SHADERGEN_INCLUDE_PATH  ${ROOT_INCLUDE_PATH}/hgl/mtl)

# ── 物理目录 = SOURCE_GROUP = 管线角色（2026-09 目录整理，对齐模块隔离设计 §四）──
#   compile/             编译执行：glslang 封装 + 构建上下文 + SPV 产物 + 求解/发射
#                        （含 ShaderGenProgramBuildBackend.cpp：生成器侧的装配后端实现）
#   glsl_module/         GLSL 代码模块体系（registry/metadata/file/manifest/preset）
#   contract/            契约层（结构校验/哈希/绑定解析/输出/覆盖率/接口）
#   meshgen/             mesh shader 生成（装配器 + 模式子模块 + 模板）
#   template/            fragment/mesh 模板发射器
#   builder/             定义 → 构建计划编排（generic builder/顶点 ABI/描述符推导）
# 公共头（inc/hgl/mtl 与 inc/hgl/graph）不参与物理搬移，路径保持稳定。
#
# 本清单只收「ShaderGen 独占」的头与源。数据面（ULRE.ShaderMaterialData）已经
# 拥有源文件或使用者的头，其 CMake 归属已下移到那一层，本清单不再重复列出：
#   ShaderLibraryPath / VkEnumText / ShaderCacheRoot / ShaderBuildContext /
#   ShaderCreateInfo(+Map) / ShaderArtifactStore / MaterialDefinitionFile /
#   MaterialDefinitionRegistry / SamplerPreset / RenderTemplate /
#   SceneRenderTemplateResolver
# 设备 profile 契约头与 MeshShaderLimits 归 ULRE.ShaderRuntimeContract。

set(SHADERGEN_COMMON_FILES
    ${ROOT_INCLUDE_PATH}/hgl/graph/ShaderBufferSource.h
    ${ROOT_INCLUDE_PATH}/hgl/graph/ShaderBufferSources.h
    ${ROOT_INCLUDE_PATH}/hgl/shader/RootIncludePath.h
    ${SHADERGEN_INCLUDE_PATH}/ShaderCodeModule.h
    ${SHADERGEN_INCLUDE_PATH}/ShaderCodeResourceManifest.h
    ${ROOT_INCLUDE_PATH}/hgl/graph/ubo/ViewportInfo.h
    ${ROOT_INCLUDE_PATH}/hgl/graph/ubo/ColorPalette.h)
SOURCE_GROUP("Common" FILES ${SHADERGEN_COMMON_FILES})

set(DOCUMENT_FILES
    ${SHADERGEN_INCLUDE_PATH}/ShaderDocument.h
    document/ShaderDocument.cpp
    document/DocumentFragmentBuilder.h
    document/DocumentFragmentBuilder.cpp)
SOURCE_GROUP("Document" FILES ${DOCUMENT_FILES})

set(TEMPLATE_FILES
    ${SHADERGEN_INCLUDE_PATH}/ResolvedRenderTemplate.h
    template/RenderTemplateResolve.cpp
    ${SHADERGEN_INCLUDE_PATH}/PipelineConfigEmitter.h
    template/PipelineConfigEmitter.cpp
    ${SHADERGEN_INCLUDE_PATH}/FragmentTemplateComposer.h
    template/FragmentTemplateComposer.cpp)
SOURCE_GROUP("Template" FILES ${TEMPLATE_FILES})

set(SSBO_STRUCT_FILES
    ${ROOT_INCLUDE_PATH}/hgl/graph/ssbo/LitMaterialData.h
    ${ROOT_INCLUDE_PATH}/hgl/graph/ssbo/SSBOTypes.h)
SOURCE_GROUP("Standard Material\\SSBO" FILES ${SSBO_STRUCT_FILES})

# ── compile/ ─────────────────────────────────────────────────────────────────
# ShaderCreateInfo.cpp / ShaderArtifactStore.cpp / SamplerPreset.cpp /
# MaterialDefinitionRegistry.cpp / SceneRenderTemplateResolver.cpp / RenderTemplate.cpp
# 已迁入 ULRE.ShaderMaterialData（数据面，运行时也要）。
#
# GLSLCompiler / ShaderReflectionData / TBuiltInResourceCompat 已迁出到
# ULRE.ShaderCompiler（src/ShaderCompiler）：编译桥运行时也要，不能跟着生成器。
set(COMPILE_FILES
    compile/ShaderGenBackendInstall.h
    compile/ShaderCreateInfoCompile.cpp
    compile/ShaderGenProgramBuildBackend.cpp
    # 编译器插件的生命周期接线（选择器 + 注册对象）。独立成 TU 是因为它是
    # **存活**代码：生成器 TU 删除后接线必须仍然在，否则发行形态没有后端。
    compile/ShaderBuildBackendLifecycle.cpp
    # 「不生成着色器」的后端：片段阶段换成领域固定主着色器 + options。
    # 临时留在本模块是因为它复用 BuildGenericMaterial 的网格阶段（见阶段五搬走）。
    compile/DomainShaderBuildBackend.cpp
    ${SHADERGEN_INCLUDE_PATH}/ShaderProgramArtifactBuilder.h
    compile/ShaderProgramArtifactBuilder.cpp
    ${SHADERGEN_INCLUDE_PATH}/MaterialShaderCompiler.h
    compile/MaterialShaderCompiler.cpp
    compile/MaterialShaderEmitter.h
    compile/MaterialShaderEmitter.cpp)
SOURCE_GROUP("Compile" FILES ${COMPILE_FILES})

# ── glsl_module/ ─────────────────────────────────────────────────────────────
set(GLSL_MODULE_FILES
    ${SHADERGEN_INCLUDE_PATH}/ShaderCodeModuleRegistry.h
    glsl_module/ShaderCodeModuleRegistry.cpp
    ${SHADERGEN_INCLUDE_PATH}/ShaderCodeModuleMetadata.h
    glsl_module/ShaderCodeModuleMetadata.cpp
    ${SHADERGEN_INCLUDE_PATH}/ShaderCodeModuleFile.h
    glsl_module/ShaderCodeModuleFile.cpp
    glsl_module/ShaderCodeModule.cpp
    ${SHADERGEN_INCLUDE_PATH}/ShaderCodeResourceManifest.h
    glsl_module/ShaderCodeResourceManifest.cpp)
SOURCE_GROUP("GLSL Module" FILES ${GLSL_MODULE_FILES})

# ── contract/ ────────────────────────────────────────────────────────────────
set(CONTRACT_FILES
    ${SHADERGEN_INCLUDE_PATH}/CanonicalShaderContract.h
    contract/CanonicalShaderContract.cpp
    ${SHADERGEN_INCLUDE_PATH}/MaterialOutputContract.h
    contract/MaterialOutputContract.cpp
    ${SHADERGEN_INCLUDE_PATH}/MaterialCoverageContract.h
    contract/MaterialCoverageContract.cpp
    ${SHADERGEN_INCLUDE_PATH}/MaterialStageInterface.h
    contract/MaterialStageInterface.cpp
    ${SHADERGEN_INCLUDE_PATH}/ShaderSemanticRegistry.h
    contract/ShaderSemanticRegistry.cpp
    ${SHADERGEN_INCLUDE_PATH}/ShaderConfig.h
    contract/ShaderConfig.cpp
    contract/CanonicalContractWriter.h
    ${SHADERGEN_INCLUDE_PATH}/ShaderStageValueType.h
    ${SHADERGEN_INCLUDE_PATH}/SerializedVertexEntry.h)
SOURCE_GROUP("Contract" FILES ${CONTRACT_FILES})

# ── meshgen/ ─────────────────────────────────────────────────────────────────
# MeshShaderLimits.h 归 ULRE.ShaderRuntimeContract（VKPhysicalDevice 侧也要用），
# MeshShaderMode.h 归 ULRE.ShaderMaterialData（MaterialRecipe.h / 材质定义文件要用），
# 本清单都不再重复列出。
set(MESHGEN_FILES
    meshgen/MeshTemplateEmitter.h
    meshgen/MeshModeDescriptor.h
    meshgen/MeshShaderHeaderGen.h
    meshgen/MeshShaderVaryingGen.h
    meshgen/MeshShaderVertexAdapter.h
    meshgen/MeshShaderModeVertexPassthrough.h
    meshgen/MeshShaderModeLineQuad.h
    meshgen/MeshShaderModeCharQuad.h
    meshgen/MeshShaderTemplate.h
    meshgen/MeshShaderTemplate.cpp)
SOURCE_GROUP("MeshGen" FILES ${MESHGEN_FILES})

# ── builder/ ─────────────────────────────────────────────────────────────────
set(BUILDER_FILES
    builder/GenericMaterialBuilder.h
    builder/GenericMaterialBuilder.cpp
    builder/VertexABIBuilder.cpp
    builder/MaterialResourceManifestBuilder.h
    ${SHADERGEN_INCLUDE_PATH}/VertexShaderNodeConfig.h
    ${SHADERGEN_INCLUDE_PATH}/VertexNodeConfigResolver.h)
SOURCE_GROUP("Builder" FILES ${BUILDER_FILES})

# ── 生成期枚举（头-only）─────────────────────────────────────────────────────
# 只剩 PassType：它唯一的消费者是 contract/MaterialOutputContract.h（生成期输出契约），
# 运行时链路不引用。PipelineConfig.h（Vulkan PSO 数据）与 MeshShaderMode.h、
# RenderTargetOutputConfig.h 的消费者在数据面/下层，已下移到 ULRE.ShaderMaterialData。
set(MATERIAL_TYPE_FILES
    ${SHADERGEN_INCLUDE_PATH}/PassType.h)
SOURCE_GROUP("Material Types" FILES ${MATERIAL_TYPE_FILES})

# ── 设备 profile 契约（头-only）──────────────────────────────────────────────
# 已整体迁入 ULRE.ShaderRuntimeContract：设备能力契约运行时也要用
# （VKDeviceCreater / VKPhysicalDevice），不能跟着 ShaderGen 走。
# 本清单不再重复列出 ShaderGenContract / PhysicalDeviceProfile{Adapter,Json} /
# ProfileTargetVersion 四头，避免同一头被两个目标重复登记。

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_cm_library(ULRE.ShaderGen "ULRE"
    ${SHADERGEN_INCLUDE_PATH}

    ${SHADERGEN_COMMON_FILES}
    ${DOCUMENT_FILES}
    ${TEMPLATE_FILES}
    ${SSBO_STRUCT_FILES}
    ${COMPILE_FILES}
    ${GLSL_MODULE_FILES}
    ${CONTRACT_FILES}
    ${MESHGEN_FILES}
    ${BUILDER_FILES}
    ${MATERIAL_TYPE_FILES}
)

# 契约层独立成目标，ShaderGen 只是它的消费者之一。PUBLIC：吃掉 ShaderGen 的
# 目标必须同时拿到契约类型，否则发行版无法只保留契约。
# 数据面（材质定义/模板解析/SPV 产物库/ShaderCreateInfo）同理：ShaderGen 现在是它
# 的上层，生成期所有「读写定义与产物」的能力都由它提供。
target_link_libraries(ULRE.ShaderGen PUBLIC ULRE.ShaderRuntimeContract
                                            ULRE.ShaderMaterialData
                                            ULRE.ShaderCompiler
                                            ULRE.Debug)
