add_executable(VulkanTest   main.cpp
                            VKInstance.cpp
                            VKPhysicalDevice.cpp
                            VKDevice.cpp
                            VKCommandBuffer.cpp)

target_link_libraries(VulkanTest PRIVATE ${ULRE} ${VULKAN_LIB})
