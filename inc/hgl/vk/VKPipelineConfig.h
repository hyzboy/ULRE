#pragma once

namespace hgl::graph
{
    /// 检测当前进程是否被 RenderDoc 注入。
    ///
    /// RenderDoc 的 in-process 捕获库（renderdoc.dll）不支持以
    /// GraphicsPipelineLibrary 方式创建的管线——被注入时必须关闭 GPL，
    /// 走 Monolithic 物化路径。
    ///
    /// 策略分层：本函数只回答"是否被注入"；GPL 的启用决策集中在
    /// 设备创建处（VulkanDeviceCreater，结合 PhyDevice 硬件能力与本检测结果），
    /// PhyDevice 的 SupportGraphicsPipelineLibrary() 始终报告纯硬件能力。
    bool IsRenderDocInjected();
}//namespace hgl::graph
