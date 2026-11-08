# T8.3：实机压测的 CTest 驱动器（**需要真实 Vulkan 设备 + 可开窗口的会话**）。
#
# 跑两遍，覆盖两条互斥的退出路径：
#   ① `CSTRESS_SELFCHECK=1`（摘要判据自带「强制冻结 + 回默认档」）
#      → 第 8 帧像素回读后即退出，判 `合批验证 PASS` / `Σ桶容量=3995` / `[CSTRESSSELF] PASS`
#   ② 常规运行（不冻结、采样子开）
#      → 跑满帧数再退，判 `溢出槽=0`（GPU 侧计数未超桶容量）+ `done frames=N`
#   两遍共同判：装配到定档规模、无 VUID / SYNC-HAZARD / `[ERROR]` / 竞技场容量拒批
#
# 为什么必须两遍：摘要判据在第 8 帧结束进程，早于任何采样帧；
# 而自检模式外的采样又拿不到确定性像素——两个判据装不进同一次运行。
#
# 用法（等价于 CTest 里那条 add_test）：
#   cmake -DEXAMPLE=<exe> -DRUNTIME_DIR=E:/ULRE -P ChessboardStressValidate.cmake

if(NOT DEFINED EXAMPLE OR NOT DEFINED RUNTIME_DIR)
    message(FATAL_ERROR "EXAMPLE and RUNTIME_DIR are required")
endif()

set(FRAMES 20)

function(clean_env)
    foreach(v CSTRESS_DUMP CSTRESS_STATIC CSTRESS_BOARDS CSTRESS_DICE CSTRESS_SELFCHECK CSTRESS_SAMPLE)
        unset(ENV{${v}})
    endforeach()
endfunction()

function(check_common label output)
    if(NOT output MATCHES "\\[CSTRESS\\] 规模定档" OR
       NOT output MATCHES "\\[CSTRESS\\] 场景装配完成")
        message(FATAL_ERROR "${label}: 场景未装配到定档规模\n${output}")
    endif()
    if(NOT output MATCHES "\\[CSTRESS\\] 合批验证 PASS")
        message(FATAL_ERROR "${label}: 合批验证未 PASS（桶组/竞技场行预算分配错）\n${output}")
    endif()
    # 默认档 Σ桶容量 = 5 × 729 + 350 = 3995（改默认档必须同步改这条与 01_ChessboardStress.cpp 文件头）
    if(NOT output MATCHES "Σ桶容量=3995（期望 3995）")
        message(FATAL_ERROR "${label}: Σ桶容量与默认档算式不符（期望 3995）\n${output}")
    endif()
    if(output MATCHES "VUID-|SYNC-HAZARD|hazard detected|\\[ERROR\\]|\\[FATAL\\]|竞技场实例行超容量")
        message(FATAL_ERROR "${label}: 运行期报错（校验层或竞技场容量）\n${output}")
    endif()
endfunction()

# ── ① 摘要自检：第 8 帧像素回读 ────────────────────────────────────
clean_env()
set(ENV{CSTRESS_SELFCHECK} "1")
set(ENV{CSTRESS_FRAMES} "${FRAMES}")
execute_process(
    COMMAND "${EXAMPLE}"
    WORKING_DIRECTORY "${RUNTIME_DIR}"
    RESULT_VARIABLE r1
    OUTPUT_VARIABLE o1
    ERROR_VARIABLE e1
    TIMEOUT 240)
set(out1 "${o1}\n${e1}")

if(NOT r1 STREQUAL "0")
    message(FATAL_ERROR "摘要自检退出码 ${r1}（PASS=0 / FAIL=1 / 回读失败=2）\n${out1}")
endif()
check_common("selfcheck" "${out1}")
if(NOT out1 MATCHES "\\[CSTRESSSELF\\] PASS")
    message(FATAL_ERROR "帧摘要自检未 PASS（像素基线漂移）\n${out1}")
endif()

# ── ② 常规运行：采样帧读 GPU 计数 ───────────────────────────────────
clean_env()
set(ENV{CSTRESS_FRAMES} "${FRAMES}")
set(ENV{CSTRESS_SAMPLE} "10")
execute_process(
    COMMAND "${EXAMPLE}"
    WORKING_DIRECTORY "${RUNTIME_DIR}"
    RESULT_VARIABLE r2
    OUTPUT_VARIABLE o2
    ERROR_VARIABLE e2
    TIMEOUT 240)
set(out2 "${o2}\n${e2}")

if(NOT r2 STREQUAL "0")
    message(FATAL_ERROR "常规运行退出码 ${r2}\n${out2}")
endif()
check_common("run" "${out2}")
if(NOT out2 MATCHES "溢出槽=0")
    message(FATAL_ERROR "GPU 侧溢出槽非 0（可见实例超桶容量 ⇒ 静默丢实例）\n${out2}")
endif()
if(NOT out2 MATCHES "\\[CSTRESS\\] done frames=${FRAMES}")
    message(FATAL_ERROR "未跑满 ${FRAMES} 帧\n${out2}")
endif()

message(STATUS "ChessboardStress: 729 棋盘 × 4 部件 + 350 骰子 —— 摘要自检 + 采样监控双路径通过")
