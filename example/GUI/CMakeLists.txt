macro(CreateProject name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} ${ULRE})

    IF(WIN32)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
    ENDIF()

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Example/GUI")
endmacro()

CreateProject(TextDrawTest           TextDrawTest.cpp)
CreateProject(DrawMultiLineText      DrawMultiLineText.cpp)
CreateProject(SDFTextEffects         SDFTextEffects.cpp)
CreateProject(MultiFontSource        MultiFontSource.cpp)
CreateProject(TextVertical           TextVertical.cpp)
