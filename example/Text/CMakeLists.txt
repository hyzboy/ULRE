# 文本渲染：SDF 字形、多行 / 竖排排版、多字体源与描边效果。

macro(CreateProject name)

    add_executable(${name} ${ARGN} ${VULKAN_APP_FRAMEWORK})
    target_link_libraries(${name} ${ULRE})

    IF(MSVC)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
        set_property(TARGET ${name} PROPERTY VS_DPI_AWARE "PerMonitor")
    ENDIF()

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Example/Text")
endmacro()

CreateProject(TextDraw              TextDraw.cpp)
CreateProject(DrawMultiLineText      DrawMultiLineText.cpp)
CreateProject(SDFTextEffects         SDFTextEffects.cpp)
CreateProject(MultiFontSource        MultiFontSource.cpp)
CreateProject(TextVertical           TextVertical.cpp)
