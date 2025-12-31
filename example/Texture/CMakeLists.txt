macro(CreateProject name)
    add_executable(${name} ${ARGN} ${VULKAN_APP_FRAMEWORK})
    target_link_libraries(${name} ${ULRE})

    IF(MSVC)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
        set_property(TARGET ${name} PROPERTY VS_DPI_AWARE "PerMonitor")
    ENDIF()

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Example/Texture")
endmacro()

CreateProject(05_texture_format     texture_format_list.cpp)
CreateProject(06_texture_quad       texture_quad.cpp)
CreateProject(07_texture_rect       texture_rect.cpp)
CreateProject(08_texture_rect_array texture_rect_array.cpp)
