# T8.3 / P5：实机压测的 CTest 驱动器（**需要真实 Vulkan 设备 + 可开窗口的会话**）。
#
# 跑四遍，覆盖四条互斥的路径：
#   ① `CSTRESS_SELFCHECK=1` + 收集cache**关**      → 基线像素摘要（第 8 帧回读后即退出）
#   ② `CSTRESS_SELFCHECK=1` + 收集cache**开**      → 摘要必须与①**逐字节一致**
#   ③ 常规运行（不冻结、采样子开、cache 关）        → 溢出槽=0 + `done frames=N`
#   ④ 稳态运行（TOPO + 收集cache 开 + PROFILE）     → P2/P3/P4 快路径判据
#
# 为什么必须分开跑：
#   · 摘要判据在第 8 帧结束进程，早于任何采样帧；而自检模式外的采样又拿不到
#     确定性像素 —— 两个判据装不进同一次运行。
#   · ①② 互斥：cache 开/关是两条不同的代码路径（快路径 / 慢路径），只有分别
#     跑并比对摘要，才能证明快路径没有改变最终画面。
#   · ④ 需要足够的稳态帧数（暖机帧会建计划/建快照，不算命中），所以单独跑。
#
# 用法（等价于 CTest 里那条 add_test）：
#   cmake -DEXAMPLE=<exe> -DRUNTIME_DIR=E:/ULRE -P ChessboardStressValidate.cmake

if(NOT DEFINED EXAMPLE OR NOT DEFINED RUNTIME_DIR)
    message(FATAL_ERROR "EXAMPLE and RUNTIME_DIR are required")
endif()

set(SELF_FRAMES 20)
set(RUN_FRAMES 20)
set(STEADY_FRAMES 40)
set(STEADY_SAMPLE 30)

# 宿主环境里残留的开关会静默改变被测代码路径 ⇒ 每次开跑前清成已知状态。
# 新增 CSTRESS_* / ULRE_MAINFRAME_* / ULRE_* 开关时必须同步补进这张表。
function(clean_env)
    foreach(v CSTRESS_DUMP CSTRESS_STATIC CSTRESS_BOARDS CSTRESS_DICE CSTRESS_SELFCHECK
              CSTRESS_SAMPLE CSTRESS_PROFILE CSTRESS_TOPO CSTRESS_COLLECT_CACHE
              CSTRESS_FRAMES CSTRESS_NO_PHYSICS CSTRESS_ORBIT
              CSTRESS_YAW CSTRESS_PITCH CSTRESS_DIST CSTRESS_TX CSTRESS_TY CSTRESS_TZ
              ULRE_MAINFRAME_TOPO_CACHE ULRE_MAINFRAME_COLLECT_CACHE
              ULRE_REPLAY_PROBE ULRE_REPLAY_SKIP ULRE_BATCH_PROBE ULRE_NO_INSTANCING)
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

# 取 `key=N` 形式的整数值，缺项返回 -1
function(extract_int key text out_var)
    if(text MATCHES "${key}=([0-9]+)")
        set(${out_var} "${CMAKE_MATCH_1}" PARENT_SCOPE)
    else()
        set(${out_var} -1 PARENT_SCOPE)
    endif()
endfunction()

# ── ① 摘要自检：收集 cache 关（第 8 帧像素回读，基线） ──────────────
clean_env()
set(ENV{CSTRESS_SELFCHECK} "1")
set(ENV{CSTRESS_FRAMES} "${SELF_FRAMES}")
set(ENV{ULRE_MAINFRAME_COLLECT_CACHE} "0")
execute_process(
    COMMAND "${EXAMPLE}"
    WORKING_DIRECTORY "${RUNTIME_DIR}"
    RESULT_VARIABLE r1
    OUTPUT_VARIABLE o1
    ERROR_VARIABLE e1
    TIMEOUT 240)
set(out1 "${o1}\n${e1}")

if(NOT r1 STREQUAL "0")
    message(FATAL_ERROR "摘要自检(cache 关)退出码 ${r1}（PASS=0 / FAIL=1 / 回读失败=2）\n${out1}")
endif()
check_common("selfcheck-cache-off" "${out1}")
if(NOT out1 MATCHES "\\[CSTRESSSELF\\] PASS")
    message(FATAL_ERROR "帧摘要自检(cache 关)未 PASS（像素基线漂移）\n${out1}")
endif()
if(NOT out1 MATCHES "fnv1a64=(0x[0-9A-Fa-f]+)")
    message(FATAL_ERROR "摘要自检(cache 关)未打印 fnv1a64 摘要\n${out1}")
endif()
set(digest_off "${CMAKE_MATCH_1}")

# ── ② 摘要自检：收集 cache 开（快速路径），摘要必须与①完全一致 ────────
clean_env()
set(ENV{CSTRESS_SELFCHECK} "1")
set(ENV{CSTRESS_FRAMES} "${SELF_FRAMES}")
set(ENV{CSTRESS_TOPO} "1")
set(ENV{CSTRESS_COLLECT_CACHE} "1")
execute_process(
    COMMAND "${EXAMPLE}"
    WORKING_DIRECTORY "${RUNTIME_DIR}"
    RESULT_VARIABLE r2
    OUTPUT_VARIABLE o2
    ERROR_VARIABLE e2
    TIMEOUT 240)
set(out2 "${o2}\n${e2}")

if(NOT r2 STREQUAL "0")
    message(FATAL_ERROR "摘要自检(cache 开)退出码 ${r2}\n${out2}")
