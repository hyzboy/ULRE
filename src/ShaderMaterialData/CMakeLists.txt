# ULRE.ShaderMaterialData —— 材质/着色器「数据面」。
#
# 这一层是运行时真正需要的：TOML 材质定义装载与注册表、渲染模板解析、SPV 产物库、
# ShaderCreateInfo 的纯数据成员、采样器预设表。它零 GLSL 文本发射、零 glslang，
# 因此发行形态（ULRE_ENABLE_SHADERGEN=0）也照常链接。
#
# 与 ULRE.ShaderGen 的分界线：任何需要「把定义拼成 GLSL 再编译」的东西都留在
# ShaderGen，跨界的唯一**行为**通道是 inc/hgl/mtl/ShaderProgramBuildBackend.h。
#
# 头清单归属判据：本层有对应 .cpp，或本层源文件直接包含该头。
#
# 已知的遗留反向包含（仅类型声明，不含 GLSL 发射行为，故本层仍能独立编译）：
#   RenderTemplate.h / MaterialRecipe.h → ShaderCodeModule.h
#   RenderTemplate.h                    → ShaderCodeResourceManifest.h
# 这两处是 glsl_module 代码模块描述符被材质定义/模板口径复用的结果；要彻底切断，
# 需把那组「模块描述符头」也下移到本层（属独立的重构任务，不在此处夹带）。

set(SHADERMATERIALDATA_FILES
    # ── 材质定义与注册表 ──
    ${ROOT_INCLUDE_PATH}/hgl/mtl/MaterialDefinitionFile.h
    MaterialDefinitionFile.cpp
    ${ROOT_INCLUDE_PATH}/hgl/mtl/MaterialDefinitionRegistry.h
    MaterialDefinitionRegistry.cpp
    ${ROOT_INCLUDE_PATH}/hgl/mtl/MaterialRecipe.h
    ${ROOT_INCLUDE_PATH}/hgl/mtl/MaterialVertexVaryingConfig.h
    # ── 渲染模板解析 ──
    ${ROOT_INCLUDE_PATH}/hgl/mtl/RenderTemplate.h
    RenderTemplate.cpp
    ${ROOT_INCLUDE_PATH}/hgl/mtl/SceneRenderTemplateResolver.h
    SceneRenderTemplateResolver.cpp
    # ── SPV 产物库 ──
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderArtifactStore.h
    ShaderArtifactStore.cpp
    # ── ShaderCreateInfo 纯数据成员 + 构建上下文 ──
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderCreateInfo.h
    ShaderCreateInfo.cpp
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderCreateInfoMap.h
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderBuildContext.h
    # ── 采样器预设 / 着色器库与缓存根路径 / 枚举文本 ──
    ${ROOT_INCLUDE_PATH}/hgl/mtl/SamplerPreset.h
    SamplerPreset.cpp
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderLibraryPath.h
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderCacheRoot.h
    ${ROOT_INCLUDE_PATH}/hgl/mtl/VkEnumText.h
    # ── 发行形态的两份查表清单 ──
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderProgramManifest.h
    ShaderProgramManifest.cpp
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderProgramManifestKey.h
    ShaderProgramManifestKey.cpp
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderProgramManifestBackend.h
    ShaderProgramManifestBackend.cpp
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderComputeProgramManifest.h
    ShaderComputeProgramManifest.cpp)

add_cm_library(ULRE.ShaderMaterialData "ULRE"
    ${ROOT_INCLUDE_PATH}/hgl/mtl

    ${SHADERMATERIALDATA_FILES}
)

# 数据面吃契约类型（ShaderProgramKey / ShaderArtifactContract / 设备 profile）。PUBLIC：
# 吃掉本库的目标同样需要这些类型。
target_link_libraries(ULRE.ShaderMaterialData PUBLIC ULRE.ShaderRuntimeContract)
