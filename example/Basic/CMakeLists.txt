macro(CreateProject name)

    add_executable(${name} ${ARGN} ${VULKAN_APP_FRAMEWORK})
    target_link_libraries(${name} ${ULRE})

    IF(MSVC)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
        set_property(TARGET ${name} PROPERTY VS_DPI_AWARE "PerMonitor")
    ENDIF()

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Example/Basic")
endmacro()


CreateProject(01_draw_triangle                  draw_triangle_use_ECS.cpp)
CreateProject(02_auto_instance                  auto_instance_use_ECS.cpp)
CreateProject(03_auto_merge_material_instance   auto_merge_material_instance_use_ECS.cpp)
CreateProject(04_clock_use                      clock_use_ECS.cpp)
CreateProject(05_Billboard                      BillboardTest_use_ECS.cpp)
CreateProject(06_SimpleCube                     SimpleCube_use_ECS.cpp)
CreateProject(07_RenderBoundBox                 RenderBoundBox_use_ECS.cpp)
CreateProject(08_RenderToTexture                RenderToTexture_use_ECS.cpp)

