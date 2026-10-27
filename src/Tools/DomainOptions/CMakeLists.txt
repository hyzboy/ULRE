cmake_minimum_required(VERSION 3.28)

# DomainOptions 回归门
#
# **只链接 ULRE.ShaderMaterialData**（+ 基础库）：不含 ULRE.ShaderProgramBuild / glslang / Vulkan。
# 因此本 target 在发行形态（ULRE_ENABLE_SHADERGEN=OFF）下照常构建，这本身就是
# 「frag_options.glsl 的发射不依赖生成器」的结构性断言。见 src/Tools/CMakeLists.txt。
add_executable(DomainOptionsRegression
    DomainOptionsRegression.cpp)

target_link_libraries(DomainOptionsRegression
    PRIVATE ULRE.ShaderMaterialData
            ULRE.Debug
            CMCore
            CMCoreType)

set_property(TARGET DomainOptionsRegression PROPERTY FOLDER "ULRE/Tools/ShaderMaterialData")

if(MSVC)
    set_target_properties(DomainOptionsRegression PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endif()

if(BUILD_TESTING)
    add_test(
        NAME domain_options_regression
        COMMAND DomainOptionsRegression)
    set_tests_properties(
        domain_options_regression
        PROPERTIES LABELS "shadermaterial;baseline")
endif()
