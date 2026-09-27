#pragma once

/**
 * per-frame 数据槽空间（A1/A7）：所有「每帧重写、GPU 异步读」的 CPU 侧数据
 * （L2W ring、CameraInfo 行、Viewport UBO、阴影 UBO …）共用这一段索引空间，
 * 索引一律取 `IRenderTarget::GetCurrentFrameIndex()`。
 *
 * 划分（固定上限，不为极端场景预留增长，超出即 fail-fast）：
 *   [0, HGL_FRAME_SLOT_MAIN)            —— 交换链主帧槽（= image_count，须 < MAIN）
 *   [HGL_FRAME_SLOT_MAIN, TOTAL)        —— 离屏 pass 槽（按 RT 顺序整带分配）
 *
 * 主帧槽与离屏槽**不相交**是关键：改造前离屏 RT 恒返回 0，prepass 的每帧数据
 * 会写坏在途主帧的槽 0（旧实现靠 RenderTo 开头的全槽排空兜住，见
 * RenderSystemCore::BeginFrame 的 T10 注释）。
 */
#ifndef HGL_FRAME_SLOT_TOTAL
#define HGL_FRAME_SLOT_TOTAL 8
#endif

#ifndef HGL_FRAME_SLOT_MAIN
#define HGL_FRAME_SLOT_MAIN 4
#endif

/**
 * L2W（local→world 矩阵 ring）深度。
 *
 * 必须与 per-frame 槽空间**一致**：L2W ring 的索引就是 frame_slot（主帧 0..3、离屏 4..7）。
 * ring 深度小于槽总数时（历史上是 3），离屏槽 4..7 取模后会与主帧槽别名（4%3=1、5%3=2、
 * 6%3=0、7%3=1），prepass 覆写主帧在途的矩阵 ⇒ 主帧 draw 读到退化矩阵、整帧只剩清屏色
 * （间歇性、竞态：实测 D3 转储三帧恒为清屏色 mean_lum=200.0）。
 */
#ifndef HGL_L2W_RING_FRAMES
#define HGL_L2W_RING_FRAMES HGL_FRAME_SLOT_TOTAL
#endif

#define HGL_TRANSFORM_DEBUG_LOGGING 0
