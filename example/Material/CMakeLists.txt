# 材质：PBR/Lit 配方、材质数据行(SSBO)、纹理集、运行时切换与 MaterialRecipe 入口。

macro(CreateProject name)

    add_executable(${name} ${ARGN} ${VULKAN_APP_FRAMEWORK})
    target_link_libraries(${name} ${ULRE})

    IF(MSVC)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
        set_property(TARGET ${name} PROPERTY VS_DPI_AWARE "PerMonitor")
    ENDIF()

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Example/Material")
endmacro()

CreateProject(MaterialRecipeEntry            MaterialRecipeEntry.cpp)
CreateProject(PBRSpheres                     PBRSpheres.cpp)
CreateProject(SingleSphereMaterialSwitch     SingleSphereMaterialSwitch.cpp)
CreateProject(BasicLitMeshes                 BasicLitMeshes.cpp)
CreateProject(FullPrecisionLitMeshes        FullPrecisionLitMeshes.cpp)
