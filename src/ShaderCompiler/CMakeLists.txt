# ── ULRE.ShaderCompiler ───────────────────────────────────────────────────────
#
# glslang 插件桥：GLSL → SPIR-V 编译 + SPIR-V 反射。**不认识 ShaderGen**。
#
# 存在理由：这些 TU 原先住在 src/ShaderProgramBuild/compile，而运行时（ULRE.SceneGraph）
# 要调 GetSPVPushConstantEnd，等于运行时反向依赖生成器；发行形态
# （ULRE_ENABLE_SHADER_PROGRAM_BUILD=OFF）因此断链。本目标把桥提取成独立模块：
#
#   SceneGraph / Work / 工具  →  ULRE.ShaderCompiler  →  GLSLCompiler 插件(.dll)
#                                        ↑
#                          ULRE.ShaderProgramBuild / 直通构建后端（只注册生命周期钩子）
#
# 约束：
#   1. 只收「编译/反射」本身，不收任何构建后端实现（后端通过钩子安装）
#   2. 公共 API 只在 inc/hgl/ShaderCompilerAPI.h，本目录其余头是模块私有
#   3. SPVParseFlat.h 属于插件源码（src/Tools/GLSLCompiler），不复制——flat 格式的
#      ABI 单一真源在插件侧；本模块经 PUBLIC 接口把该路径传播给引用
#      ShaderReflectionData.h 的消费者（ShaderProgramBuild / ShaderTooling 门禁）

set(SHADERCOMPILER_FILES
    GLSLCompiler.cpp
    ShaderReflectionData.h
    ShaderReflectionData.cpp
    TBuiltInResourceCompat.h)

add_cm_library(ULRE.ShaderCompiler "ULRE" ""
    ${ROOT_INCLUDE_PATH}/hgl/ShaderCompilerAPI.h
    ${SHADERCOMPILER_FILES})

# 桥的插件侧契约：SPVData / 设备 profile / 插件接口 ABI 全在契约层与数据面。
target_link_libraries(ULRE.ShaderCompiler PUBLIC ULRE.ShaderRuntimeContract
                                                 ULRE.ShaderMaterialData
                                                 ULRE.Debug)

# SPVParseFlat.h 只在插件源码目录里有一份（flat 文件格式的单一真源）。
# 模块私有头（ShaderReflectionData.h 等）与插件契约头经 PUBLIC 接口传播，
# 消费者（ULRE.ShaderProgramBuild / 门禁工具）链接后即获得，无需硬编码路径。
target_include_directories(ULRE.ShaderCompiler PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/Tools/GLSLCompiler)
