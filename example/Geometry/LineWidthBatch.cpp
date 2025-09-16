#include "LineWidthBatch.h"
#include <hgl/graph/VKDevice.h>
#include <hgl/graph/PrimitiveCreater.h>
#include <hgl/graph/module/MeshManager.h>
#include <hgl/graph/VKMaterial.h>

using namespace hgl;
using namespace hgl::graph;

constexpr const size_t LINE_COUNT_INCREMENT =       1024;
constexpr const size_t POSITION_COMPONENT_COUNT =   6;
constexpr const size_t COLOR_COMPONENT_COUNT =      2;

LineWidthBatch::~LineWidthBatch()
{
    Clear();
}

void LineWidthBatch::Init(const uint w,VulkanDevice *dev,MaterialInstance *mi,Pipeline *p)
{
    device = dev;
    mtl_inst = mi;

    pipeline = p;

    width = w;

    max_count = 0;
    count = 0;
}

void LineWidthBatch::Clear()
{
    SAFE_CLEAR(vab_position)
    SAFE_CLEAR(vab_color)
    SAFE_CLEAR(mesh);
    SAFE_CLEAR(primitive);
}

bool LineWidthBatch::RebuildMesh()
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

void LineWidthBatch::AddCount(uint c)
{
    if(c<=0)return;

    count+=c;

    if(count >= max_count)
    {
        max_count+=LINE_COUNT_INCREMENT;

        Clear();
        RebuildMesh();
    }
}

void LineWidthBatch::AddLine(const Vector3f &from,const Vector3f &to,uint8_t color_index)
{
    AddCount(1);

    if(!position)
        return;

    position->Write(from);
    position->Write(to);

    color->Write(color_index);
    color->Write(color_index);

    mesh->SetDrawCounts(count);
}

void LineWidthBatch::AddLine(const DataArray<LineSegmentDescriptor> &lsi_list)
{
    AddCount(lsi_list.GetCount());

    if(!position)
        return;

    for(auto &lsi:lsi_list)
    {
        position->Write(lsi.from);
        position->Write(lsi.to);

        color->Write(lsi.color);
        color->Write(lsi.color);
    }

    mesh->SetDrawCounts(count);
}

void LineWidthBatch::Draw(RenderCmdBuffer *cmd)
{
    if(!mesh)
        return;

    cmd->BindDataBuffer(mesh->GetDataBuffer());

    cmd->Draw(mesh->GetDataBuffer(),mesh->GetRenderData());
}
