set(VK_INCLUDE_PATH     ${ROOT_INCLUDE_PATH}/hgl/vk)

# ============================================================================
# Vulkan Backend - 纯 Vulkan API 层实现
# ============================================================================

# --- 0. Core ---
SET(VK_CORE_FILES               ${VK_INCLUDE_PATH}/VK.h
                                ${VK_INCLUDE_PATH}/VKFormat.h
                                ${VK_INCLUDE_PATH}/VKString.h
                                ${VK_INCLUDE_PATH}/VKInterpolation.h
                                ${VK_INCLUDE_PATH}/VKStruct.h
                                ${VK_INCLUDE_PATH}/VKObjectName.h
                                ${VK_INCLUDE_PATH}/VKUUID.h
                                VKFormat.cpp
                                VKString.cpp)

SOURCE_GROUP("Core" FILES ${VK_CORE_FILES})

# --- 1. Device & Instance ---
SET(VK_DEVICE_INSTANCE_FILES    ${VK_INCLUDE_PATH}/VKInstance.h
                                VKInstance.cpp)

SOURCE_GROUP("Device\\Instance" FILES ${VK_DEVICE_INSTANCE_FILES})

SET(VK_DEVICE_PHYSICAL_FILES    ${VK_INCLUDE_PATH}/VKPhysicalDevice.h
                                ${VK_INCLUDE_PATH}/VKSurface.h
                                ${VK_INCLUDE_PATH}/VKSurfaceExtensionName.h
                                VKPhysicalDevice.cpp
                                ShaderGenPhysicalDeviceProfileAdapter.cpp
                                VKProperties.cpp
                                DebugOutProperties.h
                                Debug/VKPhysicalDeviceCapsOutput.cpp
                                VKSurface.cpp)

SOURCE_GROUP("Device\\PhysicalDevice" FILES ${VK_DEVICE_PHYSICAL_FILES})

SET(VK_DEVICE_LOGICAL_FILES     ${VK_INCLUDE_PATH}/VKDevice.h
                                ${VK_INCLUDE_PATH}/VKDeviceAttribute.h
                                ${VK_INCLUDE_PATH}/VKDeviceCreater.h
                                VKDeviceAttribute.cpp
                                VKDeviceCreater.cpp
                                VKDevice.cpp)

SOURCE_GROUP("Device\\Logical" FILES ${VK_DEVICE_LOGICAL_FILES})

SET(VK_DEVICE_DEBUG_FILES       ${VK_INCLUDE_PATH}/VKDebugOut.h
                                ${VK_INCLUDE_PATH}/VKDebugUtils.h
                                Debug/VKDebugOut.cpp
                                Debug/VKDebugUtils.cpp)

SOURCE_GROUP("Device\\Debug" FILES ${VK_DEVICE_DEBUG_FILES})

IF(WIN32)
    OPTION(FORCE_DISCETE_GPU    "Force Discrete GPU" OFF)

    IF(FORCE_DISCETE_GPU)
        SET(VULKAN_RENDER_DEVICE_SOURCE ForceDiscreteGPU.c)
    ENDIF()

    SET(VK_DEVICE_PLATFORM_FILES   platform/WinVulkan.cpp)
ENDIF(WIN32)

IF(UNIX)

    IF(ANDROID)
        SET(VK_DEVICE_PLATFORM_FILES   platform/AndroidVulkan.cpp)
    ELSE()
        IF(APPLE)
            if(CMAKE_SYSTEM_NAME STREQUAL "iOS" OR CMAKE_OSX_SYSROOT MATCHES ".*iPhone.*")
                message(STATUS "Targeting iOS")
                set(VK_DEVICE_PLATFORM_FILES platform/iOSVulkan.cpp)
            elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_OSX_SYSROOT MATCHES ".*MacOSX.*")
                message(STATUS "Targeting macOS")
                set(VK_DEVICE_PLATFORM_FILES platform/MacVulkan.cpp)
            else()
                message(FATAL_ERROR "Unknown Apple platform")
            endif()
        ELSE()
            find_package(PkgConfig REQUIRED)
            pkg_check_modules(WAYLAND_CLIENT wayland-client)
            if(WAYLAND_CLIENT_FOUND)
                message(STATUS "Targeting Wayland")
                set(VK_DEVICE_PLATFORM_FILES platform/WaylandVulkan.cpp)
            else()
                message(STATUS "Targeting XCB")
                set(VK_DEVICE_PLATFORM_FILES platform/XCBVulkan.cpp)
            endif()
        ENDIF()
    ENDIF()

ENDIF(UNIX)

