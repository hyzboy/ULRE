#include <hgl/graph/geo/line/LineRenderManager.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/render/RenderContext.h>
#include <hgl/vk/VKRenderTarget.h>
#include <hgl/graph/geo/GeometryCreater.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/VKVertexInputLayout.h>
#include <hgl/graph/geo/VKGeometry.h>
#include <hgl/graph/module/MaterialManager.h>
#include <hgl/graph/mtl/Material3DCreateConfig.h>
#include <hgl/vk/VKMaterial.h>
#include <hgl/vk/VKVertexInputConfig.h>
#include <hgl/graph/module/GeometryManager.h>
#include <hgl/graph/module/PrimitiveManager.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/graph/mtl/UBOCommon.h>
#include <hgl/vk/StructuredBufferAccessor.h>
#include <hgl/object/ObjectTracker.h>

/**
 * \file LineRenderManager.cpp
 * CN: 提供 LineRenderManager 的实现。包含: 工厂创建函数、渲染目标切换、颜色调色板写入、线段添加/批量添加、清理与绘制逻辑。
 * EN: Implementation for LineRenderManager. Contains: factory creation, render target switching, palette color update, single/bulk line add, clearing and draw logic.
 *
 * CN: 主要执行流程概述:
 *  1) CreateLineRenderManager: 创建材质/实例/管线/调色板UBO并实例化管理器。
 *  2) AddLine: 将线段写入对应宽度批次(LineWidthBatch), 不支持宽线时全部进入 index0。
 *  3) Draw: 绑定调色板UBO与管线, 遍历批次(支持宽线则循环设置 SetLineWidth), 调用批次 Draw。
 *  4) SetRenderTarget: 依据新的 RenderPass 重建 Pipeline 并刷新批次引用。
 *
 * EN: Main execution flow summary:
 *  1) CreateLineRenderManager: Build material/instance/pipeline/palette UBO and construct manager.
 *  2) AddLine: Append line(s) into width batch; if wide lines unsupported everything goes to batch index 0.
 *  3) Draw: Bind palette UBO & pipeline, iterate batches (setting SetLineWidth when supported) then batch Draw.
 *  4) SetRenderTarget: Recreate Pipeline for new RenderPass and update batches.
 */

DEFINE_LOGGER_MODULE(LineRenderManager)

namespace hgl::graph
{
        static inline uint8 NormalizeBatchIndex(const bool support_wide_lines, const uint8 width)
        {
            if (!support_wide_lines)
                return 0;

            if (width == 0)
                return 0;

            const uint8 clamped = width > MAX_LINE_WIDTH ? static_cast<uint8>(MAX_LINE_WIDTH) : width;
            return static_cast<uint8>(clamped - 1);
        }

    constexpr const size_t LINE_COUNT_INCREMENT =       1024;          ///< CN: 线段容量增量基数 EN: line count allocation granularity
    constexpr const size_t POSITION_COMPONENT_COUNT =   6;             ///< CN: 位置分量(2个点*3) EN: position components (2 points * 3)
    constexpr const size_t COLOR_COMPONENT_COUNT =      2;             ///< CN: 颜色索引分量(2个端点) EN: color index components (2 endpoints)

    constexpr const size_t POSITION_COUNT_INCREMENT = LINE_COUNT_INCREMENT * POSITION_COMPONENT_COUNT; ///< CN: 位置缓冲增量 EN: position buffer increment
    constexpr const size_t COLOR_COUNT_INCREMENT    = LINE_COUNT_INCREMENT * COLOR_COMPONENT_COUNT;    ///< CN: 颜色缓冲增量 EN: color buffer increment

    /**
     * CN: 工厂函数 创建 LineRenderManager。
     * EN: Factory function to create a LineRenderManager.
     * \param rf CN: 渲染框架 EN: render framework
     * \param rt CN: 渲染目标 EN: render target
     * \return CN: 创建成功的管理器或nullptr EN: created manager or nullptr
     */
    LineRenderManager* CreateLineRenderManager(GraphicsContext *gc,IRenderTarget *rt)
    {
        HGL_CAPTURE_SCOPE();

        if (!gc)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: CreateLineRenderManager失败 GraphicsContext为空 EN: graphics context is null"));
            return nullptr;
        }

        // Create PureColor3D material via factory
        mtl::Material3DCreateConfig cfg(PrimitiveType::Lines,
                                        mtl::WithCamera::With,
                                        mtl::WithLocalToWorld::Without,
                                        mtl::WithSky::Without);

        auto *mci = mtl::CreateVertexPattleColor3D(gc->GetDevAttr(), &cfg);

