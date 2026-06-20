# Tests for ULRE.ShaderGen
#
# MaterialKeyTest compiles MaterialKey.cpp directly so it does not pull in the
# full ULRE.ShaderGen static library (and its glslang/SPIRV-Cross transitive
# dependencies).  The global include_directories set by use_cm_module() in the
# root CMakeLists.txt provide all required header search paths.

function(shadergen_set_test_runtime_dir name)
    set_target_properties(${name} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY ${ULRE_RUNTIME_PATH})
endfunction()

add_executable(MaterialKeyTest
    MaterialKeyTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../MaterialKey.cpp
)

target_link_libraries(MaterialKeyTest PRIVATE CMCore)

set_property(TARGET MaterialKeyTest PROPERTY FOLDER "ULRE/Tests/ShaderGen")
shadergen_set_test_runtime_dir(MaterialKeyTest)

add_test(NAME MaterialKeyTest COMMAND MaterialKeyTest)

# RecipeToKeyTest links ULRE.ShaderGen because ResolveRecipePrimaryKey()
# depends on MapPresetToVariantKey() implemented in MaterialLibrary.cpp.
add_executable(RecipeToKeyTest
    RecipeToKeyTests.cpp
)

target_link_libraries(RecipeToKeyTest PRIVATE ${ULRE})

set_property(TARGET RecipeToKeyTest PROPERTY FOLDER "ULRE/Tests/ShaderGen")
shadergen_set_test_runtime_dir(RecipeToKeyTest)

add_test(NAME RecipeToKeyTest COMMAND RecipeToKeyTest)

# MaterialRecipeStoreTests links ULRE because MaterialRecipe depends on Vulkan
# types (VKFormat, VKGraphicsPipelinePreset) pulled in transitively.
add_executable(MaterialRecipeStoreTests
    MaterialRecipeStoreTests.cpp
)

target_link_libraries(MaterialRecipeStoreTests PRIVATE ${ULRE})

set_property(TARGET MaterialRecipeStoreTests PROPERTY FOLDER "ULRE/Tests/ShaderGen")
shadergen_set_test_runtime_dir(MaterialRecipeStoreTests)

add_test(NAME MaterialRecipeStoreTests COMMAND MaterialRecipeStoreTests)

add_executable(VariantRegistryTests
    VariantRegistryTests.cpp
)

target_link_libraries(VariantRegistryTests PRIVATE ${ULRE})

set_property(TARGET VariantRegistryTests PROPERTY FOLDER "ULRE/Tests/ShaderGen")
shadergen_set_test_runtime_dir(VariantRegistryTests)

add_test(NAME VariantRegistryTests COMMAND VariantRegistryTests)

add_executable(PositionProviderRegistryTests
    PositionProviderRegistryTests.cpp
)

target_link_libraries(PositionProviderRegistryTests PRIVATE ${ULRE})

set_property(TARGET PositionProviderRegistryTests PROPERTY FOLDER "ULRE/Tests/ShaderGen")
shadergen_set_test_runtime_dir(PositionProviderRegistryTests)

add_test(NAME PositionProviderRegistryTests COMMAND PositionProviderRegistryTests)

add_executable(CompositorAssemblerPositionKindTests
    CompositorAssemblerPositionKindTests.cpp
)

target_link_libraries(CompositorAssemblerPositionKindTests PRIVATE ${ULRE})

set_property(TARGET CompositorAssemblerPositionKindTests PROPERTY FOLDER "ULRE/Tests/ShaderGen")
shadergen_set_test_runtime_dir(CompositorAssemblerPositionKindTests)

add_test(NAME CompositorAssemblerPositionKindTests COMMAND CompositorAssemblerPositionKindTests)

add_executable(CompositorAssemblerSFMConstraintTests
    CompositorAssemblerSFMConstraintTests.cpp
)

target_link_libraries(CompositorAssemblerSFMConstraintTests PRIVATE ${ULRE})

