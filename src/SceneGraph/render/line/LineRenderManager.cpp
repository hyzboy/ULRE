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

namespace hgl::graph
{
    constexpr const size_t LINE_COUNT_INCREMENT =       1024;
    constexpr const size_t POSITION_COMPONENT_COUNT =   6;
    constexpr const size_t COLOR_COMPONENT_COUNT =      2;

    constexpr const size_t POSITION_COUNT_INCREMENT = LINE_COUNT_INCREMENT * POSITION_COMPONENT_COUNT;
    constexpr const size_t COLOR_COUNT_INCREMENT    = LINE_COUNT_INCREMENT * COLOR_COMPONENT_COUNT;

    LineRenderManager* CreateLineRenderManager(RenderFramework *rf,IRenderTarget *rt)
    {
        if (!rf)
            return(nullptr);

        // Create PureColor3D material via factory
        mtl::Material3DCreateConfig cfg(PrimitiveType::Lines,
                                        mtl::WithCamera::With,
                                        mtl::WithLocalToWorld::Without,
                                        mtl::WithSky::Without);

        auto *mci = mtl::CreateVertexPattleColor3D(rf->GetDevAttr(), &cfg);

        if (mci == nullptr)
            return nullptr;

        Material *mat = rf->CreateMaterial("M_Line3D", mci);

        if (mat == nullptr)
        {
            delete mci;
            return nullptr;
        }

        VILConfig vil_config;

        vil_config.Add(VAN::Color,VF_V1U8);

        MaterialInstance *mi = rf->CreateMaterialInstance(mat,&vil_config);

        if (mi == nullptr)
        {
            delete mat;
            delete mci;
            return nullptr;
        }

        RenderPass *rp = rt->GetRenderPass();

        VulkanDevAttr *dev_attr = rf->GetDevAttr();

        Pipeline *p;

        if(dev_attr->wide_lines)
            p = rp->CreatePipeline(mi,InlinePipeline::DynamicLineWidth3D);
        else
            p = rp->CreatePipeline(mi,InlinePipeline::Solid3D);

        UBOLineColorPalette *lcp=rf->CreateUBO<UBOLineColorPalette>(&mtl::SBS_ColorPattle);

        LineRenderManager *mgr = new LineRenderManager(rf->GetDevice(),mi,p,lcp);

        return mgr;
    }

    LineRenderManager::LineRenderManager(VulkanDevice *dev,MaterialInstance *mi,Pipeline *p,UBOLineColorPalette *lcp)
    {
        support_wide_lines = dev->GetDevAttr()->wide_lines;

        device = dev;
        mi_line = mi;
        pipeline=p;
        ubo_color=lcp;

        // allocate shared backup once for all batches
        shared_backup = new SharedLineBackup();

        if(support_wide_lines)
        {
            for(uint i = 0;i < MAX_LINE_WIDTH;i++)
            {
                // pass shared backup pointer to each LineWidthBatch
                line_groups[i].Init(i + 1,device,mi,pipeline,shared_backup);
            }
        }
        else
        {
            line_groups[0].Init(0,device,mi,pipeline,shared_backup);
        }

        total_line_count = 0;
    }

    LineRenderManager::~LineRenderManager()
    {
        delete shared_backup;
    }

    void LineRenderManager::SetColor(const int index, const Color4f& c)
    {
        if (index < 0 || index >= 256) return;

        ubo_color->Write(&c,index*sizeof(Color4f),sizeof(Color4f));
    }

    bool LineRenderManager::AddLine(const Vector3f& from, const Vector3f& to, const uint8_t color_index, uint8 width)
    {
        if (width == 0
            ||width > MAX_LINE_WIDTH)
            return(false);

        if(support_wide_lines)
            line_groups[width-1].AddLine(from,to,color_index);
        else
            line_groups[0].AddLine(from,to,color_index);

        ++total_line_count;
        return(true);
    }

    bool LineRenderManager::AddLine(const uint8 width,const DataArray<LineSegmentDescriptor> &lsi_list)
    {
        if(width==0
            ||width>MAX_LINE_WIDTH)
            return(false);

        if(lsi_list.IsEmpty())
            return(false);

        if(support_wide_lines)
            line_groups[width-1].AddLine(lsi_list);
        else
            line_groups[0].AddLine(lsi_list);

        total_line_count += lsi_list.GetCount();
        return(true);
    }

    void LineRenderManager::ClearLines()
    {
        for(auto &lsb : line_groups)
            lsb.SetCount(0);

        total_line_count = 0;
    }

    bool LineRenderManager::Draw(RenderCmdBuffer *cmd)
    {
        if (!cmd)
            return(false);

        if(total_line_count==0)
            return(true);

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

