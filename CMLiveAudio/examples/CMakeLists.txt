# CMLiveAudio Examples
cmake_minimum_required(VERSION 3.10)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../inc)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# ============================================================================
# 与 CMAudio examples 保持一致的示例工程宏：
#   统一 VC 工程文件夹为 Examples/CMLiveAudio/<sub_folder>
# ============================================================================
macro(cm_live_audio_example sub_folder project_name)
    add_executable(${project_name} ${ARGN})
    target_link_libraries(${project_name} PRIVATE CMLiveAudio CMAudio CMCore)

    # 设置 IDE 工程文件夹（与 CMAudio 的 Examples/CMAudio/... 风格一致）
    set_property(TARGET ${project_name} PROPERTY FOLDER "Examples/CMLiveAudio/${sub_folder}")

    if(WIN32 AND COMMAND set_debugger_directory)
        # 调试运行目录指向构建目录
        set_debugger_directory(${project_name} ${CMAKE_CURRENT_BINARY_DIR})
    endif()
endmacro()

# ---- 模块冒烟：库 + 跨模块链接 ----
cm_live_audio_example("Hello" hello hello.cpp)

# ---- IO 层：WASAPI 可行性 spike ----
cm_live_audio_example("IO" io_latency_spike io_latency_spike.cpp)
