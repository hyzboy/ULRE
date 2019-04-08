add_executable(VulkanTest   main.cpp
                            VKInstance.cpp)

target_link_libraries(VulkanTest PRIVATE ${ULRE} ${VULKAN_LIB})
