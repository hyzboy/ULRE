set(SHADERGEN_INCLUDE_PATH  ${ROOT_INCLUDE_PATH}/hgl/mtl)

option(ULRE_ENABLE_RELEASE_SHELL
    "Prepare release-shell interfaces without forcing a final SPV-only runtime split during active shader development"
    OFF)

# ── 物理目录 = SOURCE_GROUP = 管线角色（2026-09 目录整理，对齐模块隔离设计 §四）──
#   compile/             编译执行：glslang 封装 + 构建上下文 + SPV 产物 + 求解/发射
#   glsl_module/         GLSL 代码模块体系（registry/metadata/file/manifest/preset）
#   material_definition/ 材质定义（TOML 加载 + 注册表）
#   contract/            契约层（结构校验/哈希/绑定解析/输出/覆盖率/接口）
#   meshgen/             mesh shader 生成（装配器 + 模式子模块 + 模板）
#   compositor/          fragment 单趟发射器
#   builder/             定义 → 构建计划编排（generic builder/顶点 ABI/描述符推导）
# 公共头（inc/hgl/mtl 与 inc/hgl/graph）不参与物理搬移，路径保持稳定。

set(SHADERGEN_COMMON_FILES
    ${ROOT_INCLUDE_PATH}/hgl/graph/ShaderBufferSource.h
    ${ROOT_INCLUDE_PATH}/hgl/graph/ShaderBufferSources.h
    ${SHADERGEN_INCLUDE_PATH}/ShaderLibraryPath.h
    ${SHADERGEN_INCLUDE_PATH}/ShaderCacheRoot.h
    ${SHADERGEN_INCLUDE_PATH}/ShaderCodeModule.h
    ${SHADERGEN_INCLUDE_PATH}/ShaderCodeResourceManifest.h
    ${ROOT_INCLUDE_PATH}/hgl/graph/ubo/SkyInfo.h
    ${ROOT_INCLUDE_PATH}/hgl/graph/ubo/ViewportInfo.h
    ${ROOT_INCLUDE_PATH}/hgl/graph/ubo/UBOShaderSources.h
    ${ROOT_INCLUDE_PATH}/hgl/graph/ubo/ColorPalette.h)
SOURCE_GROUP("Common" FILES ${SHADERGEN_COMMON_FILES})

set(DOCUMENT_FILES
    ${SHADERGEN_INCLUDE_PATH}/ShaderDocument.h
    ${SHADERGEN_INCLUDE_PATH}/ShaderDocumentLegacyAdapter.h
    document/ShaderDocument.cpp
    document/DocumentFragmentBuilder.cpp
    document/ShaderDocumentLegacyAdapter.cpp)
SOURCE_GROUP("Document" FILES ${DOCUMENT_FILES})

set(SSBO_STRUCT_FILES
    ${ROOT_INCLUDE_PATH}/hgl/graph/ssbo/LitMaterialData.h
    ${ROOT_INCLUDE_PATH}/hgl/graph/ssbo/TextureSlot.h
    ${ROOT_INCLUDE_PATH}/hgl/graph/ssbo/SSBOTypes.h)
SOURCE_GROUP("Standard Material\\SSBO" FILES ${SSBO_STRUCT_FILES})

