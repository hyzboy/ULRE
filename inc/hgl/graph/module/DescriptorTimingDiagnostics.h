#pragma once

//=============================================================================
// Descriptor Timing Diagnostics - 可开关的时序诊断日志系统
//
// 通过启用/禁用 ENABLE_DESC_TIMING_LOGS 宏来全局控制所有时序日志输出
// 启用时：所有 LOG_DESC_TIMING(...) 调用会输出对应的 LogInfo 日志
// 禁用时：LOG_DESC_TIMING(...) 调用被完全优化掉，无任何性能开销
//=============================================================================

#ifndef HGL_DESCRIPTOR_TIMING_DIAGNOSTICS_H
#define HGL_DESCRIPTOR_TIMING_DIAGNOSTICS_H

// ============================================================================
// 启用/禁用开关 - 修改此处来控制是否输出时序诊断日志
// ============================================================================
#define ENABLE_DESC_TIMING_LOGS 1

// ============================================================================
// 日志宏定义
// ============================================================================
#if ENABLE_DESC_TIMING_LOGS

    // 信息级别日志 - 用于重要的时序检查点
    #define LOG_DESC_TIMING(fmt, ...) \
        LogInfo(u8"[#DESC_TIMING] " fmt, ##__VA_ARGS__)

    // Verbose级别日志 - 用于详细的中间步骤
    #define LOG_DESC_TIMING_VERBOSE(fmt, ...) \
        LogVerbose(u8"[#DESC_TIMING] " fmt, ##__VA_ARGS__)

#else

    // 禁用时：宏被替换为空操作，编译器会优化掉 (zero overhead)
    #define LOG_DESC_TIMING(fmt, ...) \
        do { (void)sizeof(fmt); } while(0)

    #define LOG_DESC_TIMING_VERBOSE(fmt, ...) \
        do { (void)sizeof(fmt); } while(0)

#endif // ENABLE_DESC_TIMING_LOGS

#endif // HGL_DESCRIPTOR_TIMING_DIAGNOSTICS_H
