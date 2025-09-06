macro(CreateProject name)
    add_executable(${name} ${ARGN} GUIAppFramework.h)
    target_link_libraries(${name} ${ULRE})

    IF(WIN32)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
    ENDIF()

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Example/GUI")
endmacro()

#CreateProject(00.control_point_2d   control_point_2d.cpp)
#CreateProject(01.align_test         align_test.cpp)
CreateProject(02.TextDrawTest       DrawText.cpp)
CreateProject(03.DrawMultiLineText  DrawMultiLineText.cpp)
#CreateProject(04.DrawRoundrectangle DrawRoundrectangle.cpp)
