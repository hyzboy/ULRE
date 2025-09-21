add_subdirectory(Basic)
add_subdirectory(Texture)
add_subdirectory(Gizmo)
add_subdirectory(GUI)
add_subdirectory(Geometry)
add_subdirectory(Environment)

add_subdirectory(Landscape)

macro(CreateProject name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} ${ULRE})

    IF(WIN32)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
    ENDIF()

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Example")
endmacro()

