# Tests for ULRE.ShaderGen
#
# MaterialKeyTest compiles MaterialKey.cpp directly so it does not pull in the
# full ULRE.ShaderGen static library (and its glslang/SPIRV-Cross transitive
# dependencies).  The global include_directories set by use_cm_module() in the
# root CMakeLists.txt provide all required header search paths.

function(link_ulre_for_tests target_name)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # GNU ld is order-sensitive for static archives; a second pass helps
        # resolve symbols from earlier archives referenced by later ones.
        target_link_libraries(${target_name} PRIVATE ${ULRE} ${ULRE})
    else()
        target_link_libraries(${target_name} PRIVATE ${ULRE})
    endif()
endfunction()

add_executable(MaterialKeyTest
    MaterialKeyTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../MaterialKey.cpp
)

target_link_libraries(MaterialKeyTest PRIVATE CMCore)

set_property(TARGET MaterialKeyTest PROPERTY FOLDER "ULRE/Tests/ShaderGen")

add_test(NAME MaterialKeyTest COMMAND MaterialKeyTest)

# RecipeToKeyTest links ULRE.ShaderGen because ResolveRecipePrimaryKey()
# depends on RouteKey() / MaterialLibrary routing implemented in MaterialLibrary.cpp.
add_executable(RecipeToKeyTest
    RecipeToKeyTests.cpp
)

link_ulre_for_tests(RecipeToKeyTest)

set_property(TARGET RecipeToKeyTest PROPERTY FOLDER "ULRE/Tests/ShaderGen")

add_test(NAME RecipeToKeyTest COMMAND RecipeToKeyTest)

# MaterialRecipeStoreTests links ULRE because MaterialRecipe depends on Vulkan
# types (VKFormat, VKGraphicsPipelinePreset) pulled in transitively.
add_executable(MaterialRecipeStoreTests
    MaterialRecipeStoreTests.cpp
)

link_ulre_for_tests(MaterialRecipeStoreTests)

set_property(TARGET MaterialRecipeStoreTests PROPERTY FOLDER "ULRE/Tests/ShaderGen")

add_test(NAME MaterialRecipeStoreTests COMMAND MaterialRecipeStoreTests)

# MaterialFactory3DRegistrationTests validates the explicit builtin factory
# registration entry and name coverage.
add_executable(MaterialFactory3DRegistrationTests
    MaterialFactory3DRegistrationTests.cpp
)

link_ulre_for_tests(MaterialFactory3DRegistrationTests)

set_property(TARGET MaterialFactory3DRegistrationTests PROPERTY FOLDER "ULRE/Tests/ShaderGen")

add_test(NAME MaterialFactory3DRegistrationTests COMMAND MaterialFactory3DRegistrationTests)

# EnumerateRecipeKeysTests exercises EnumerateRecipeKeys() and the new
# PassExpansion / StaticMaterialDefRegistry / toolchain-version code.
add_executable(EnumerateRecipeKeysTests
    EnumerateRecipeKeysTests.cpp
)

link_ulre_for_tests(EnumerateRecipeKeysTests)

set_property(TARGET EnumerateRecipeKeysTests PROPERTY FOLDER "ULRE/Tests/ShaderGen")

add_test(NAME EnumerateRecipeKeysTests COMMAND EnumerateRecipeKeysTests)

# CompositorAssemblerGoldenTests validates generated-source assembly output:
# key define injection, surface include routing, and read-failure diagnostics.
add_executable(CompositorAssemblerGoldenTests
    CompositorAssemblerGoldenTests.cpp
)

link_ulre_for_tests(CompositorAssemblerGoldenTests)

set_property(TARGET CompositorAssemblerGoldenTests PROPERTY FOLDER "ULRE/Tests/ShaderGen")

add_test(NAME CompositorAssemblerGoldenTests COMMAND CompositorAssemblerGoldenTests)

# MaterialKeyAbiSnapshot guards sizeof/alignof, constants, and hash stability.
add_executable(MaterialKeyAbiSnapshot
    MaterialKeyAbiSnapshot.cpp
)

link_ulre_for_tests(MaterialKeyAbiSnapshot)

set_property(TARGET MaterialKeyAbiSnapshot PROPERTY FOLDER "ULRE/Tests/ShaderGen")

add_test(NAME MaterialKeyAbiSnapshot COMMAND MaterialKeyAbiSnapshot)

# AttributeProviderRegistryTest compiles AttributeProviderRegistry.cpp directly
# (same pattern as MaterialKeyTest) to avoid pulling in full ULRE.ShaderGen.
add_executable(AttributeProviderRegistryTest
    AttributeProviderRegistryTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../AttributeProviderRegistry.cpp
)

target_link_libraries(AttributeProviderRegistryTest PRIVATE CMCore)

set_property(TARGET AttributeProviderRegistryTest PROPERTY FOLDER "ULRE/Tests/ShaderGen")

add_test(NAME AttributeProviderRegistryTest COMMAND AttributeProviderRegistryTest)

# PhaseB_EmitterTest exercises EmitAttribInput / EmitVertexStageInputs and
# MaterialVariantKey::attribute_providers hash behaviour.
# Compiles ShaderLayoutEmitter.cpp, ShaderWriter.cpp, and
# AttributeProviderRegistry.cpp directly to avoid pulling in full ULRE.ShaderGen.
add_executable(PhaseB_EmitterTest
    PhaseB_EmitterTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ShaderLayoutEmitter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../ShaderWriter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../AttributeProviderRegistry.cpp
)

target_link_libraries(PhaseB_EmitterTest PRIVATE CMCore)

set_property(TARGET PhaseB_EmitterTest PROPERTY FOLDER "ULRE/Tests/ShaderGen")

add_test(NAME PhaseB_EmitterTest COMMAND PhaseB_EmitterTest)

# VKBufferTest exercises SSBOOffsetHelper.h (pure constexpr utilities):
#   ComputeVABUsageFlags, AlignStorageBufferOffset, GetAttribProviderStride.
# No Vulkan device or runtime required — links CMCore only.
add_executable(VKBufferTest
    VKBufferTest.cpp
)

target_link_libraries(VKBufferTest PRIVATE CMCore)

set_property(TARGET VKBufferTest PROPERTY FOLDER "ULRE/Tests/ShaderGen")

add_test(NAME VKBufferTest COMMAND VKBufferTest)

# Phase0GoldenDump exports selected built-in variant assembled GLSL snapshots
# to doc/refactor/golden/phase0/ for phase-0 baseline completion.
add_executable(Phase0GoldenDump
    Phase0GoldenDump.cpp
)

link_ulre_for_tests(Phase0GoldenDump)

set_property(TARGET Phase0GoldenDump PROPERTY FOLDER "ULRE/Tools/ShaderGen")
