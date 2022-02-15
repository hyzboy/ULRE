include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common)

SET(VULKAN_APP_FRAMEWORK ${CMAKE_CURRENT_SOURCE_DIR}/common/VulkanAppFramework.h)

add_subdirectory(Vulkan)
add_subdirectory(2dVector)
add_subdirectory(GUI)
