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
CreateProject(05_Billboard                      BillboardTest.cpp)
CreateProject(05b_BillboardECS                  BillboardECS.cpp)
CreateProject(05c_BillboardPerspectiveECS       BillboardPerspectiveECS.cpp)
CreateProject(05d_FacingMeshBillboardECS        FacingMeshBillboardECS.cpp)
CreateProject(05e_FacingMeshBillboardZECS       FacingMeshBillboardZECS.cpp)
CreateProject(06_SimpleCube                     SimpleCube.cpp)
CreateProject(06d_PBRSpheresECS                 PBRSpheresECS.cpp)
CreateProject(06b_BasicLitMeshesECS             06b_BasicLitMeshesECS.cpp)
CreateProject(06c_TextureBlinnPhongMeshesECS    06c_TextureBlinnPhongMeshesECS.cpp)
CreateProject(07_RenderBoundBox                 RenderBoundBox.cpp)
CreateProject(08_RenderToTexture                RenderToTexture.cpp)
CreateProject(09_RecursiveCube                  RecursiveCube.cpp)

CreateProject(11_BlendModeMaskedECS            BlendModeMaskedECS.cpp)
CreateProject(12_BlendModeDitherECS            BlendModeDitherECS.cpp)
CreateProject(13_BlendModeA2CECS               BlendModeA2CECS.cpp)
CreateProject(14_BlendModeTransparentECS       BlendModeTransparentECS.cpp)

