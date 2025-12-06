#pragma once

#include<hgl/math/Vector.h>
#include<hgl/color/Color4f.h>
#include<hgl/type/String.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/log/Log.h>
#include <hgl/graph/geo/line/LineWidthBatch.h>

namespace hgl::graph
{
    constexpr const size_t MAX_LINE_WIDTH = 16;                 ///< CN: 最大支持线宽 EN: Maximum supported line width

    using LineColorPalette=Color4f[256];                        ///< CN: 线颜色调色板(256项) EN: Line color palette (256 entries)
    using UBOLineColorPalette=UBOInstance<LineColorPalette>;    ///< CN: 线颜色调色板UBO封装 EN: UBO instance for line color palette

    /** \class LineRenderManager
     * CN: 管理并绘制 3D/场景 中的调试/辅助/通用线段。核心特性:
     *  - 以线宽为索引存放多个批次(LineWidthBatch), 每个批次保存同一宽度的线段顶点与颜色索引数据。
     *  - 使用统一颜色调色板UBO(UBOLineColorPalette), 线段只记录颜色索引(uint8), 降低顶点数据量, 绘制前一次性绑定UBO。
     *  - 根据设备特性(support_wide_lines) 选择是否使用 GPU 动态线宽 (cmd->SetLineWidth)。不支持时所有线段统一宽度批次 index 0。
     *  - 支持在渲染目标(RenderTarget) 变化时安全地重建 Pipeline (保持材质实例不变)。
     *  - 提供单条/批量 添加、统一清理、统计总数 及 一次性绘制接口。
     *  - 利用 SharedLineBackup 实现批次之间的共享临时/备份缓冲, 减少重复分配与内存碎片。
     *
     * EN: Manages and renders 3D (debug/utility/general) line segments. Key features:
     *  - Maintains multiple batches (LineWidthBatch) indexed by line width; each batch holds vertex + color index data for a single width.
     *  - Uses a shared color palette UBO (UBOLineColorPalette); lines store only an 8-bit color index reducing per-vertex size; the UBO is bound once before drawing.
     *  - Chooses dynamic GPU line width path (cmd->SetLineWidth) if supported (support_wide_lines); otherwise all lines use batch index 0 (single width path).
     *  - Rebuilds graphics Pipeline safely when render target changes while keeping the material instance intact.
     *  - Provides APIs for single / bulk line addition, clearing, counting, and one-shot drawing.
     *  - Employs a SharedLineBackup to share temporary/backup buffers among batches to minimize reallocations and fragmentation.
     *
     * CN: 设计与实现原理摘要
     *  1) 数据存储: 每条线使用两个顶点, 顶点结构由位置 + 颜色索引(压缩为 uint8) 组成。位置直接进入顶点缓冲; 颜色索引在着色器中用作调色板查表。
     *  2) 调色板: 256 个 Color4f, 通过 UBO 绑定; 用户可随时调用 SetColor 修改其中条目, 后续绘制即时生效 (无需重建管线)。
     *  3) 宽度控制: 如果设备支持动态宽线, 逐批次设置 VkCmd SetLineWidth; 否则仅使用 line_groups[0] 统一宽度 (宽度参数仍被接受但逻辑集中至一个批次)。
     *  4) 资源共享: SharedLineBackup 用于批次间缓冲的复用或回退, 减少频繁创建/销毁 (具体实现参见 SharedLineBackup 定义)。
     *  5) Pipeline 重建: SetRenderTarget 触发重新基于新的 RenderPass 创建 Pipeline, 然后通知各批次更新。
     *  6) 统计: total_line_count 累加添加的线段数量, ClearLines 重置所有批次计数与统计。
     *
     * EN: Design / Implementation summary
     *  1) Storage: A line = two vertices; vertex layout packs position + an 8-bit palette index. Shader fetches actual Color4f from the palette UBO.
     *  2) Palette: 256 Color4f entries inside a UBO; SetColor updates a single entry with immediate effect (no pipeline rebuild required).
     *  3) Width handling: If device supports dynamic widths, per-batch width is applied via SetLineWidth; otherwise all lines are merged into line_groups[0].
     *  4) Resource sharing: SharedLineBackup enables buffer reuse/fallback across batches reducing frequent allocation/free cycles.
     *  5) Pipeline rebuild: SetRenderTarget creates a new Pipeline for the new RenderPass and updates each batch to use it.
     *  6) Statistics: total_line_count tracks the number of stored lines; ClearLines resets all batches and the counter.
     *
     * CN: 使用方法示例 (伪代码)
     *  RenderFramework *rf = ...;
     *  IRenderTarget  *rt = ...;
     *  LineRenderManager *lm = rf->GetLineRenderManager();
     *  lm->SetColor(0, Color4f(1,0,0,1));    // 设置调色板颜色
     *  lm->AddLine(p0, p1, 0, 2);             // 添加一条线 宽度=2 颜色索引=0
     *  lm->AddLine(p2, p3, 1, 1);             // 添加另一条线
     *  lm->Draw(cmd);                         // 在录制的命令缓冲中绘制
     *  lm->ClearLines();                      // 清空以便下一帧重填
     *
     * EN: Usage example (pseudo)
     *  RenderFramework *rf = ...;
     *  IRenderTarget  *rt = ...;
     *  LineRenderManager *lm = rf->GetLineRenderManager();
     *  lm->SetColor(0, Color4f(1,0,0,1));     // Set palette color
     *  lm->AddLine(p0, p1, 0, 2);             // Add a line width=2 color index=0
     *  lm->AddLine(p2, p3, 1, 1);             // Add another line
     *  lm->Draw(cmd);                         // Record draw commands
     *  lm->ClearLines();                      // Reset for next frame
     *
     * CN: 注意事项
     *  - 非线程安全: 多线程添加需外部同步。
     *  - 调色板索引越界会被忽略 (SetColor 内做范围判断)。
     *  - 宽度>MAX_LINE_WIDTH 或 宽度=0 的 AddLine 请求会被直接拒绝。
     *  - 当 device 不支持宽线时, width 仍可传入但仅使用批次0 (宽度由管线/光栅化状态决定)。
     *  - Draw 若 total_line_count=0 直接返回 true (空操作)。
     *
     * EN: Notes
     *  - Not thread-safe: external synchronization required for multi-threaded additions.
     *  - Out-of-range palette indices are ignored (range check in SetColor).
     *  - AddLine with width==0 or width>MAX_LINE_WIDTH fails fast.
     *  - If device lacks wide line support, all lines go to batch 0 (fixed rasterization width).
     *  - Draw returns true early when no lines are queued.
     */
    class LineRenderManager
    {
        OBJECT_LOGGER