        if (mci == nullptr)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: 创建MaterialCreateInfo失败 EN: failed to create material create info"));
            return nullptr;
        }

        auto *mat_mgr = gc->GetMaterialManager();
        if (!mat_mgr)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: 获取MaterialManager失败 EN: material manager is null"));
            delete mci;
            return nullptr;
        }

        Material *mat = mat_mgr->CreateMaterial("M_Line3D", mci);

        if (mat == nullptr)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: 创建Material失败 EN: failed to create material"));
            delete mci;
            return nullptr;
        }

        VILConfig vil_config;
        vil_config.Add(VAN::Color,VF_V1U8);

        MaterialInstance *mi = mat_mgr->CreateMaterialInstance(mat,&vil_config);

        if (mi == nullptr)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: 创建MaterialInstance失败 EN: failed to create material instance"));
            delete mci;
            return nullptr;
        }

        if(!rt)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: 渲染目标为空 EN: render target is null"));
            delete mci;
            return nullptr;
        }

        RenderPass *rp = rt->GetRenderPass();
        if(!rp)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: 获取RenderPass失败 EN: failed to get render pass"));
            delete mci;
            return nullptr;
        }

        VulkanDevAttr *dev_attr = gc->GetDevAttr();
        if(!dev_attr)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: VulkanDevAttr为空 EN: VulkanDevAttr is null"));
            delete mci;
            return nullptr;
        }

        Pipeline *p = nullptr;

        if(dev_attr->wide_lines)
        {
            p = rp->CreatePipeline(mi,InlinePipeline::DynamicLineWidth3D);
            if(!p)
                MLogError(LineRenderManager,OS_TEXT("CN: 创建动态线宽管线失败 EN: failed to create dynamic line width pipeline"));
        }
        else
        {
            p = rp->CreatePipeline(mi,InlinePipeline::Solid3D);
            if(!p)
                MLogError(LineRenderManager,OS_TEXT("CN: 创建普通线管线失败 EN: failed to create solid line pipeline"));
        }

        if(!p)
        {
            delete mci;
            return nullptr;
        }

        VulkanDevice *device = gc->GetDevice();
        if (!device)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: VulkanDevice为空 EN: VulkanDevice is null"));
            delete mci;
            return nullptr;
        }

        auto *buffer_manager = gc->GetBufferManager();
        if (!buffer_manager)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: 获取BufferManager失败 EN: buffer manager is null"));
            delete mci;
            return nullptr;
        }

        auto *buf = buffer_manager->CreateUBO("LineColorPaletteUBO", StructuredBufferAccessor<LineColorPalette>::GetSize());
        if (!buf)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: 创建颜色调色板UBO失败 EN: failed to create palette UBO"));
            delete mci;
            return nullptr;
        }
        buf->SetUpdateClass(BufferUpdateClass::Default);

        UBOLineColorPalette *lcp = StructuredBufferAccessor<LineColorPalette>::Create(buf, &mtl::SBS_ColorPattle, false);
        if(!lcp)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: 创建颜色调色板UBO失败 EN: failed to create palette UBO"));
            delete mci;
            return nullptr;
        }

        LineRenderManager *mgr = new LineRenderManager(device,mi,p,lcp,mat_mgr,buffer_manager);
        if(!mgr)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: 分配LineRenderManager失败 EN: allocation of LineRenderManager failed"));
            delete lcp;
            delete mci;
            return nullptr;
        }

        MLogInfo(LineRenderManager,OS_TEXT("CN: 成功创建LineRenderManager EN: LineRenderManager created successfully"));
        return mgr;
    }

    LineRenderManager* CreateLineRenderManager(RenderContext *rc,IRenderTarget *rt)
    {
        HGL_CAPTURE_SCOPE();

        if (!rc)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: CreateLineRenderManager失败 RenderContext为空 EN: render context is null"));
            return nullptr;
        }

        auto *graphics_context = rc->GetGraphicsContext();
        if (!graphics_context)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: GraphicsContext为空 EN: graphics context is null"));
            return nullptr;
        }

        VulkanDevice *device = graphics_context->GetDevice();
        if (!device)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: VulkanDevice为空 EN: VulkanDevice is null"));
            return nullptr;
        }

        VulkanDevAttr *dev_attr = device->GetDevAttr();
        if(!dev_attr)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: VulkanDevAttr为空 EN: VulkanDevAttr is null"));
            return nullptr;
        }

        mtl::Material3DCreateConfig cfg(PrimitiveType::Lines,
                                        mtl::WithCamera::With,
                                        mtl::WithLocalToWorld::Without,
                                        mtl::WithSky::Without);

        auto *mci = mtl::CreateVertexPattleColor3D(dev_attr, &cfg);
        if (mci == nullptr)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: 创建MaterialCreateInfo失败 EN: failed to create material create info"));
            return nullptr;
        }

        auto *mat_mgr = graphics_context->GetMaterialManager();
        if (!mat_mgr)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: 获取MaterialManager失败 EN: material manager is null"));
            delete mci;
            return nullptr;
        }

        Material *mat = mat_mgr->CreateMaterial("M_Line3D", mci);
        if (mat == nullptr)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: 创建Material失败 EN: failed to create material"));
            delete mci;
            return nullptr;
        }

        VILConfig vil_config;
        vil_config.Add(VAN::Color,VF_V1U8);

        MaterialInstance *mi = mat_mgr->CreateMaterialInstance(mat,&vil_config);
        if (mi == nullptr)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: 创建MaterialInstance失败 EN: failed to create material instance"));
            delete mci;
            return nullptr;
        }

        if(!rt)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: 渲染目标为空 EN: render target is null"));
            delete mci;
            return nullptr;
        }

        RenderPass *rp = rt->GetRenderPass();
        if(!rp)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: 获取RenderPass失败 EN: failed to get render pass"));
            delete mci;
            return nullptr;
        }

        Pipeline *p = nullptr;

        if(dev_attr->wide_lines)
        {
            p = rp->CreatePipeline(mi,InlinePipeline::DynamicLineWidth3D);
            if(!p)
                MLogError(LineRenderManager,OS_TEXT("CN: 创建动态线宽管线失败 EN: failed to create dynamic line width pipeline"));
        }
        else
        {
            p = rp->CreatePipeline(mi,InlinePipeline::Solid3D);
            if(!p)
                MLogError(LineRenderManager,OS_TEXT("CN: 创建普通线管线失败 EN: failed to create solid line pipeline"));
        }

        if(!p)
        {
            delete mci;
            return nullptr;
        }

        auto *buffer_manager = graphics_context->GetBufferManager();
        if (!buffer_manager)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: 获取BufferManager失败 EN: buffer manager is null"));
            delete mci;
            return nullptr;
        }

        auto *buf = buffer_manager->CreateUBO("LineColorPaletteUBO", StructuredBufferAccessor<LineColorPalette>::GetSize());
        if (!buf)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: 创建颜色调色板UBO失败 EN: failed to create palette UBO"));
            delete mci;
            return nullptr;
        }
        buf->SetUpdateClass(BufferUpdateClass::Default);

        UBOLineColorPalette *lcp = StructuredBufferAccessor<LineColorPalette>::Create(buf, &mtl::SBS_ColorPattle, false);
        if(!lcp)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: 创建颜色调色板UBO失败 EN: failed to create palette UBO"));
            delete mci;
            return nullptr;
        }

        LineRenderManager *mgr = new LineRenderManager(device,mi,p,lcp,mat_mgr,buffer_manager);
        if(!mgr)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: 分配LineRenderManager失败 EN: allocation of LineRenderManager failed"));
            delete lcp;
            delete mci;
            return nullptr;
        }

        MLogInfo(LineRenderManager,OS_TEXT("CN: 成功创建LineRenderManager EN: LineRenderManager created successfully"));
        return mgr;
    }

    /**
     * CN: 切换渲染目标, 如果成功则重建管线并更新批次。空指针或失败路径记录日志。
     * EN: Switch render target; on success rebuild pipeline and update batches. Logs on error paths.
     */
    void LineRenderManager::SetRenderTarget(IRenderTarget *rt)
    {
        if(!rt)
        {
            LogError(OS_TEXT("CN: SetRenderTarget失败 参数rt为空 EN: rt is null"));
            return;
        }
        if(!mi_line)
        {
            LogError(OS_TEXT("CN: SetRenderTarget失败 mi_line为空 EN: mi_line is null"));
            return;
        }

        RenderPass *rp = rt->GetRenderPass();
        if(!rp)
        {
            LogError(OS_TEXT("CN: SetRenderTarget失败 获取RenderPass为空 EN: failed to get RenderPass"));
            return;
        }

        Pipeline *new_pipeline = nullptr;
        if(support_wide_lines)
            new_pipeline = rp->CreatePipeline(mi_line,InlinePipeline::DynamicLineWidth3D);
        else
            new_pipeline = rp->CreatePipeline(mi_line,InlinePipeline::Solid3D);

        if(!new_pipeline)
        {
            LogError(OS_TEXT("CN: 重建Pipeline失败 保留旧Pipeline EN: failed to recreate pipeline; keep old one"));
            return; // keep old pipeline
        }

        //注：pipeline都是由RenderPass自己管理释放的，不同手动释放

        pipeline = new_pipeline;
        LogInfo(OS_TEXT("CN: 渲染目标切换并重建Pipeline成功 EN: render target switched and pipeline rebuilt"));

        if(support_wide_lines)
        {
            for(size_t i=0;i<MAX_LINE_WIDTH;i++)
                line_groups[i].UpdatePipeline(pipeline);
        }
        else
        {
            line_groups[0].UpdatePipeline(pipeline);
        }
    }

    /**
     * CN: 构造 - 初始化批次, 分配共享备份, 根据宽线支持性设定批次数量。
     * EN: Constructor - Initialize batches, allocate shared backup, configure depending on wide line support.
     */
    LineRenderManager::LineRenderManager(VulkanDevice *dev,MaterialInstance *mi,Pipeline *p,UBOLineColorPalette *lcp,MaterialManager *mm,BufferManager *bm)
    {
        support_wide_lines = dev && dev->GetDevAttr() ? dev->GetDevAttr()->wide_lines : false;

        device = dev;
        mi_line = mi;
        pipeline=p;
        ubo_color=lcp;
        material_manager = mm;
        buffer_manager = bm;

        if(mi_line && ubo_color)
        {
            Material *mat = mi_line->GetMaterial();
            if(mat)
            {
                mat->BindUBO(&mtl::SBS_ColorPattle, ubo_color->GetBuffer());
                mat->Update();
            }
        }

        shared_backup = new SharedLineBackup();
        if(!shared_backup)
            LogError(OS_TEXT("CN: 分配SharedLineBackup失败 EN: failed to allocate SharedLineBackup"));

        if(support_wide_lines)
        {
            for(uint i = 0;i < MAX_LINE_WIDTH;i++)
            {
                AnsiString batch_name = "LineRenderManager:LineWidth" + AnsiString::numberOf(i + 1);
                line_groups[i].Init(i + 1,device,mi,pipeline,shared_backup,batch_name);
            }
            LogInfo(OS_TEXT("CN: 初始化宽线批次数量=") + OSString::numberOf(MAX_LINE_WIDTH) + OS_TEXT(" EN: initialized wide line batches"));
        }
        else
        {
            AnsiString batch_name = "LineRenderManager:LineWidthUnified";
            line_groups[0].Init(0,device,mi,pipeline,shared_backup,batch_name);
            LogInfo(OS_TEXT("CN: 设备不支持宽线 使用单一批次 EN: wide line unsupported; using single batch"));
        }

        total_line_count = 0;
    }

    /**
     * CN: 析构 - 释放共享备份。
     * EN: Destructor - release shared backup.
     */
    LineRenderManager::~LineRenderManager()
    {
        // CN: 清理所有批次中的几何体和原始体 EN: Clear all batches' geometry and primitives
        if(support_wide_lines)
        {
            for(size_t i=0;i<MAX_LINE_WIDTH;i++)
                line_groups[i].Clear();
        }
        else
        {
            line_groups[0].Clear();
        }

        if (material_manager && mi_line)
        {
            Material *mat = mi_line->GetMaterial();
            material_manager->Destroy(mi_line);
            if (mat)
                material_manager->Destroy(mat);
        }

        if (ubo_color)
        {
            DeviceBuffer *buf = ubo_color->ubo();
            delete ubo_color;
            if (buffer_manager && buf)
                buffer_manager->Release(buf);
        }

        delete shared_backup;
        shared_backup = nullptr;
    }

    /**
     * CN: 写入调色板颜色; 索引越界将记录警告并返回。
     * EN: Update palette color; out-of-range index logs warning and returns.
     */
    void LineRenderManager::SetColor(const int index, const Color4f& c)
    {
        if(!ubo_color)
        {
            LogError(OS_TEXT("CN: 调色板UBO为空 EN: palette UBO is null"));
            return;
        }

        if(index < 0 || index >= 256)
        {
            LogWarning(OS_TEXT("CN: 调色板索引越界 EN: palette index out of range"));
            return;
        }

        auto *palette = ubo_color->Data();
        if(!palette)
            return;

        (*palette)[index] = c;
        ubo_color->MarkDirty();
        ubo_color->Commit();
    }

    bool LineRenderManager::AddLine(const Vector3f& from, const Vector3f& to, const uint8 color_index, uint8 width)
    {
        if(width == 0 || width > MAX_LINE_WIDTH)
            return false;

        if(support_wide_lines)
        {
            line_groups[width - 1].AddLine(from, to, color_index);
        }
        else
        {
            line_groups[0].AddLine(from, to, color_index);
        }

        ++total_line_count;
        return true;
    }

    bool LineRenderManager::AddLine(const uint8 width, const std::vector<LineSegmentDescriptor> &list)
    {
        if(width == 0 || width > MAX_LINE_WIDTH)
            return false;

        if(list.empty())
            return true;

        if(support_wide_lines)
        {
            line_groups[width - 1].AddLine(list);
        }
        else
        {
            line_groups[0].AddLine(list);
        }

        total_line_count += static_cast<uint32>(list.size());
        return true;
    }

    void LineRenderManager::ClearLines()
    {
        if(support_wide_lines)
        {
            for(size_t i = 0; i < MAX_LINE_WIDTH; ++i)
            {
                line_groups[i].Reset();
            }
        }
        else
        {
            line_groups[0].Reset();
        }

        total_line_count = 0;
    }

    bool LineRenderManager::Draw(RenderCmdBuffer *cmd)
    {
        if(!cmd)
            return false;

        if(total_line_count == 0)
            return true;

        if(!pipeline || !mi_line)
            return false;

        Material *mat = mi_line->GetMaterial();
        if(mat)
            cmd->BindDescriptorSets(mat);

        cmd->BindPipeline(pipeline);

        if(support_wide_lines)
        {
            for(uint i = 0; i < MAX_LINE_WIDTH; ++i)
            {
                if(line_groups[i].GetCount() == 0)
                    continue;

                cmd->SetLineWidth(static_cast<float>(i + 1));
                line_groups[i].Draw(cmd);
            }
        }
        else
        {
            if(line_groups[0].GetCount() > 0)
                line_groups[0].Draw(cmd);
        }

        return true;
    }

    void LineRenderManager::UpsertComponentLines(uint64 component_key, uint8 width, const std::vector<LineSegmentDescriptor>& lines)
    {
        if (width == 0 || width > MAX_LINE_WIDTH)
            return;

        if (lines.empty())
        {
            RemoveComponentLines(component_key);
            return;
        }

        const uint8 new_batch_index = NormalizeBatchIndex(support_wide_lines, width);

        auto it = component_line_map.find(component_key);
        if (it != component_line_map.end())
        {
            const uint8 old_batch_index = NormalizeBatchIndex(support_wide_lines, it->second.width);
            if (old_batch_index != new_batch_index)
                dirty_batch_indices.insert(old_batch_index);

            it->second.width = width;
            it->second.lines = lines;
        }
        else
        {
            ComponentLineBlock block;
            block.width = width;
            block.lines = lines;
            component_line_map.emplace(component_key, std::move(block));
        }

        dirty_batch_indices.insert(new_batch_index);
        component_lines_dirty = true;
    }

    void LineRenderManager::RemoveComponentLines(uint64 component_key)
    {
        auto it = component_line_map.find(component_key);
        if (it == component_line_map.end())
            return;

        const uint8 old_batch_index = NormalizeBatchIndex(support_wide_lines, it->second.width);
        dirty_batch_indices.insert(old_batch_index);

        component_line_map.erase(it);
        component_lines_dirty = true;
    }

    void LineRenderManager::ClearComponentLines()
    {
        if (component_line_map.empty())
            return;

        component_line_map.clear();

        if (support_wide_lines)
        {
            for (uint8 i = 0; i < static_cast<uint8>(MAX_LINE_WIDTH); ++i)
                dirty_batch_indices.insert(i);
        }
        else
        {
            dirty_batch_indices.insert(0);
        }

        component_lines_dirty = true;
    }

    void LineRenderManager::CommitComponentLines()
    {
        if (!component_lines_dirty)
            return;

        if (!support_wide_lines)
        {
            dirty_batch_indices.clear();
            dirty_batch_indices.insert(0);
        }

        for (const uint8 batch_index : dirty_batch_indices)
        {
            line_groups[batch_index].Reset();
        }

        for (const auto& entry : component_line_map)
        {
            const auto& block = entry.second;
            if (block.lines.empty())
                continue;

            const uint8 batch_index = NormalizeBatchIndex(support_wide_lines, block.width);
            if (dirty_batch_indices.find(batch_index) == dirty_batch_indices.end())
                continue;

            line_groups[batch_index].AddLine(block.lines);
        }

        total_line_count = 0;
        for (const auto& entry : component_line_map)
        {
            total_line_count += static_cast<uint32>(entry.second.lines.size());
        }

        dirty_batch_indices.clear();
        component_lines_dirty = false;
    }
} // namespace hgl::graph

