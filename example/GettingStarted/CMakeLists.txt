# 入门：最小可运行的单个图元（三角形 / 立方体 / 球体），ECS 骨架最短路径。

macro(CreateProject name)

    add_executable(${name} ${ARGN} ${VULKAN_APP_FRAMEWORK})
    target_link_libraries(${name} ${ULRE})

    IF(MSVC)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
        set_property(TARGET ${name} PROPERTY VS_DPI_AWARE "PerMonitor")
    ENDIF()

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Example/GettingStarted")
endmacro()

CreateProject(DrawTriangle                   DrawTriangle.cpp)
CreateProject(SimpleCube                     SimpleCube.cpp)
CreateProject(SimpleSphere                   SimpleSphere.cpp)