    private:

        VulkanDevice *device;               ///< CN: Vulkan设备指针 EN: Vulkan device pointer
        bool support_wide_lines;            ///< CN: 是否支持动态宽线 EN: Whether dynamic wide line is supported

    private:

        UBOLineColorPalette *ubo_color;     ///< CN: 线颜色调色板UBO EN: Line color palette UBO

    private:

        LineWidthBatch line_groups[MAX_LINE_WIDTH];      ///< CN: 以线宽为索引的批次数组 EN: Batches indexed by line width (1..MAX or 0 only if unsupported)
        uint32 total_line_count=0;                       ///< CN: 当前累计线段数量 EN: Accumulated line count

    private:    

        MaterialInstance *  mi_line   = nullptr;         ///< CN: 线材质实例 EN: Line material instance
        Pipeline *          pipeline  = nullptr;         ///< CN: 当前渲染管线 EN: Active graphics pipeline
        SharedLineBackup *  shared_backup = nullptr;     ///< CN: 批次间共享缓冲备份 EN: Shared backup buffer among batches

    public:

        /**
         * CN: 构造函数 - 根据设备能力初始化批次数组, 并记录材质/管线/调色板引用。
         * EN: Constructor - Initializes batches based on device capability and stores material/pipeline/palette references.
         * \param dev CN: Vulkan设备 EN: Vulkan device
         * \param mi  CN: 材质实例 EN: Material instance
         * \param p   CN: 初始管线 EN: Initial pipeline
         * \param lcp CN: 颜色调色板UBO实例 EN: Color palette UBO instance
         */
        LineRenderManager(VulkanDevice *dev,MaterialInstance *mi,Pipeline *p,UBOLineColorPalette *lcp);

        /**
         * CN: 析构函数 - 释放共享备份对象, 其它资源由外部管理器负责。
         * EN: Destructor - Releases shared backup; other resources owned externally.
         */
        ~LineRenderManager();

        /**
         * CN: 设置新的渲染目标 (RenderTarget), 将基于其 RenderPass 重建 Pipeline, 并通知所有批次更新引用。
         * EN: Set a new RenderTarget; rebuilds the Pipeline against its RenderPass and updates all batches.
         * \param rt CN: 新的渲染目标 EN: New render target
         */
        void SetRenderTarget(IRenderTarget *rt);

        /**
         * CN: 设置调色板中指定索引(index:0-255)的颜色。若索引越界会被忽略。
         * EN: Set a color for the palette entry (index 0-255). Out-of-range indices are ignored.
         * \param index CN: 调色板索引 EN: Palette index
         * \param c CN: 颜色值 EN: Color value
         */
        void SetColor(const int index, const Color4f& c);

        /**
         * CN: 添加一条线段。
         * EN: Add a single line segment.
         * \param from CN: 起点 EN: Start point
         * \param to CN: 终点 EN: End point
         * \param color_index CN: 颜色调色板索引 EN: Palette color index
         * \param width CN: 线宽(1..MAX_LINE_WIDTH), 若超范围返回 false EN: Line width (1..MAX_LINE_WIDTH), fails if out of range
         * \return CN: 成功返回 true EN: True on success
         */
        bool AddLine(const math::Vector3f& from, const math::Vector3f& to, const uint8 color_index, uint8 width = 1);

        /**
         * CN: 批量添加多条线段 (共享相同的线宽)。
         * EN: Add multiple line segments (same width).
         * \param width CN: 线宽 EN: Line width
         * \param list CN: 线段描述数组 EN: Array of line segment descriptors
         * \return CN: 成功返回 true EN: True on success
         */
        bool AddLine(const uint8 width,const DataArray<LineSegmentDescriptor> &list);

        /**
         * CN: 清空所有批次中的线段数据与统计计数。
         * EN: Clear all batches and reset statistics.
         */
        void ClearLines();

        /**
         * CN: 绘制当前所有批次 (若支持宽线则逐批设置宽度)。当没有线段时快速返回 true。
         * EN: Draw all batches (setting line width per batch if supported). Returns true early if empty.
         * \param cmd CN: 渲染命令缓冲 EN: Render command buffer
         * \return CN: 成功返回 true EN: True on success
         */
        bool Draw(RenderCmdBuffer *cmd);

        /**
         * CN: 获取当前缓存的线段总数。
         * EN: Get the total number of stored line segments.
         * \return CN: 线段总数 EN: Line count
         */
        uint32 GetLineCount() const {return total_line_count;}
    };
}
