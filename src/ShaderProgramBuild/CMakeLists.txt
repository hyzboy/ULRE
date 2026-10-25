set(SHADERPROGRAMBUILD_INCLUDE_PATH  ${ROOT_INCLUDE_PATH}/hgl/mtl)

# ── 物理目录 = SOURCE_GROUP = 管线角色 ──
#   compile/             阶段编译 + 构建上下文 + SPV 产物
#   contract/            契约层（结构校验/哈希/绑定解析/输出/覆盖率/接口）
#   template/            fragment/mesh 模板发射器
#   builder/             材质定义 → 程序构建计划（顶点 ABI / 链接规格）
# 公共头（inc/hgl/mtl 与 inc/hgl/graph）不参与物理搬移，路径保持稳定。
#
# 本清单只收程序构建/阶段编译所需的头与源。材质与代码模块数据
# （ULRE.ShaderMaterialData）由下层目标持有：
# 以下数据面源与头的 CMake 归属已下移到那一层，本清单不再重复列出：
#   ShaderLibraryPath / VkEnumText / ShaderCacheRoot / ShaderBuildContext /
#   ShaderCreateInfo(+Map) / ShaderArtifactStore / MaterialDefinitionFile /
#   MaterialDefinitionRegistry / SamplerPreset / RenderTemplate /
#   SceneRenderTemplateResolver
# 设备 profile 契约头与 MeshShaderLimits 归 ULRE.ShaderRuntimeContract。

set(SHADERPROGRAMBUILD_COMMON_FILES
    ${ROOT_INCLUDE_PATH}/hgl/graph/ShaderBufferSource.h
    ${ROOT_INCLUDE_PATH}/hgl/graph/ShaderBufferSources.h
    ${ROOT_INCLUDE_PATH}/hgl/shader/RootIncludePath.h
    ${ROOT_INCLUDE_PATH}/hgl/graph/ubo/ViewportInfo.h
    ${ROOT_INCLUDE_PATH}/hgl/graph/ubo/ColorPalette.h)
SOURCE_GROUP("Common" FILES ${SHADERPROGRAMBUILD_COMMON_FILES})

set(TEMPLATE_FILES
    ${SHADERPROGRAMBUILD_INCLUDE_PATH}/ResolvedRenderTemplate.h
    template/RenderTemplateResolve.cpp
    ${SHADERPROGRAMBUILD_INCLUDE_PATH}/PipelineConfigEmitter.h
    template/PipelineConfigEmitter.cpp)
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
    compile/ShaderProgramBuildBackendInstall.h
    compile/ShaderCreateInfoCompile.cpp
    # 编译器插件的生命周期接线（注册对象）。独立成 TU 是因为它是**存活**代码：
    # 无对外符号，靠 WHOLE_ARCHIVE 才不被链接器丢掉。
    compile/ShaderBuildBackendLifecycle.cpp
    # 「不生成着色器」的后端：片段阶段换成领域固定主着色器 + options。
    # 留在本模块，因为它复用 AssembleMaterialProgram 的网格阶段。
    compile/DomainShaderBuildBackend.cpp
    ${SHADERPROGRAMBUILD_INCLUDE_PATH}/ShaderProgramArtifactBuilder.h
    compile/ShaderProgramArtifactBuilder.cpp
    ${SHADERPROGRAMBUILD_INCLUDE_PATH}/StageSPVCompiler.h
    compile/StageSPVCompiler.cpp)
SOURCE_GROUP("Compile" FILES ${COMPILE_FILES})

# ── contract/ ────────────────────────────────────────────────────────────────
set(CONTRACT_FILES
    ${SHADERPROGRAMBUILD_INCLUDE_PATH}/CanonicalShaderContract.h
    contract/CanonicalShaderContract.cpp
    ${SHADERPROGRAMBUILD_INCLUDE_PATH}/MaterialOutputContract.h
    contract/MaterialOutputContract.cpp
    ${SHADERPROGRAMBUILD_INCLUDE_PATH}/MaterialCoverageContract.h
    contract/MaterialCoverageContract.cpp
    ${SHADERPROGRAMBUILD_INCLUDE_PATH}/MaterialStageInterface.h
    contract/MaterialStageInterface.cpp
    ${SHADERPROGRAMBUILD_INCLUDE_PATH}/ShaderSemanticRegistry.h
    contract/ShaderSemanticRegistry.cpp
    ${SHADERPROGRAMBUILD_INCLUDE_PATH}/ShaderConfig.h
    contract/ShaderConfig.cpp
    contract/CanonicalContractWriter.h
    ${SHADERPROGRAMBUILD_INCLUDE_PATH}/ShaderStageValueType.h
    ${SHADERPROGRAMBUILD_INCLUDE_PATH}/SerializedVertexEntry.h)
SOURCE_GROUP("Contract" FILES ${CONTRACT_FILES})

# ── builder/ ─────────────────────────────────────────────────────────────────
set(BUILDER_FILES
    builder/MaterialProgramAssembler.h
    builder/MaterialProgramAssembler.cpp
    builder/VertexABIBuilder.cpp
    builder/MaterialResourceManifestBuilder.h
    ${SHADERPROGRAMBUILD_INCLUDE_PATH}/VertexShaderNodeConfig.h
    ${SHADERPROGRAMBUILD_INCLUDE_PATH}/VertexNodeConfigResolver.h)
SOURCE_GROUP("Builder" FILES ${BUILDER_FILES})

# ── 构建期枚举（头-only）─────────────────────────────────────────────────────
# 只剩 PassType：它唯一的消费者是 contract/MaterialOutputContract.h（输出契约），
# 运行时链路不引用。PipelineConfig.h（Vulkan PSO 数据）与 MeshShaderMode.h、
# RenderTargetOutputConfig.h 的消费者在数据面/下层，已下移到 ULRE.ShaderMaterialData。
set(MATERIAL_TYPE_FILES
    ${SHADERPROGRAMBUILD_INCLUDE_PATH}/PassType.h)
SOURCE_GROUP("Material Types" FILES ${MATERIAL_TYPE_FILES})

# ── 设备 profile 契约（头-only）──────────────────────────────────────────────
# 已整体迁入 ULRE.ShaderRuntimeContract：设备能力契约运行时也要用
# （VKDeviceCreater / VKPhysicalDevice），不能跟着程序构建目标走。
# 本清单不再重复列出 ShaderGenContract / PhysicalDeviceProfile{Adapter,Json} /
# ProfileTargetVersion 四头，避免同一头被两个目标重复登记。

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_cm_library(ULRE.ShaderProgramBuild "ULRE"
    ${SHADERPROGRAMBUILD_INCLUDE_PATH}

    ${SHADERPROGRAMBUILD_COMMON_FILES}
    ${TEMPLATE_FILES}
    ${SSBO_STRUCT_FILES}
    ${COMPILE_FILES}
    ${CONTRACT_FILES}
    ${BUILDER_FILES}
    ${MATERIAL_TYPE_FILES}
)

# Program build consumes the runtime contracts, material data, and stage compiler.
target_link_libraries(ULRE.ShaderProgramBuild PUBLIC ULRE.ShaderRuntimeContract
                                                    ULRE.ShaderMaterialData
                                                    ULRE.ShaderCompiler
                                                    ULRE.Debug)
