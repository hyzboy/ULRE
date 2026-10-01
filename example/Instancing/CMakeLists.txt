# 实例化：RenderCollector 自动合并（同几何 / 同材质 / 同程序）批量绘制。

macro(CreateProject name)

    add_executable(${name} ${ARGN} ${VULKAN_APP_FRAMEWORK})
    target_link_libraries(${name} ${ULRE})

    IF(MSVC)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
        set_property(TARGET ${name} PROPERTY VS_DPI_AWARE "PerMonitor")
    ENDIF()

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Example/Instancing")
endmacro()

CreateProject(AutoInstance                   AutoInstance.cpp)
CreateProject(AutoMergeMaterialInstance      AutoMergeMaterialInstance.cpp)
