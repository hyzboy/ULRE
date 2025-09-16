#include "DrawLineManager.h"
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

    DrawLineManager* CreateDrawLineManager(RenderFramework *rf)
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

        UBOLineColorPattle *lcp=rf->CreateUBO<UBOLineColorPattle>(&SBS_ColorPattle);

        DrawLineManager *mgr = new DrawLineManager(mi,p,lcp,rf->GetMeshManager());

        return mgr;
    }

    void DrawLineManager::LineSegmentBuffer::Init(const uint w,hgl::graph::VulkanDevice *dev,MaterialInstance *mi,Pipeline *p)
    {
        device = dev;
        mtl_inst = mi;

        pipeline = p;

        width = w;

        max_count = 0;
        count = 0;
    }

    void DrawLineManager::LineSegmentBuffer::Clear()
    {
        SAFE_CLEAR(vab_position)
        SAFE_CLEAR(vab_color)
        SAFE_CLEAR(mesh);
        SAFE_CLEAR(primitive);
    }

    DrawLineManager::LineSegmentBuffer::~LineSegmentBuffer()
    {
        Clear();
    }

    bool DrawLineManager::LineSegmentBuffer::RebuildMesh()
    {
        Clear();

        AnsiString name = "Line3D(Width:" + AnsiString::numberOf(width) + ")";

        primitive = CreatePrimitive(device,mtl_inst->GetVIL(),name,max_count * 2);

        if(!primitive)
            return(false);

        mesh=DirectCreateMesh(primitive,mtl_inst,pipeline);

        vab_position=new VABMap3f(primitive->GetVABMap(VAN::Position));
        vab_color=new VABMap1u8(primitive->GetVABMap(VAN::Color));

        position=vab_position->Map();
        color=vab_color->Map();

        return(true);
    }

    void DrawLineManager::LineSegmentBuffer::AddCount(uint c)
    {
        if(count<=0)return;

        count+=c;

        if(count >= max_count)
        {
            max_count+=LINE_COUNT_INCREMENT;

            Clear();
            RebuildMesh();
        }
    }

    void DrawLineManager::LineSegmentBuffer::AddLine(const Vector3f &from,const Vector3f &to,uint8_t color_index)
    {
        AddCount(1);

        position->Write(from);
        position->Write(to);

        color->Write(color_index);
        color->Write(color_index);

        mesh->SetDrawCounts(count);

        dirty = true;
    }

    void DrawLineManager::LineSegmentBuffer::AddLine(const DataArray<LineSegmentInfo> &lsi_list)
    {
        AddCount(lsi_list.GetCount());

        for(auto &lsi:lsi_list)
        {
            position->Write(lsi.from);
            position->Write(lsi.to);

            color->Write(lsi.color);
            color->Write(lsi.color);
        }

        mesh->SetDrawCounts(count);            
    }

    uint32_t DrawLineManager::LineSegmentBuffer::Update()
    {
        if(count<=0||!dirty)
            return(count);

        RebuildMesh();
        return count;
    }

    void DrawLineManager::LineSegmentBuffer::Draw(RenderCmdBuffer *cmd)
    {
        cmd->BindDataBuffer(mesh->GetDataBuffer());

        cmd->Draw(mesh->GetDataBuffer(),mesh->GetRenderData());
    }

    DrawLineManager::DrawLineManager(MaterialInstance *mi,Pipeline *p,UBOLineColorPattle *lcp,MeshManager *mm)
    {
        mi_line3d = mi;
        pipeline=p;
        ubo_color=lcp;
        mesh_manager = mm;

        for(uint i = 0;i < MAX_LINE_WIDTH;i++)
        {
            line_groups[i].Init(i + 1,mm->GetDevice(),mi,pipeline);
        }
    }

    DrawLineManager::~DrawLineManager()
    {
        delete ubo_color;
    }

    void DrawLineManager::SetColor(const int index, const Color4f& c)
    {
        if (index < 0 || index >= 256) return;

        ubo_color->Write(&c,index*sizeof(Color4f),sizeof(Color4f));
    }

    bool DrawLineManager::AddLine(const Vector3f& from, const Vector3f& to, const uint8_t color_index, uint8_t width)
    {
        if (width == 0
            ||width > MAX_LINE_WIDTH)
            return(false);

        line_groups[width-1].AddLine(from,to,color_index);

        return(true);
    }

    bool DrawLineManager::AddLine(const uint8_t width,const DataArray<LineSegmentInfo> &lsi_list)
    {
        if(width==0
            ||width>MAX_LINE_WIDTH)
            return(false);

        if(lsi_list.IsEmpty())
            return(false);

        line_groups[width-1].AddLine(lsi_list);
        return(true);
    }

    void DrawLineManager::ClearLines()
    {
        for(auto &lsb : line_groups)
            lsb.count = 0;
    }

    void DrawLineManager::Update()
    {
        total_line_count=0;

        for(auto &lsb:line_groups)
        {
            total_line_count+=lsb.Update();
        }
    }

    bool DrawLineManager::Draw(RenderCmdBuffer *cmd)
    {
        if (!cmd)
            return(false);

        if(total_line_count==0)
            return(true);

        mi_line3d->GetMaterial()->BindUBO(&SBS_ColorPattle,ubo_color->ubo());

        cmd->BindPipeline(pipeline);
        cmd->BindDescriptorSets(mi_line3d->GetMaterial());

        for(size_t i = 0;i < MAX_LINE_WIDTH;i++)
        {
            if(line_groups[i].count <= 0)
                continue;

            cmd->SetLineWidth(i + 1);

            line_groups[i].Draw(cmd);
        }

        return(true);
    }
} // namespace

