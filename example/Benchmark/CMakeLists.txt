# ─────────────────────────────────────────────────────────────────────────────
# Benchmark —— 阶段八 实机压测工程（T8.1 场景 / T8.2 监控）
#
# 与 `src/ecs/tests/TestGPUModelStress.cpp`（无头 CPU 口径）互补：
#   无头台架量「不碰显卡时」的 CPU 记账/内存收益（10,000 模型 / 40,000 部件）；
#   本目录跑**真机渲染路径**，规模受引擎三个 4096 硬上限约束（见各示例文件头注释）。
# ─────────────────────────────────────────────────────────────────────────────

CreateProject(ChessboardStress 01_ChessboardStress.cpp)

# T8.3：把压测包成 CTest。默认关——它需要**真实 Vulkan 设备 + 能开窗口的会话**，
# 无头 CI 上必然失败；按 example/Cull 的既有约定用选项隔离。
option(ULRE_ENABLE_GPU_MODEL_STRESS_TEST "注册 GPU 模型实机压测（729 棋盘 + 350 骰子，需真实 GPU）" OFF)
if(BUILD_TESTING AND ULRE_ENABLE_GPU_MODEL_STRESS_TEST)
    add_test(NAME GPUModelChessboardStress
        COMMAND ${CMAKE_COMMAND}
            "-DEXAMPLE=$<TARGET_FILE:ChessboardStress>"
            "-DRUNTIME_DIR=${ULRE_RUNTIME_PATH}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/ChessboardStressValidate.cmake")
    set_tests_properties(GPUModelChessboardStress PROPERTIES
        LABELS "vulkan;integration;stress"
        RUN_SERIAL TRUE
        TIMEOUT 300)
endif()