SOURCE_GROUP("Device\\Platform" FILES ${VK_DEVICE_PLATFORM_FILES})

# --- 2. Memory Management ---
SET(VK_MEMORY_FILES             ${VK_INCLUDE_PATH}/VKMemory.h
                                ${VK_INCLUDE_PATH}/VKBuffer.h
                                ${VK_INCLUDE_PATH}/VKBufferAccessBase.h
                                ${VK_INCLUDE_PATH}/VKBufferAccessor.h
                                ${VK_INCLUDE_PATH}/VKBufferData.h
                                ${VK_INCLUDE_PATH}/VKIndexBuffer.h
                                ${VK_INCLUDE_PATH}/VKStagedBuffer.h
                                ${VK_INCLUDE_PATH}/VKStagedBufferAccessor.h
                                ${VK_INCLUDE_PATH}/VKReBarBuffer.h
                                ${VK_INCLUDE_PATH}/BufferPolicy.h
                                ${VK_INCLUDE_PATH}/DeviceBufferRingWriter.h
                                ${VK_INCLUDE_PATH}/VKRingBufferWrapper.h
                                ${VK_INCLUDE_PATH}/UBOAccessor.h
                                ${VK_INCLUDE_PATH}/AlignedStructureBuffer.h
                                VKMemory.cpp
                                VKBufferOwner.cpp
                                VKBuffer.cpp
                                VKRingBufferWrapper.cpp
                                VKStagedBuffer.cpp
                                VKReBarBuffer.cpp)

SOURCE_GROUP("Memory" FILES ${VK_MEMORY_FILES})

# --- 3. Vertex Input (与 Vulkan 底层密切) ---
SET(VK_VERTEX_INPUT_FILES       ${VK_INCLUDE_PATH}/VKVertexInput.h
                                ${VK_INCLUDE_PATH}/VKVertexInputAttribute.h
                                ${VK_INCLUDE_PATH}/VKVertexInputConfig.h
                                ${VK_INCLUDE_PATH}/VKVertexInputFormat.h
                                ${VK_INCLUDE_PATH}/VKVertexInputLayout.h
                                ${VK_INCLUDE_PATH}/VKVertexInputLayoutHash.h
                                ${VK_INCLUDE_PATH}/VKVertexAttribBuffer.h
                                ${VK_INCLUDE_PATH}/VKVABList.h
                                ${VK_INCLUDE_PATH}/VKPrimitiveType.h
                                VKVertexInput.cpp
                                VKVertexInputLayout.cpp
                                VKVertexInputLayoutHash.cpp
                                VkPrimitiveType.cpp)

SOURCE_GROUP("VertexInput" FILES ${VK_VERTEX_INPUT_FILES})

# --- 4. Pipeline ---
SET(PIPELINE_HEADER_PATH   ${VK_INCLUDE_PATH}/pipeline)
SET(PIPELINE_SOURCE_PATH   pipeline)

SET(VK_PIPELINE_FILES       ${PIPELINE_HEADER_PATH}/VKPipeline.h
                            ${PIPELINE_HEADER_PATH}/VKGplRequest.h
                            ${PIPELINE_HEADER_PATH}/VKGraphicsPipelineBuilder.h
                            ${PIPELINE_HEADER_PATH}/VKGplLibraryPool.h
                            ${PIPELINE_HEADER_PATH}/VKGplLibraryStatsTracker.h
                            ${PIPELINE_HEADER_PATH}/VKRenderStateProfile.h
                            ${PIPELINE_HEADER_PATH}/VKPipelineData.h
                            ${PIPELINE_HEADER_PATH}/VKPipelineLayoutData.h
                            ${PIPELINE_HEADER_PATH}/VKPipelineHash.h
                            ${PIPELINE_HEADER_PATH}/VKInlinePipeline.h
                            ${PIPELINE_HEADER_PATH}/VKComputePipeline.h
                            ${PIPELINE_SOURCE_PATH}/VKPipelineCompare.cpp
                            ${PIPELINE_SOURCE_PATH}/VKPipelineData.cpp
                            ${PIPELINE_SOURCE_PATH}/VKPipelineDataManage.cpp
                            ${PIPELINE_SOURCE_PATH}/VKPipelineDataSaveLoad.cpp
                            ${PIPELINE_SOURCE_PATH}/VKPipelineHash.cpp
                            ${PIPELINE_SOURCE_PATH}/VKGplRequest.cpp
                            ${PIPELINE_SOURCE_PATH}/VKGraphicsPipelineBuilder.cpp
                            ${PIPELINE_SOURCE_PATH}/VKGplLibraryPool.cpp
                            ${PIPELINE_SOURCE_PATH}/VKGplLibraryStatsTracker.cpp
                            ${PIPELINE_SOURCE_PATH}/VKRenderStateProfile.cpp
                            ${PIPELINE_SOURCE_PATH}/VKPipeline.cpp
                            ${PIPELINE_SOURCE_PATH}/VKPipelineCache.cpp
                            ${PIPELINE_SOURCE_PATH}/VKPipelineLayoutData.cpp
                            VKComputePipeline.cpp)

