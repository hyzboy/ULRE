cmake_minimum_required(VERSION 3.28)

# 发行形态探针（RuntimeFormProbe）
#
# **只链接 ULRE::Runtime**，永不含 ULRE.ShaderGen / glslang。因此本 target 能否
# 链接通过，本身就是「运行时不再引用生成器符号」的可执行断言：哪天有运行时 .cpp
# 又反向依赖了生成器，这里会直接 LNK2019。
#
# 注意：不注册 ctest。它需要一份由开发形态烘焙出来的 shader-cache 夹具
# （`ShaderResourceSchemaRegressionGate emit-release-cache <dir>`），标准构建树里
# 没有这份夹具，注册成 ctest 只会变成一条恒红的用例。夹具链路由
# scripts/ci/verify-release-form.ps1 负责驱动。
add_executable(RuntimeFormProbe
    RuntimeFormProbe.cpp)

target_link_libraries(RuntimeFormProbe
    PRIVATE ULRE::Runtime)

target_include_directories(RuntimeFormProbe
    PRIVATE ${CMAKE_SOURCE_DIR}/src)

set_property(TARGET RuntimeFormProbe PROPERTY FOLDER "ULRE/Tools/RuntimeForm")

if(MSVC)
    set_target_properties(RuntimeFormProbe PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_property(TARGET RuntimeFormProbe PROPERTY VS_DPI_AWARE "PerMonitor")
endif()
