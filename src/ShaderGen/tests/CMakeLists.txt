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

# CompositorCompilerPipelineTests validates preparation-stage descriptor
# contract invariants and layout define injection for reflection output.
add_executable(CompositorCompilerPipelineTests
    CompositorCompilerPipelineTests.cpp
)

link_ulre_for_tests(CompositorCompilerPipelineTests)

set_property(TARGET CompositorCompilerPipelineTests PROPERTY FOLDER "ULRE/Tests/ShaderGen")

add_test(NAME CompositorCompilerPipelineTests COMMAND CompositorCompilerPipelineTests)

# ShaderCreateInfoLifecycleTests validates SPV cache lifecycle invariants:
# empty GLSL compile attempts must clear stale SPV payload.
add_executable(ShaderCreateInfoLifecycleTests
    ShaderCreateInfoLifecycleTests.cpp
)

link_ulre_for_tests(ShaderCreateInfoLifecycleTests)

set_property(TARGET ShaderCreateInfoLifecycleTests PROPERTY FOLDER "ULRE/Tests/ShaderGen")

add_test(NAME ShaderCreateInfoLifecycleTests COMMAND ShaderCreateInfoLifecycleTests)

# MaterialLifecycleStressTests validates repeated MaterialCreateInfo snapshot
# build/destroy ownership paths and (when available) repeated SPV compile
# lifecycle invariants under pressure rounds.
add_executable(MaterialLifecycleStressTests
    MaterialLifecycleStressTests.cpp
)

link_ulre_for_tests(MaterialLifecycleStressTests)

set_property(TARGET MaterialLifecycleStressTests PROPERTY FOLDER "ULRE/Tests/ShaderGen")

add_test(NAME MaterialLifecycleStressTests COMMAND MaterialLifecycleStressTests)

# ShaderCompilerThreadSmokeTests validates multi-threaded CompileShader smoke
# behavior when GLSLCompiler plugin is available.
add_executable(ShaderCompilerThreadSmokeTests
    ShaderCompilerThreadSmokeTests.cpp
)

link_ulre_for_tests(ShaderCompilerThreadSmokeTests)

set_property(TARGET ShaderCompilerThreadSmokeTests PROPERTY FOLDER "ULRE/Tests/ShaderGen")

add_test(NAME ShaderCompilerThreadSmokeTests COMMAND ShaderCompilerThreadSmokeTests)

# ShaderCompilerContextContractTests validates context apply/capture roundtrip,
# target-version mapping, and path-only context consistency behavior.
add_executable(ShaderCompilerContextContractTests
    ShaderCompilerContextContractTests.cpp
)

link_ulre_for_tests(ShaderCompilerContextContractTests)

set_property(TARGET ShaderCompilerContextContractTests PROPERTY FOLDER "ULRE/Tests/ShaderGen")

add_test(NAME ShaderCompilerContextContractTests COMMAND ShaderCompilerContextContractTests)

# VariantLookupCanonicalizationTests validates registry lookup canonicalization
# contracts for provider overrides, sky normalization, feature-mask matching,
# and billboard blend/pass route consistency.
add_executable(VariantLookupCanonicalizationTests
    VariantLookupCanonicalizationTests.cpp
)

link_ulre_for_tests(VariantLookupCanonicalizationTests)

set_property(TARGET VariantLookupCanonicalizationTests PROPERTY FOLDER "ULRE/Tests/ShaderGen")

add_test(NAME VariantLookupCanonicalizationTests COMMAND VariantLookupCanonicalizationTests)

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
