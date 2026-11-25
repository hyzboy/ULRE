# 调试可视化：包围盒、坐标轴、地面网格等场景辅助绘制。

CreateProject(RenderBoundBox                 RenderBoundBox.cpp)
CreateProject(SimplestAxis SimplestAxis.cpp)
CreateProject(PlaneGrid3D PlaneGrid3D.cpp)
CreateProject(RenderDiagnosticsFixture RenderDiagnosticsFixture.cpp)

option(ULRE_ENABLE_RENDER_BOUND_BOX_TEST "Run bounding-box line topology regression on a real Vulkan device" OFF)
if(BUILD_TESTING AND ULRE_ENABLE_RENDER_BOUND_BOX_TEST)
    add_test(NAME RenderBoundBoxIntegration
        COMMAND ${CMAKE_COMMAND}
            "-DEXAMPLE=$<TARGET_FILE:RenderBoundBox>"
            "-DRUNTIME_DIR=${ULRE_RUNTIME_PATH}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/RenderBoundBoxValidate.cmake")
    set_tests_properties(RenderBoundBoxIntegration PROPERTIES
        LABELS "vulkan;integration;render"
        RUN_SERIAL TRUE
        TIMEOUT 140)
endif()