# ── compile/ ─────────────────────────────────────────────────────────────────
set(COMPILE_FILES
    compile/GLSLCompiler.h
    compile/GLSLCompiler.cpp
    compile/TBuiltInResourceCompat.h
    ${SHADERGEN_INCLUDE_PATH}/ShaderBuildContext.h
    compile/ShaderBuildContext.cpp
    ${SHADERGEN_INCLUDE_PATH}/ShaderCreateInfo.h
    compile/ShaderCreateInfo.cpp
    ${SHADERGEN_INCLUDE_PATH}/ShaderCreateInfoMap.h
    compile/ShaderCreateInfoMap.cpp
    ${SHADERGEN_INCLUDE_PATH}/ShaderArtifactContract.h
    ${SHADERGEN_INCLUDE_PATH}/ShaderStageKey.h
    ${SHADERGEN_INCLUDE_PATH}/ShaderKeyUtility.h
    ${SHADERGEN_INCLUDE_PATH}/ShaderProgramKey.h
    ${SHADERGEN_INCLUDE_PATH}/ShaderLinkSpec.h
    ${SHADERGEN_INCLUDE_PATH}/ShaderArtifactStore.h
    compile/ShaderArtifactStore.cpp
    ${SHADERGEN_INCLUDE_PATH}/ShaderProgramArtifactBuilder.h
    compile/ShaderProgramArtifactBuilder.cpp
    ${SHADERGEN_INCLUDE_PATH}/ShaderStructureDump.h
    compile/ShaderStructureDump.cpp
    ${SHADERGEN_INCLUDE_PATH}/DescriptorSetLayoutAllocator.h
    compile/DescriptorSetLayoutAllocator.cpp
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
    ${SHADERGEN_INCLUDE_PATH}/ShaderCodeModuleCapabilityResolver.h
    glsl_module/ShaderCodeModuleCapabilityResolver.cpp
    glsl_module/ShaderCodeModule.cpp
    ${SHADERGEN_INCLUDE_PATH}/ShaderCodeResourceManifest.h
    glsl_module/ShaderCodeResourceManifest.cpp
    ${SHADERGEN_INCLUDE_PATH}/SamplerPreset.h
    glsl_module/SamplerPreset.cpp)
SOURCE_GROUP("GLSL Module" FILES ${GLSL_MODULE_FILES})

# ── material_definition/ ─────────────────────────────────────────────────────
set(MATERIAL_DEFINITION_FILES
    ${SHADERGEN_INCLUDE_PATH}/MaterialDefinitionRegistry.h
    material_definition/MaterialDefinitionRegistry.cpp
    ${SHADERGEN_INCLUDE_PATH}/MaterialDefinitionFile.h
    material_definition/MaterialDefinitionFile.cpp)
SOURCE_GROUP("Material Definition" FILES ${MATERIAL_DEFINITION_FILES})

# ── contract/ ────────────────────────────────────────────────────────────────
set(CONTRACT_FILES
    ${SHADERGEN_INCLUDE_PATH}/CanonicalShaderContract.h
    contract/CanonicalShaderContract.cpp
    ${SHADERGEN_INCLUDE_PATH}/DescriptorContract.h
    contract/DescriptorContract.cpp
    ${SHADERGEN_INCLUDE_PATH}/MaterialOutputContract.h
    contract/MaterialOutputContract.cpp
    ${SHADERGEN_INCLUDE_PATH}/MaterialCoverageContract.h
    contract/MaterialCoverageContract.cpp
    ${SHADERGEN_INCLUDE_PATH}/MaterialBindingContract.h
    ${SHADERGEN_INCLUDE_PATH}/BindingTableBuilder.h
    contract/MaterialBindingContract.cpp
    contract/BindingTableBuilder.cpp
    ${SHADERGEN_INCLUDE_PATH}/MaterialStageInterface.h
    contract/MaterialStageInterface.cpp
    ${SHADERGEN_INCLUDE_PATH}/ShaderSemanticRegistry.h
    contract/ShaderSemanticRegistry.cpp
    contract/CanonicalContractWriter.h
    ${SHADERGEN_INCLUDE_PATH}/ShaderStageValueType.h
    ${SHADERGEN_INCLUDE_PATH}/ShaderResourceSchema.h
    ${SHADERGEN_INCLUDE_PATH}/DescriptorSemantic.h
    ${SHADERGEN_INCLUDE_PATH}/SerializedDescriptorEntry.h
    ${SHADERGEN_INCLUDE_PATH}/SerializedVertexEntry.h)
SOURCE_GROUP("Contract" FILES ${CONTRACT_FILES})

# ── meshgen/ ─────────────────────────────────────────────────────────────────
set(MESHGEN_FILES
    ${SHADERGEN_INCLUDE_PATH}/MeshShaderMode.h
    ${SHADERGEN_INCLUDE_PATH}/MeshShaderLimits.h
    meshgen/MeshShaderAssembler.h
    meshgen/MeshShaderHeaderGen.h
    meshgen/MeshShaderVaryingGen.h
    meshgen/MeshShaderVertexAdapter.h
    meshgen/MeshShaderModeVertexPassthrough.h
    meshgen/MeshShaderModeLineQuad.h
    meshgen/MeshShaderModeCharQuad.h
    meshgen/MeshShaderTemplate.h
    meshgen/MeshShaderTemplate.cpp)