endif()
check_common("selfcheck-cache-on" "${out2}")
if(NOT out2 MATCHES "\\[CSTRESSSELF\\] PASS")
    message(FATAL_ERROR "帧摘要自检(cache 开)未 PASS（快路径改变了画面）\n${out2}")
endif()
if(NOT out2 MATCHES "fnv1a64=(0x[0-9A-Fa-f]+)")
    message(FATAL_ERROR "摘要自检(cache 开)未打印 fnv1a64 摘要\n${out2}")
endif()
set(digest_on "${CMAKE_MATCH_1}")

if(NOT digest_on STREQUAL digest_off)
    message(FATAL_ERROR
        "收集快路径与慢路径的画面不一致：cache 关=${digest_off} / cache 开=${digest_on}\n"
        "=> 快照成员或批计划与实体真实状态不符（台帐漏写者 / 计划过期）\n${out2}")
endif()

# ── ③ 常规运行：采样帧读 GPU 计数（cache 关，走原始慢路径） ──────────
clean_env()
set(ENV{CSTRESS_FRAMES} "${RUN_FRAMES}")
set(ENV{CSTRESS_SAMPLE} "10")
execute_process(
    COMMAND "${EXAMPLE}"
    WORKING_DIRECTORY "${RUNTIME_DIR}"
    RESULT_VARIABLE r3
    OUTPUT_VARIABLE o3
    ERROR_VARIABLE e3
    TIMEOUT 240)
set(out3 "${o3}\n${e3}")

if(NOT r3 STREQUAL "0")
    message(FATAL_ERROR "常规运行退出码 ${r3}\n${out3}")
endif()
check_common("run" "${out3}")
if(NOT out3 MATCHES "溢出槽=0")
    message(FATAL_ERROR "GPU 侧溢出槽非 0（可见实例超桶容量 => 静默丢实例）\n${out3}")
endif()
if(NOT out3 MATCHES "\\[CSTRESS\\] done frames=${RUN_FRAMES}")
    message(FATAL_ERROR "未跑满 ${RUN_FRAMES} 帧\n${out3}")
endif()

# ── ④ 稳态运行：TOPO + 收集 cache 开 + PROFILE，判快路径是否真接管 ────
#   读法见 01_ChessboardStress.cpp 的三个 Report* 注释块。这里只钉三条：
#     · P3 回退必须为 0（非 0 => 台帐漏了拓扑写者，快照带陈旧成员）
#     · P3 稳态必须为 yes，且接管数 >= 1（否则快路径形同虚设）
#     · P4 未命中增量 <= 2（允许暖机帧建计划；之后必须一直命中）
clean_env()
set(ENV{CSTRESS_FRAMES} "${STEADY_FRAMES}")
set(ENV{CSTRESS_SAMPLE} "${STEADY_SAMPLE}")
set(ENV{CSTRESS_PROFILE} "1")
set(ENV{CSTRESS_TOPO} "1")
set(ENV{CSTRESS_COLLECT_CACHE} "1")
execute_process(
    COMMAND "${EXAMPLE}"
    WORKING_DIRECTORY "${RUNTIME_DIR}"
    RESULT_VARIABLE r4
    OUTPUT_VARIABLE o4
    ERROR_VARIABLE e4
    TIMEOUT 240)
set(out4 "${o4}\n${e4}")

if(NOT r4 STREQUAL "0")
    message(FATAL_ERROR "稳态运行退出码 ${r4}\n${out4}")
endif()
check_common("steady" "${out4}")
if(NOT out4 MATCHES "\\[CSTRESS\\] done frames=${STEADY_FRAMES}")
    message(FATAL_ERROR "稳态运行未跑满 ${STEADY_FRAMES} 帧\n${out4}")
endif()
if(NOT out4 MATCHES "P3 主帧收集快路径")
    message(FATAL_ERROR "稳态运行未打印 P3 快路径报表（采样帧设置错？）\n${out4}")
endif()
if(NOT out4 MATCHES "P4 材质批计划重放")
    message(FATAL_ERROR "稳态运行未打印 P4 计划报表\n${out4}")
endif()

extract_int("接管" "${out4}" coll_hits)
extract_int("回退" "${out4}" coll_fallbacks)
extract_int("计划未命中" "${out4}" plan_misses)

if(coll_fallbacks LESS 0)
    message(FATAL_ERROR "稳态运行报表里找不到 回退=\n${out4}")
endif()
if(NOT coll_fallbacks EQUAL 0)
    message(FATAL_ERROR "P3 快路径有 ${coll_fallbacks} 次回退（台帐漏了拓扑写者？）\n${out4}")
endif()
if(NOT out4 MATCHES "稳态=yes")
    message(FATAL_ERROR "主帧收集未进入稳态（代数仍在增长 / 材质链未就绪）\n${out4}")
endif()
if(coll_hits LESS 1)
    message(FATAL_ERROR "P3 快路径一次都没接管（接管=${coll_hits}）\n${out4}")
endif()
if(plan_misses LESS 0)
    message(FATAL_ERROR "稳态运行报表里找不到 计划未命中=\n${out4}")
endif()
if(plan_misses GREATER 2)
    message(FATAL_ERROR "P4 批计划未命中 ${plan_misses} 次（稳态仍在重建：拓扑变化 / 项序列变化 / pass 指针不稳定）\n${out4}")
endif()

message(STATUS
    "ChessboardStress: 729 棋盘 × 4 部件 + 350 骰子 —— 双路径摘要对拍(${digest_off}) + 采样监控 + 稳态快路径(接管=${coll_hits} 回退=0 计划未命中=${plan_misses}) 通过")
