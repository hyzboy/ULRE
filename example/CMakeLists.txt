# 全部 example 共用一个建目标宏。
#
# 此前 14 个示例目录各自拷贝了一份 `macro(CreateProject name)`，内容近乎相同但已出现漂移
# （`VS_DPI_AWARE` 只在部分副本里；`${VULKAN_APP_FRAMEWORK}` 全仓从未定义因而恒为空）。
# 收归到这里之后，各示例目录的 CMakeLists 只剩「本目录要建哪些 target」这一件事。
#
# IDE 分组由相对 example/ 的路径推出：目录结构即分组结构，无需逐目录手写 FOLDER。
macro(CreateProject name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} ULRE::Engine)

    if(MSVC)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
        set_property(TARGET ${name} PROPERTY VS_DPI_AWARE "PerMonitor")
    endif()

    file(RELATIVE_PATH _example_rel "${CMAKE_CURRENT_SOURCE_DIR}/.." "${CMAKE_CURRENT_SOURCE_DIR}")
    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Example/${_example_rel}")
endmacro()

add_subdirectory(GettingStarted)
add_subdirectory(Material)
add_subdirectory(Instancing)
add_subdirectory(Transform)
add_subdirectory(Geometry)
add_subdirectory(Texture)
add_subdirectory(Offscreen)
add_subdirectory(Shadow)
add_subdirectory(Environment)
add_subdirectory(Text)
add_subdirectory(Interaction)
add_subdirectory(Debug)
add_subdirectory(Cull)
add_subdirectory(ApiValidation)
