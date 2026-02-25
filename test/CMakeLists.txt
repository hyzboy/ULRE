cmake_minimum_required(VERSION 3.28)

add_executable(test_ComposedShaderGenerator
    test_ComposedShaderGenerator.cpp)

target_link_libraries(test_ComposedShaderGenerator
    PRIVATE ULRE.ShaderGen
            ULRE.Vulkan)

add_executable(test_ComposedShaderGenerator_Verify
    test_ComposedShaderGenerator_Verify.cpp)

add_executable(test_MaterialShaderGen
    test_MaterialShaderGen.cpp)

target_link_libraries(test_MaterialShaderGen
    PRIVATE ULRE.ShaderGen
            ULRE.Vulkan)

include(CTest)
if(BUILD_TESTING)
    add_test(NAME test_ComposedShaderGenerator
             COMMAND test_ComposedShaderGenerator)

    add_test(NAME test_ComposedShaderGenerator_Verify
             COMMAND test_ComposedShaderGenerator_Verify)

    add_test(NAME test_MaterialShaderGen
             COMMAND test_MaterialShaderGen ${CMAKE_CURRENT_BINARY_DIR}/shader_gen_output)
endif()

# 需要时可直接运行：
#   cmake --build <build_dir> --target test_ComposedShaderGenerator
#   cmake --build <build_dir> --target test_ComposedShaderGenerator_Verify
#   cmake --build <build_dir> --target test_MaterialShaderGen
# 在生成目录执行：
#   .\\test\\Debug\\test_ComposedShaderGenerator.exe
#   .\\test\\Debug\\test_ComposedShaderGenerator_Verify.exe
#   .\\test\\Debug\\test_MaterialShaderGen.exe [output_dir]
