# ULRE.ShaderMaterialData —— 材质/着色器「数据面」。
#
# 这一层是运行时真正需要的：TOML 材质定义装载与注册表、渲染模板解析、SPV 产物库、
# ShaderCreateInfo 的纯数据成员、采样器预设表。它零 GLSL 文本发射、零 glslang，
# 因此发行形态（ULRE_ENABLE_SHADERGEN=0）也照常链接。
#
# 与 ULRE.ShaderGen 的分界线：任何需要「把定义拼成 GLSL 再编译」的东西都留在
# ShaderGen，跨界的唯一通道是 inc/hgl/mtl/ShaderProgramBuildBackend.h。

set(SHADERMATERIALDATA_FILES
    MaterialDefinitionFile.cpp
    MaterialDefinitionRegistry.cpp
    RenderTemplate.cpp
    ShaderArtifactStore.cpp
    ShaderCreateInfo.cpp
    SamplerPreset.cpp
    SceneRenderTemplateResolver.cpp
    ShaderProgramManifest.cpp
    ShaderProgramManifestKey.cpp
    ShaderProgramManifestBackend.cpp
    ShaderComputeProgramManifest.cpp)

add_cm_library(ULRE.ShaderMaterialData "ULRE"
    ${ROOT_INCLUDE_PATH}/hgl/mtl

    ${SHADERMATERIALDATA_FILES}
)

# 数据面吃契约类型（ShaderProgramKey / ShaderArtifactContract / 设备 profile）。PUBLIC：
# 吃掉本库的目标同样需要这些类型。
target_link_libraries(ULRE.ShaderMaterialData PUBLIC ULRE.ShaderRuntimeContract)
