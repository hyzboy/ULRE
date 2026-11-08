# EnvSunPicker —— .env 资产太阳方向拾取工具（引擎级工具，与示例同形态）
#
# 链 ULRE::Engine（Vulkan 渲染 + ECS + 环境装配），非发行内容；
# Windows 下额外链 comdlg32（GetOpenFileNameW 系统打开框）。

add_executable(EnvSunPicker EnvSunPicker.cpp)

target_link_libraries(EnvSunPicker PRIVATE ULRE::Engine)

if(WIN32)
    target_link_libraries(EnvSunPicker PRIVATE comdlg32)
endif()

set_target_properties(EnvSunPicker PROPERTIES
    FOLDER "ULRE/Tools/EnvSunPicker")

if(MSVC)
    # 与 example/CreateProject 一致：调试时工作目录 = 运行时目录，DPI PerMonitor
    set_target_properties(EnvSunPicker PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${ULRE_RUNTIME_PATH}"
        VS_DPI_AWARE "PerMonitor")
endif()
