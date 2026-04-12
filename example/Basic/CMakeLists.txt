macro(CreateProject name)

    add_executable(${name} ${ARGN} ${VULKAN_APP_FRAMEWORK})
    target_link_libraries(${name} ${ULRE})

    IF(MSVC)
        set_target_properties(${name} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
        set_property(TARGET ${name} PROPERTY VS_DPI_AWARE "PerMonitor")
    ENDIF()

    set_property(TARGET ${name} PROPERTY FOLDER "ULRE/Example/Basic")
endmacro()


CreateProject(01_draw_triangle                  draw_triangle.cpp)
CreateProject(02_auto_instance                  auto_instance.cpp)
CreateProject(03_auto_merge_material_instance   auto_merge_material_instance.cpp)
CreateProject(04_clock_use                      clock.cpp)
CreateProject(05_FacingMeshBillboardECS         FacingMeshBillboardECS.cpp)
CreateProject(06_FacingMeshBillboardZECS        FacingMeshBillboardZECS.cpp)
CreateProject(07_SimpleCube                     SimpleCube.cpp)
CreateProject(07a_SimpleCube_AutoTransparency   SimpleCube_AutoTransparency.cpp)
CreateProject(07b_SimpleCylinder               SimpleCylinder.cpp)
CreateProject(07c_SimpleTube                   SimpleTube.cpp)
CreateProject(08_PBRSpheresECS                  PBRSpheresECS.cpp)
CreateProject(09_BasicLitMeshesECS              BasicLitMeshesECS.cpp)
CreateProject(10_TextureBlinnPhongMeshesECS     TextureBlinnPhongMeshesECS.cpp)
CreateProject(11_RenderBoundBox                 RenderBoundBox.cpp)
CreateProject(12_RenderToTexture                RenderToTexture.cpp)
CreateProject(13_RecursiveCube                  RecursiveCube.cpp)
CreateProject(14_PBRColor3DSpheresECS           PBRColor3DSpheresECS.cpp)
CreateProject(15_PBRSpheresNTQualityECS         PBRSpheresNTQualityECS.cpp)


add_subdirectory(BillboardIconECS)