SOURCE_GROUP("Pipeline" FILES ${VK_PIPELINE_FILES})

# --- 5. Descriptor Sets ---
SET(VK_DESCRIPTOR_SETS_FILES    ${VK_INCLUDE_PATH}/VKDescriptorSet.h
                                ${VK_INCLUDE_PATH}/VKDynamicBufferAccess.h
                                VKDescriptorSet.cpp)

SOURCE_GROUP("DescriptorSets" FILES ${VK_DESCRIPTOR_SETS_FILES})

# --- 6. Shader ---
SET(VK_SHADER_FILES             ${VK_INCLUDE_PATH}/VKShaderModule.h
                                ${VK_INCLUDE_PATH}/VKShaderModuleMap.h
                                ${VK_INCLUDE_PATH}/VKShaderDescriptor.h
                                ${VK_INCLUDE_PATH}/VKShaderDescriptorSet.h
                                VKShaderModuleMap.cpp
                                VKShaderModule.cpp
                                VKShaderDescriptorSet.cpp
                                VKShaderStage.cpp)

SOURCE_GROUP("Shader" FILES ${VK_SHADER_FILES})

# --- 7. Image & Texture Base (纯 API 层) ---
SET(VK_IMAGE_FILES              ${VK_INCLUDE_PATH}/VKImageCreateInfo.h
                                ${VK_INCLUDE_PATH}/VKImageType.h
                                ${VK_INCLUDE_PATH}/VKImageView.h
                                ${VK_INCLUDE_PATH}/VKTexture.h
                                ${VK_INCLUDE_PATH}/VKTextureCreateInfo.h
                                ${VK_INCLUDE_PATH}/VKTextureType.h
                                ${VK_INCLUDE_PATH}/VKSampler.h
                                ${VK_INCLUDE_PATH}/VKSamplerType.h
                                VKImageView.cpp
                                VKTexture.cpp
                                VKSampler.cpp)

SOURCE_GROUP("Image" FILES ${VK_IMAGE_FILES})

SET(VK_TEXTURE_DEVICE_FILES     Texture/BufferImageCopy2D.h
                                Texture/CopyBufferToImage.h
                                Texture/GenMipmaps.cpp
                                Texture/VKDeviceTexture.cpp
                                Texture/VKDeviceTexture2D.cpp
                                Texture/VKDeviceTexture2DArray.cpp
                                Texture/VKDeviceTextureCube.cpp)

SOURCE_GROUP("Texture\\Device" FILES ${VK_TEXTURE_DEVICE_FILES})

# --- 8. Render Pass ---
SET(VK_RENDER_PASS_FILES        ${VK_INCLUDE_PATH}/pipeline/VKRenderFormat.h
                                ${VK_INCLUDE_PATH}/pipeline/VKLoadStoreOpInfo.h
                                ${VK_INCLUDE_PATH}/pipeline/VKAttachment.h
                                pipeline/VKRenderFormat.cpp)

SOURCE_GROUP("RenderPass" FILES ${VK_RENDER_PASS_FILES})

# --- 9. Render Target Base ---
SET(VK_RENDER_TARGET_FILES      ${VK_INCLUDE_PATH}/VKRenderTarget.h
                                ${VK_INCLUDE_PATH}/VKRenderTargetData.h
                                ${VK_INCLUDE_PATH}/VKRenderTargetSingle.h
                                ${VK_INCLUDE_PATH}/VKRenderTargetMultiFrame.h
                                ${VK_INCLUDE_PATH}/VKRenderTargetSwapchain.h
                                ${VK_INCLUDE_PATH}/VKRenderbufferInfo.h
                                VKRenderTarget.cpp
                                VKRenderTargetData.cpp
                                VKSwapchainRenderTarget.cpp)

SOURCE_GROUP("RenderTarget" FILES ${VK_RENDER_TARGET_FILES})

# --- 10. Swapchain ---
SET(VK_SWAPCHAIN_FILES          ${VK_INCLUDE_PATH}/VKSwapchain.h
                                VKSwapchain.cpp)

