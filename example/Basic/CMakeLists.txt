macro(CreateProject name)

    add_executable(${name} ${ARGN} ${VULKAN_APP_FRAMEWORK} MaterialLoader/MaterialLoader.cpp)
    target_link_libraries(${name} ${ULRE})

    IF(MSVC)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
        set_property(TARGET ${name} PROPERTY VS_DPI_AWARE "PerMonitor")
    ENDIF()

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Example/Basic")
endmacro()

CreateProject(00_RenderFrameworkTest            rf_test.cpp)
CreateProject(01_draw_triangle_in_NDC           draw_triangle_in_NDC.cpp)
CreateProject(02_draw_triangle_use_UBO          draw_triangle_use_UBO.cpp)
CreateProject(03_auto_instance                  auto_instance.cpp)
CreateProject(04_auto_merge_material_instance   auto_merge_material_instance.cpp)

CreateProject(05_Billboard                      BillboardTest.cpp)
