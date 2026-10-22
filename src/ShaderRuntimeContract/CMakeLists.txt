# ── ULRE.ShaderRuntimeContract ────────────────────────────────────────────────
#
# 纯数据契约层：只收「运行时也需要」的身份 / 产物 / 设备能力类型。
#
# 这个目标存在的唯一理由是**让运行时不必链接 ULRE.ShaderGen**。所以它有硬约束，
# 往这里加文件前先逐条确认：
#
#   1. 零 GLSL 源码处理（不 include 任何 glsl_module / template / builder 头）
#   2. 零文件 IO（不读 TOML、不扫目录、不拼缓存路径）
#   3. 零外部编译器插件（不碰 GLSLCompiler / glslang）
#   4. 头必须自包含，可以被发行版的任何子集单独编入
#
# 判据：本目标产出的 .lib 里不允许出现 GLSLCompiler / glslang / TOML 解析 /
# 目录扫描相关的任何符号。ShaderRuntimeContract.cpp 是这条约束的编译期哨兵。

set(SHADERRUNTIMECONTRACT_FILES
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderProgramKey.h
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderStageKey.h
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderKeyUtility.h
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderLinkSpec.h
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderArtifactContract.h
    ${ROOT_INCLUDE_PATH}/hgl/mtl/MeshShaderLimits.h
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderCompilerProfileAPI.h
    ${ROOT_INCLUDE_PATH}/hgl/mtl/contract/ShaderGenContract.h
    ${ROOT_INCLUDE_PATH}/hgl/mtl/contract/ShaderGenProfileTargetVersion.h
    ${ROOT_INCLUDE_PATH}/hgl/mtl/contract/ShaderGenPhysicalDeviceProfileAdapter.h
    ${ROOT_INCLUDE_PATH}/hgl/mtl/contract/ShaderGenPhysicalDeviceProfileJson.h
    ${ROOT_INCLUDE_PATH}/hgl/mtl/ShaderProgramBuildBackend.h
    ShaderProgramBuildBackend.cpp
    ShaderRuntimeContract.cpp)
SOURCE_GROUP("Contract Types" FILES ${SHADERRUNTIMECONTRACT_FILES})

# include_path 传空：这些头都在全局 include 根 inc/ 下，无需额外导出目录。
add_cm_library(ULRE.ShaderRuntimeContract "ULRE" ""
    ${SHADERRUNTIMECONTRACT_FILES})
