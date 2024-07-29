macro(CreateProject name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} ${ULRE})

    IF(WIN32)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
    ENDIF()

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Example/2dVector")
endmacro()

CreateProject(00.line line.cpp)
CreateProject(01.LerpLine LerpLine.cpp)

CreateProject(02.roundbox roundbox.cpp)