SOURCE_GROUP("Swapchain" FILES ${VK_SWAPCHAIN_FILES})

# --- 11. Command Buffer ---
SET(VK_COMMAND_BUFFER_FILES     ${VK_INCLUDE_PATH}/VKCommandBuffer.h
                                ${VK_INCLUDE_PATH}/VKIndirectCommandBuffer.h
                                VKCommandBuffer.cpp
                                VKCommandBufferRender.cpp
                                VKIndirectCommandBuffer.cpp)

SOURCE_GROUP("CommandBuffer" FILES ${VK_COMMAND_BUFFER_FILES})

# --- 12. Synchronization ---
SET(VK_SYNC_FILES               ${VK_INCLUDE_PATH}/VKSemaphore.h
                                ${VK_INCLUDE_PATH}/VKFence.h
                                ${VK_INCLUDE_PATH}/VKQueue.h
                                VKSemaphore.cpp
                                VKFence.cpp
                                VKQueue.cpp)

SOURCE_GROUP("Synchronization" FILES ${VK_SYNC_FILES})

# --- 13. Buffer Operations (Device Level) ---
SET(VK_BUFFER_OPS_FILES         VKDeviceMemory.cpp
                                VKDeviceBuffer.cpp
                                VKBufferAccessBase.cpp
                                VKDeviceStagedBuffer.cpp
                                VKDeviceImage.cpp
                                VKDeviceSampler.cpp
                                VKDeviceMaterial.cpp)

SOURCE_GROUP("BufferOperations" FILES ${VK_BUFFER_OPS_FILES})

SET(GEOMETRY_VERTEX_DATA_FILES  ${VK_INCLUDE_PATH}/VertexAttribDataAccess.h
                                ${VK_INCLUDE_PATH}/VertexDataManager.h
                                ${VK_INCLUDE_PATH}/VertexAttrib.h
                                VertexAttrib.cpp
                                VertexDataManager.cpp)

SOURCE_GROUP("VertexData" FILES ${GEOMETRY_VERTEX_DATA_FILES})

# --- 14. High-level Headers (SceneGraph/ECS 仍使用，但头文件在 vk 目录) ---
SET(VK_HIGH_LEVEL_HEADERS       ${VK_INCLUDE_PATH}/VKMaterial.h
                                ${VK_INCLUDE_PATH}/VKMaterialParameters.h
                                ${VK_INCLUDE_PATH}/VKMaterialInstance.h
                                ${VK_INCLUDE_PATH}/VKMaterialDescriptorManager.h
                                ${VK_INCLUDE_PATH}/VKTextureLoader.h)

SOURCE_GROUP("HighLevelHeaders" FILES ${VK_HIGH_LEVEL_HEADERS})

# ============================================================================
# Combine all Vulkan files
# ============================================================================

SET(VULKAN_ALL_FILES    ${VK_DEVICE_INSTANCE_FILES}
                        ${VK_CORE_FILES}
                        ${VK_DEVICE_PHYSICAL_FILES}
                        ${VK_DEVICE_LOGICAL_FILES}
                        ${VK_DEVICE_DEBUG_FILES}
                        ${VK_MEMORY_FILES}
                        ${VK_VERTEX_INPUT_FILES}
                        ${VK_PIPELINE_FILES}
                        ${VK_DESCRIPTOR_SETS_FILES}
                        ${VK_SHADER_FILES}
                        ${VK_IMAGE_FILES}
                        ${VK_TEXTURE_DEVICE_FILES}
                        ${VK_RENDER_PASS_FILES}
                        ${VK_RENDER_TARGET_FILES}
                        ${VK_SWAPCHAIN_FILES}
                        ${VK_COMMAND_BUFFER_FILES}
                        ${VK_SYNC_FILES}
                        ${VK_BUFFER_OPS_FILES}
                        ${VULKAN_RENDER_DEVICE_SOURCE}
                        ${GEOMETRY_VERTEX_DATA_FILES}
                        ${VK_HIGH_LEVEL_HEADERS})

# ============================================================================
# Build Library
# ============================================================================

add_cm_library(ULRE.Vulkan "ULRE"   ${VK_INCLUDE_PATH}
                                    ${VULKAN_ALL_FILES}
                                    ${VK_DEVICE_PLATFORM_FILES}
)

if(TARGET Vulkan::Vulkan)
    target_link_libraries(ULRE.Vulkan PUBLIC Vulkan::Vulkan)
elseif(WIN32)
    target_link_libraries(ULRE.Vulkan PUBLIC vulkan-1)
endif()
