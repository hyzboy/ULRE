#pragma once

// ── Vulkan 特性宏配置 ─────────────────────────────────────────────────────────────
// 描述符池回退支持开关：
// 开启时（默认）：在不支持 Descriptor Buffer 或 RenderDoc 截获环境下，自动回退到传统的 DescriptorPool。
// 关闭时（注释）：未来或正式版中可彻底屏蔽 DescriptorPool 机制，仅保留纯 Descriptor Buffer 路径。
#define HGL_VK_DESCRIPTOR_POOL_FALLBACK