SOURCE_GROUP("MeshGen" FILES ${MESHGEN_FILES})

# ── compositor/ ──────────────────────────────────────────────────────────────
set(COMPOSITOR_FILES
    ${SHADERGEN_INCLUDE_PATH}/CompositorAssembler.h
    compositor/CompositorAssembler.cpp)
SOURCE_GROUP("Compositor" FILES ${COMPOSITOR_FILES})

# ── builder/ ─────────────────────────────────────────────────────────────────
set(BUILDER_FILES
    builder/GenericMaterialBuilder.h
    builder/GenericMaterialBuilder.cpp
    builder/VertexABIBuilder.cpp
    builder/DefinitionDescriptorBuilder.h
    builder/DescriptorBuilderCommon.h
    ${SHADERGEN_INCLUDE_PATH}/VertexShaderNodeConfig.h
    ${SHADERGEN_INCLUDE_PATH}/VertexNodeConfigResolver.h)
SOURCE_GROUP("Builder" FILES ${BUILDER_FILES})

# ── 公共类型（Surface/Blend/Pass/Pipeline 等，头-only）────────────────────────
set(MATERIAL_TYPE_FILES
    ${SHADERGEN_INCLUDE_PATH}/SurfaceType.h
    ${SHADERGEN_INCLUDE_PATH}/BlendMode.h
    ${SHADERGEN_INCLUDE_PATH}/PassType.h
    ${SHADERGEN_INCLUDE_PATH}/PipelineConfig.h
    ${ROOT_INCLUDE_PATH}/hgl/common/RenderTargetOutputConfig.h)
SOURCE_GROUP("Material Types" FILES ${MATERIAL_TYPE_FILES})

# ── 设备 profile 契约（头-only）──────────────────────────────────────────────
set(PROFILE_FILES
    ${SHADERGEN_INCLUDE_PATH}/ShaderCompilerProfileAPI.h
    ${ROOT_INCLUDE_PATH}/hgl/mtl/contract/ShaderGenContract.h
    ${ROOT_INCLUDE_PATH}/hgl/mtl/contract/ShaderGenPhysicalDeviceProfileAdapter.h
    ${ROOT_INCLUDE_PATH}/hgl/mtl/contract/ShaderGenPhysicalDeviceProfileJson.h
    ${ROOT_INCLUDE_PATH}/hgl/mtl/contract/ShaderGenProfileTargetVersion.h)
SOURCE_GROUP("Device Profile" FILES ${PROFILE_FILES})

set(RELEASE_SHELL_FILES
    ${SHADERGEN_INCLUDE_PATH}/ShaderRuntimeReleaseShell.h
    release/ShaderRuntimeReleaseShell.cpp)
SOURCE_GROUP("Release Shell" FILES ${RELEASE_SHELL_FILES})

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_cm_library(ULRE.ShaderGen "ULRE"
    ${SHADERGEN_INCLUDE_PATH}

    ${SHADERGEN_COMMON_FILES}
    ${DOCUMENT_FILES}
    ${SSBO_STRUCT_FILES}
    ${COMPILE_FILES}
    ${GLSL_MODULE_FILES}
    ${MATERIAL_DEFINITION_FILES}
    ${CONTRACT_FILES}
    ${MESHGEN_FILES}
    ${COMPOSITOR_FILES}
    ${BUILDER_FILES}
    ${MATERIAL_TYPE_FILES}
    ${PROFILE_FILES}
    ${RELEASE_SHELL_FILES}
)

if(ULRE_ENABLE_RELEASE_SHELL)
    target_compile_definitions(ULRE.ShaderGen PRIVATE ULRE_ENABLE_RELEASE_SHELL=1)
    message(STATUS "ULRE_ENABLE_RELEASE_SHELL is ON: development-first runtime packaging guard is active.")
endif()
