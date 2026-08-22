macro(CreateProject name)

    add_executable(${name} ${ARGN} ${VULKAN_APP_FRAMEWORK})
    target_link_libraries(${name} ${ULRE})

    IF(MSVC)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
        set_property(TARGET ${name} PROPERTY VS_DPI_AWARE "PerMonitor")
    ENDIF()

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Example/Basic")
endmacro()


CreateProject(DrawTriangle                   DrawTriangle.cpp)
CreateProject(SimpleMeshTriangle             SimpleMeshTriangle.cpp)
CreateProject(AutoInstance                   AutoInstance.cpp)
CreateProject(AutoMergeMaterialInstance      AutoMergeMaterialInstance.cpp)
CreateProject(ClockUse                       ClockUse.cpp)
CreateProject(SimpleCube                     SimpleCube.cpp)
CreateProject(MaterialRecipeEntry            MaterialRecipeEntry.cpp)
CreateProject(PBRSpheres                     PBRSpheres.cpp)
CreateProject(SingleSphereMaterialSwitch     SingleSphereMaterialSwitch.cpp)
CreateProject(BasicLitMeshes                 BasicLitMeshes.cpp)
CreateProject(TextureBlinnPhongMeshes        TextureBlinnPhongMeshes.cpp)
CreateProject(RenderBoundBox                 RenderBoundBox.cpp)
CreateProject(RenderToTexture                RenderToTexture.cpp)
CreateProject(RecursiveCube                  RecursiveCube.cpp)
