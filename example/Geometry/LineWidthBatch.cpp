#include "LineWidthBatch.h"
#include <hgl/graph/VKDevice.h>
#include <hgl/graph/PrimitiveCreater.h>
#include <hgl/graph/module/MeshManager.h>
#include <hgl/graph/VKMaterial.h>
#include <vector>
#include "SharedLineBackup.h"

using namespace hgl;
using namespace hgl::graph;

constexpr const size_t LINE_COUNT_INCREMENT =       1024;
constexpr const size_t POSITION_COMPONENT_COUNT =   6;
constexpr const size_t COLOR_COMPONENT_COUNT =      2;

LineWidthBatch::~LineWidthBatch()
{
    Clear();
}

void LineWidthBatch::Init(const uint w,VulkanDevice *dev,MaterialInstance *mi,Pipeline *p,SharedLineBackup *sb)
{
    device = dev;
    mtl_inst = mi;

    pipeline = p;

    line_width = w;

    max_count = 0;
    count = 0;

    shared_backup = sb;
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

    AnsiString name = "Line3D(Width:" + AnsiString::numberOf(line_width) + ")";

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

void LineWidthBatch::Expand(uint c)
{
    if(c<=0)return;

    const uint32_t old_count = count;

    count+=c;

    if(count > max_count)
    {
        // Shared backup must exist; use it exclusively
        SharedLineBackup *backup = shared_backup;

        const uint32_t vertex_count = old_count * 2;

        if(!backup)
        {
            // Fallback to previous behavior if no shared backup provided
            // (should not happen per new requirement)
            return;
        }

        if(position && color && vertex_count>0)
        {
            backup->EnsureCapacity(vertex_count);
            backup->positions.clear();
            backup->colors.clear();

            for(uint32_t i=0;i<vertex_count;i++)
            {
                float *p = position->Get(i);
                if(p)
                    backup->positions.emplace_back(p[0],p[1],p[2]);
                else
                    backup->positions.emplace_back(Vector3f(0,0,0));

                uint8_t *cp = color->Get(i);
                if(cp)
                    backup->colors.push_back(*cp);
                else
                    backup->colors.push_back(0);
            }
        }

        max_count+=LINE_COUNT_INCREMENT;

        // Recreate buffers
        Clear();
        if(!RebuildMesh())
        {
            // failed to rebuild, restore count
            count = old_count;
            return;
        }

        // Restore backed up data into new buffers
        if(!backup->Empty() && position && color)
        {
            for(size_t i=0;i<backup->positions.size();++i)
            {
                position->Write(backup->positions[i]);
                color->Write(backup->colors[i]);
            }

            if(mesh)
                mesh->SetDrawCounts(old_count*2);

            // Clear contents but do not reduce capacity
            backup->positions.clear();
            backup->colors.clear();
        }
    }
}

void LineWidthBatch::AddLine(const Vector3f &from,const Vector3f &to,uint8_t color_index)
{
    Expand(1);

    if(!position)
        return;

    position->Write(from);
    position->Write(to);

    color->Write(color_index);
    color->Write(color_index);

    mesh->SetDrawCounts(count*2);
}

void LineWidthBatch::AddLine(const DataArray<LineSegmentDescriptor> &lsi_list)
{
    Expand(lsi_list.GetCount());

    if(!position)
        return;

    for(auto &lsi:lsi_list)
    {
        position->Write(lsi.from);
        position->Write(lsi.to);

        color->Write(lsi.color);
        color->Write(lsi.color);
    }

    mesh->SetDrawCounts(count*2);
}

void LineWidthBatch::Draw(RenderCmdBuffer *cmd)
{
    if(!mesh)
        return;

    cmd->BindDataBuffer(mesh->GetDataBuffer());

    cmd->Draw(mesh->GetDataBuffer(),mesh->GetRenderData());
}