set_property(TARGET CompositorAssemblerSFMConstraintTests PROPERTY FOLDER "ULRE/Tests/ShaderGen")
shadergen_set_test_runtime_dir(CompositorAssemblerSFMConstraintTests)

add_test(NAME CompositorAssemblerSFMConstraintTests COMMAND CompositorAssemblerSFMConstraintTests)

add_executable(SFMAuditSnapshotTests
    SFMAuditSnapshotTests.cpp
)

target_link_libraries(SFMAuditSnapshotTests PRIVATE ${ULRE})

set_property(TARGET SFMAuditSnapshotTests PROPERTY FOLDER "ULRE/Tests/ShaderGen")
shadergen_set_test_runtime_dir(SFMAuditSnapshotTests)

add_test(NAME SFMAuditSnapshotTests COMMAND SFMAuditSnapshotTests)

# EnumerateRecipeKeysTests exercises EnumerateRecipeKeys() and the new
# PassExpansion / StaticMaterialDefRegistry / toolchain-version code.
add_executable(EnumerateRecipeKeysTests
    EnumerateRecipeKeysTests.cpp
)

target_link_libraries(EnumerateRecipeKeysTests PRIVATE ${ULRE})

set_property(TARGET EnumerateRecipeKeysTests PROPERTY FOLDER "ULRE/Tests/ShaderGen")
shadergen_set_test_runtime_dir(EnumerateRecipeKeysTests)

add_test(NAME EnumerateRecipeKeysTests COMMAND EnumerateRecipeKeysTests)

# SFMAnnotationPolicyTests validates Phase2 Week1-1 whitelist and
# annotation error-code encoding/decoding helpers.
add_executable(SFMAnnotationPolicyTests
    SFMAnnotationPolicyTests.cpp
)

target_link_libraries(SFMAnnotationPolicyTests PRIVATE ${ULRE})

set_property(TARGET SFMAnnotationPolicyTests PROPERTY FOLDER "ULRE/Tests/ShaderGen")
shadergen_set_test_runtime_dir(SFMAnnotationPolicyTests)

add_test(NAME SFMAnnotationPolicyTests COMMAND SFMAnnotationPolicyTests)

# SFMAnnotationScannerTests validates the parser-backed Week1-2 entry point
# and the duplicate/conflict/derive diagnostics surfaced by the scanner.
add_executable(SFMAnnotationScannerTests
    SFMAnnotationScannerTests.cpp
)

target_link_libraries(SFMAnnotationScannerTests PRIVATE ${ULRE})

set_property(TARGET SFMAnnotationScannerTests PROPERTY FOLDER "ULRE/Tests/ShaderGen")
shadergen_set_test_runtime_dir(SFMAnnotationScannerTests)

add_test(NAME SFMAnnotationScannerTests COMMAND SFMAnnotationScannerTests)

# MaterialKeyAbiSnapshot guards sizeof/alignof, constants, and hash stability.
add_executable(MaterialKeyAbiSnapshot
    MaterialKeyAbiSnapshot.cpp
)

target_link_libraries(MaterialKeyAbiSnapshot PRIVATE ${ULRE})

set_property(TARGET MaterialKeyAbiSnapshot PROPERTY FOLDER "ULRE/Tests/ShaderGen")
shadergen_set_test_runtime_dir(MaterialKeyAbiSnapshot)

add_test(NAME MaterialKeyAbiSnapshot COMMAND MaterialKeyAbiSnapshot)

# ShaderBuildPipelineSmokeTests validates the new pipeline minimal chain
# (config validation + minimal source generation + context compile).
add_executable(ShaderBuildPipelineSmokeTests
    ShaderBuildPipelineSmokeTests.cpp
)

target_link_libraries(ShaderBuildPipelineSmokeTests PRIVATE ${ULRE})

set_property(TARGET ShaderBuildPipelineSmokeTests PROPERTY FOLDER "ULRE/Tests/ShaderGen")
shadergen_set_test_runtime_dir(ShaderBuildPipelineSmokeTests)

add_test(NAME ShaderBuildPipelineSmokeTests COMMAND ShaderBuildPipelineSmokeTests)
