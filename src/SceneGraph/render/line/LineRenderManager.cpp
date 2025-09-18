#include <hgl/graph/geo/line/LineRenderManager.h>
#include <hgl/graph/VKRenderTarget.h>
#include <hgl/graph/PrimitiveCreater.h>
#include <hgl/graph/VKDevice.h>
#include <hgl/graph/VKVertexInputLayout.h>
#include <hgl/graph/VKPrimitive.h>
#include <hgl/graph/module/MaterialManager.h>
#include <hgl/graph/mtl/Material3DCreateConfig.h>
#include <hgl/graph/VKMaterial.h>
#include <hgl/graph/VKVertexInputConfig.h>
#include <hgl/graph/module/PrimitiveManager.h>
#include <hgl/graph/module/MeshManager.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/graph/RenderFramework.h>
#include <hgl/graph/mtl/UBOCommon.h>

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
    LineRenderManager* CreateLineRenderManager(RenderFramework *rf,IRenderTarget *rt)
    {
        if (!rf)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: CreateLineRenderManager失败 RenderFramework为空 EN: rf is null"));
            return(nullptr);
        }

        // Create PureColor3D material via factory
        mtl::Material3DCreateConfig cfg(PrimitiveType::Lines,
                                        mtl::WithCamera::With,
                                        mtl::WithLocalToWorld::Without,
                                        mtl::WithSky::Without);

        auto *mci = mtl::CreateVertexPattleColor3D(rf->GetDevAttr(), &cfg);

        if (mci == nullptr)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: 创建MaterialCreateInfo失败 EN: failed to create material create info"));
            return nullptr;
        }

        Material *mat = rf->CreateMaterial("M_Line3D", mci);

        if (mat == nullptr)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: 创建Material失败 EN: failed to create material"));
            delete mci;
            return nullptr;
        }

        VILConfig vil_config;
        vil_config.Add(VAN::Color,VF_V1U8);

        MaterialInstance *mi = rf->CreateMaterialInstance(mat,&vil_config);

        if (mi == nullptr)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: 创建MaterialInstance失败 EN: failed to create material instance"));
            delete mat;
            delete mci;
            return nullptr;
        }

        if(!rt)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: 渲染目标为空 EN: render target is null"));
            delete mi; delete mat; delete mci;
            return nullptr;
        }

        RenderPass *rp = rt->GetRenderPass();
        if(!rp)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: 获取RenderPass失败 EN: failed to get render pass"));
            delete mi; delete mat; delete mci;
            return nullptr;
        }

        VulkanDevAttr *dev_attr = rf->GetDevAttr();
        if(!dev_attr)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: VulkanDevAttr为空 EN: VulkanDevAttr is null"));
            delete mi; delete mat; delete mci;
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
            delete mi; delete mat; delete mci;
            return nullptr;
        }

        UBOLineColorPalette *lcp = rf->CreateUBO<UBOLineColorPalette>(&mtl::SBS_ColorPattle);
        if(!lcp)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: 创建颜色调色板UBO失败 EN: failed to create palette UBO"));
            delete mi; delete mat; delete mci;
            return nullptr;
        }

        LineRenderManager *mgr = new LineRenderManager(rf->GetDevice(),mi,p,lcp);
        if(!mgr)
        {
            MLogError(LineRenderManager,OS_TEXT("CN: 分配LineRenderManager失败 EN: allocation of LineRenderManager failed"));
            delete lcp; delete mi; delete mat; delete mci;
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
    LineRenderManager::LineRenderManager(VulkanDevice *dev,MaterialInstance *mi,Pipeline *p,UBOLineColorPalette *lcp)
    {
        support_wide_lines = dev && dev->GetDevAttr() ? dev->GetDevAttr()->wide_lines : false;

        device = dev;
        mi_line = mi;
        pipeline=p;
        ubo_color=lcp;

        shared_backup = new SharedLineBackup();
        if(!shared_backup)
            LogError(OS_TEXT("CN: 分配SharedLineBackup失败 EN: failed to allocate SharedLineBackup"));

        if(support_wide_lines)
        {
            for(uint i = 0;i < MAX_LINE_WIDTH;i++)
            {
                line_groups[i].Init(i + 1,device,mi,pipeline,shared_backup);
            }
            LogInfo(OS_TEXT("CN: 初始化宽线批次数量=") + OSString::numberOf(MAX_LINE_WIDTH) + OS_TEXT(" EN: initialized wide line batches"));
        }
        else
        {
            line_groups[0].Init(0,device,mi,pipeline,shared_backup);
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
        delete shared_backup;
        shared_backup = nullptr;
    }

    /**
     * CN: 写入调色板颜色; 索引越界将记录警告并返回。
     * EN: Update palette color; out-of-range index logs warning and returns.
     */
    void LineRenderManager::SetColor(const int index, const Color4f& c)
    {
        if (index < 0 || index >= 256)
        {
            LogWarning(OS_TEXT("CN: SetColor索引越界 index=") + OSString::numberOf(index) + OS_TEXT(" EN: palette index out of range"));
            return;
        }

        ubo_color->Write(&c,index*sizeof(Color4f),sizeof(Color4f));
    }

    /**
     * CN: 添加单条线段; 宽度非法返回false并记录日志。
     * EN: Add single line; invalid width returns false and logs.
     */
    bool LineRenderManager::AddLine(const Vector3f& from, const Vector3f& to, const uint8_t color_index, uint8 width)
    {
        if (width == 0 || width > MAX_LINE_WIDTH)
        {
            LogError(OS_TEXT("CN: AddLine失败 非法宽度=") + OSString::numberOf(width) + OS_TEXT(" EN: invalid width"));
            return(false);
        }

        if(support_wide_lines)
            line_groups[width-1].AddLine(from,to,color_index);
        else
            line_groups[0].AddLine(from,to,color_index);

        ++total_line_count;
        return(true);
    }

    /**
     * CN: 批量添加线段; 宽度非法或列表为空返回false并记录日志。
     * EN: Bulk add lines; invalid width or empty list returns false and logs.
     */
    bool LineRenderManager::AddLine(const uint8 width,const DataArray<LineSegmentDescriptor> &lsi_list)
    {
        if(width==0 || width>MAX_LINE_WIDTH)
        {
            LogError(OS_TEXT("CN: 批量AddLine失败 非法宽度=") + OSString::numberOf(width) + OS_TEXT(" EN: invalid width"));
            return(false);
        }

        if(lsi_list.IsEmpty())
        {
            LogWarning(OS_TEXT("CN: 批量AddLine失败 列表为空 EN: line list empty"));
            return(false);
        }

        if(support_wide_lines)
            line_groups[width-1].AddLine(lsi_list);
        else
            line_groups[0].AddLine(lsi_list);

        total_line_count += lsi_list.GetCount();
        return(true);
    }

    /**
     * CN: 清空全部批次线段与计数。
     * EN: Clear all batches and reset counter.
     */
    void LineRenderManager::ClearLines()
    {
        for(auto &lsb : line_groups)
            lsb.SetCount(0);

        total_line_count = 0;
    }

    /**
     * CN: 绘制全部线段; 若命令缓冲为空或没有线段时记录日志并返回。
     * EN: Draw all lines; logs and returns when command buffer null or no lines.
     */
    bool LineRenderManager::Draw(RenderCmdBuffer *cmd)
    {
        if (!cmd)
        {
            LogError(OS_TEXT("CN: Draw失败 命令缓冲为空 EN: command buffer is null"));
            return(false);
        }

        if(total_line_count==0)
        {
            LogVerbose(OS_TEXT("CN: Draw跳过 没有线段 EN: draw skipped no lines"));
            return(true);
        }

        mi_line->GetMaterial()->BindUBO(&mtl::SBS_ColorPattle,ubo_color->ubo());

        cmd->BindPipeline(pipeline);
        cmd->BindDescriptorSets(mi_line->GetMaterial());

        if(support_wide_lines)
        {
            for(size_t i = 0;i < MAX_LINE_WIDTH;i++)
            {
                if(line_groups[i].GetCount() <= 0)
                    continue;

                cmd->SetLineWidth(i + 1);
                line_groups[i].Draw(cmd);
            }
        }
        else
        {
            line_groups[0].Draw(cmd);
        }

        return(true);
    }
} // namespace

