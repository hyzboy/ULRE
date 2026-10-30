if(NOT DEFINED EXAMPLE OR NOT DEFINED RUNTIME_DIR)
    message(FATAL_ERROR "EXAMPLE and RUNTIME_DIR are required")
endif()

set(ENV{VK_LAYER_ENABLES} "VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT")
set(ENV{GC_FRAMES} "120")
set(ENV{GC_STATIC} "1")
unset(ENV{GC_SELFCHECK})
unset(ENV{GC_DUMP})

execute_process(
    COMMAND "${EXAMPLE}"
    WORKING_DIRECTORY "${RUNTIME_DIR}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
    TIMEOUT 120)
set(output "${stdout}\n${stderr}")

if(NOT result STREQUAL "0")
    message(FATAL_ERROR "GPUDrivenCull failed: ${result}\n${output}")
endif()
if(NOT output MATCHES "Inserted device layer \"VK_LAYER_KHRONOS_validation\"")
    message(FATAL_ERROR "Khronos validation layer was not enabled")
endif()
if(NOT output MATCHES "GPU cull instanced draw ENGAGED" OR
   NOT output MATCHES "\\[GC\\] done frames=120")
    message(FATAL_ERROR "GPU culling did not render all 120 frames\n${output}")
endif()
if(output MATCHES "hazard detected|VUID-|SYNC-HAZARD|\\[Validation\\].*(Error|ERROR|Invalid)")
    message(FATAL_ERROR "Vulkan validation reported an error\n${output}")
endif()
message(STATUS "GPU culling and swapchain synchronization passed validation for 120 frames")
