#include "LineRenderManager.h"
#include "LineWidthBatch.h"
#include <cstring>
#include <hgl/graph/VKRenderTarget.h>
#include <hgl/graph/PrimitiveCreater.h>
#include <hgl/graph/VKDevice.h>
#include <hgl/graph/VKVertexInputLayout.h>
#include <hgl/graph/VKPrimitive.h>
#include <hgl/graph/module/MaterialManager.h>
#include <hgl/graph/mtl/Material3DCreateConfig.h>
#include <hgl/graph/VKMaterial.h>
#include <hgl/graph/module/PrimitiveManager.h>
#include <hgl/graph/module/MeshManager.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/graph/RenderFramework.h>
#include "SharedLineBackup.h"

namespace hgl::graph
{
    constexpr const ShaderBufferSource SBS_ColorPattle=
    {
        DescriptorSetType::PerMaterial,

        "color_pattle",
        "ColorPattle",

        "vec4 color[256]"
    };

    constexpr const size_t LINE_COUNT_INCREMENT =       1024;
    constexpr const size_t POSITION_COMPONENT_COUNT =   6;
    constexpr const size_t COLOR_COMPONENT_COUNT =      2;

    constexpr const size_t POSITION_COUNT_INCREMENT = LINE_COUNT_INCREMENT * POSITION_COMPONENT_COUNT;
    constexpr const size_t COLOR_COUNT_INCREMENT    = LINE_COUNT_INCREMENT * COLOR_COMPONENT_COUNT;

    LineRenderManager* CreateLineRenderManager(RenderFramework *rf)
    {
        if (!rf)
            return(nullptr);

        // Create PureColor3D material via factory
        mtl::Material3DCreateConfig cfg(PrimitiveType::Lines,
                                        mtl::WithCamera::With,
                                        mtl::WithLocalToWorld::Without,
                                        mtl::WithSky::Without);

        auto *mci = mtl::CreatePureColor3D(rf->GetDevAttr(), &cfg);

        if (mci == nullptr)
            return nullptr;

        Material *mat = rf->CreateMaterial("M_Line3D", mci);

        if (mat == nullptr)
        {
            delete mci;
            return nullptr;
        }

        MaterialInstance *mi = rf->CreateMaterialInstance(mat);

        if (mi == nullptr)
        {
            delete mat;
            delete mci;
            return nullptr;
        }

        Pipeline *p = rf->CreatePipeline(mi,InlinePipeline::DynamicLineWidth3D);

        UBOLineColorPalette *lcp=rf->CreateUBO<UBOLineColorPalette>(&SBS_ColorPattle);

        LineRenderManager *mgr = new LineRenderManager(rf->GetDevice(),mi,p,lcp);

        return mgr;
    }

    LineRenderManager::LineRenderManager(VulkanDevice *dev,MaterialInstance *mi,Pipeline *p,UBOLineColorPalette *lcp)
    {
        device = dev;
        mi_line = mi;
        pipeline=p;
        ubo_color=lcp;

        // allocate shared backup once for all batches
        shared_backup = new SharedLineBackup();

        for(uint i = 0;i < MAX_LINE_WIDTH;i++)
        {
            // pass shared backup pointer to each LineWidthBatch
            line_groups[i].Init(i + 1,device,mi,pipeline,shared_backup);
        }

        total_line_count = 0;
    }

    LineRenderManager::~LineRenderManager()
    {
        delete ubo_color;
        delete shared_backup;
    }

    void LineRenderManager::SetColor(const int index, const Color4f& c)
    {
        if (index < 0 || index >= 256) return;

        ubo_color->Write(&c,index*sizeof(Color4f),sizeof(Color4f));
    }

    bool LineRenderManager::AddLine(const Vector3f& from, const Vector3f& to, const uint8_t color_index, uint8_t width)
    {
        if (width == 0
            ||width > MAX_LINE_WIDTH)
            return(false);

        line_groups[width-1].AddLine(from,to,color_index);
        ++total_line_count;
        return(true);
    }

    bool LineRenderManager::AddLine(const uint8_t width,const DataArray<LineSegmentDescriptor> &lsi_list)
    {
        if(width==0
            ||width>MAX_LINE_WIDTH)
            return(false);

        if(lsi_list.IsEmpty())
            return(false);

        line_groups[width-1].AddLine(lsi_list);
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

        mi_line->GetMaterial()->BindUBO(&SBS_ColorPattle,ubo_color->ubo());

        cmd->BindPipeline(pipeline);
        cmd->BindDescriptorSets(mi_line->GetMaterial());

        for(size_t i = 0;i < MAX_LINE_WIDTH;i++)
        {
            if(line_groups[i].GetCount() <= 0)
                continue;

            cmd->SetLineWidth(i + 1);

            line_groups[i].Draw(cmd);
        }

        return(true);
    }
} // namespace

