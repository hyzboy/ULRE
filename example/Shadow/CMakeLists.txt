# 阴影：depth-only 阴影贴图、级联阴影(CSM)、Alpha 遮罩阴影。

macro(CreateProject name)

    add_executable(${name} ${ARGN} ${VULKAN_APP_FRAMEWORK})
    target_link_libraries(${name} ${ULRE})

    IF(MSVC)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
        set_property(TARGET ${name} PROPERTY VS_DPI_AWARE "PerMonitor")
    ENDIF()

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Example/Shadow")
endmacro()

CreateProject(ShadowMap                      ShadowMap.cpp)
CreateProject(CascadeShadowMap               CascadeShadowMap.cpp)
CreateProject(AlphaTestShadow                AlphaTestShadow.cpp)
