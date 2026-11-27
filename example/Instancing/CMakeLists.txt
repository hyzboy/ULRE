# 实例化：RenderCollector 自动合并（同几何 / 同材质 / 同程序）批量绘制。

CreateProject(AutoInstance                   AutoInstance.cpp)
CreateProject(AutoMergeMaterialInstance      AutoMergeMaterialInstance.cpp)
CreateProject(GPUModelChessboard             GPUModelChessboard.cpp)
CreateProject(GPUModelMultiMaterial          GPUModelMultiMaterial.cpp)
CreateProject(GPUInstancedDataRows           GPUInstancedDataRows.cpp)

option(ULRE_ENABLE_GPU_MODEL_MULTI_MATERIAL_TEST "Run multi-material regression on a real Vulkan device" OFF)
if(BUILD_TESTING AND ULRE_ENABLE_GPU_MODEL_MULTI_MATERIAL_TEST)
    add_test(NAME GPUModelMultiMaterialIntegration
        COMMAND ${CMAKE_COMMAND}
            "-DEXAMPLE=$<TARGET_FILE:GPUModelMultiMaterial>"
            "-DRUNTIME_DIR=${ULRE_RUNTIME_PATH}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/GPUModelMultiMaterialValidate.cmake")
    set_tests_properties(GPUModelMultiMaterialIntegration PROPERTIES
        LABELS "vulkan;integration;gpu-model"
        RUN_SERIAL TRUE
        TIMEOUT 380)
endif()
