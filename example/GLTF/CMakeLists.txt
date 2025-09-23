find_package(fastgltf CONFIG REQUIRED)

# 纯控制台工具，不依赖引擎或 Vulkan 宏
add_executable(01_LoadGLTF
    LoadGLTF.cpp
    GLTFMaterial.h
    GLTFLoader.h
    GLTFLoader.cpp
)

target_link_libraries(01_LoadGLTF PRIVATE fastgltf::fastgltf)

if(MSVC)
    # 可选：工作目录设置为工程根/运行时目录，根据需要调整
    # set_target_properties(01_LoadGLTF PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_property(TARGET 01_LoadGLTF PROPERTY VS_DPI_AWARE "PerMonitor")
endif()

set_property(TARGET 01_LoadGLTF PROPERTY FOLDER "ULRE/Example/GLTF")
