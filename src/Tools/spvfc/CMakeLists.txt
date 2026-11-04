cmake_minimum_required(VERSION 3.28)

# spvfc — 命令行着色器编译器（GLSL/HLSL → .spvf flat 文件）
#
# 历史：原住在 GLSLCompiler 插件 submodule 工程里，经 GLSLCompilerStatic 静态库
# 使用插件 C API。插件取消后（整合 1-4），本工具直接链接 ULRE.ShaderCompiler
# 并调用 hgl::graph::core 命名空间 API——flat 格式头 SPVParseFlat.h 与内部头
# ShaderCompilerCore.h 均由该目标的 PUBLIC include 目录传播。
#
# glslang / spirv-cross / SPIRV-Tools 经 ULRE.ShaderCompiler 的 PRIVATE 依赖
# 在链接期传播（$<LINK_ONLY>），SDK include 不外泄。
add_executable(spvfc spvfc.cpp)

target_compile_features(spvfc PRIVATE cxx_std_17)

target_link_libraries(spvfc PRIVATE ULRE.ShaderCompiler)

set_property(TARGET spvfc PROPERTY FOLDER "ULRE/Tools")

if(MSVC)
    set_target_properties(spvfc PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()