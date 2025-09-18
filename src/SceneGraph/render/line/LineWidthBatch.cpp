#include <hgl/graph/geo/line/LineWidthBatch.h>
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
            // Fallback: abort expansion
            count = old_count;
            return;
        }

        if(vab_position && vab_color && vertex_count>0)
        {
            // Ensure shared backup has enough capacity (only grows)
            backup->EnsureCapacity(vertex_count);

            // Bulk read using VABFormatMap::Read; must succeed
            bool pos_ok = vab_position->Read(backup->positions.GetData(), vertex_count);
            if(!pos_ok)
            {
                // cannot safely backup, abort expansion
                count = old_count;
                return;
            }

            bool col_ok = vab_color->Read(backup->colors.GetData(), vertex_count);
            if(!col_ok)
            {
                // cannot safely backup, abort expansion
                count = old_count;
                return;
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

        // Restore backed up data into new buffers (bulk write)
        if(!backup->IsEmpty() && vab_position && vab_color)
        {
            // Bulk write using VABFormatMap::Write; must succeed
            bool pos_write_ok = vab_position->Write(backup->positions.GetData(), static_cast<uint32_t>(backup->positions.GetCount()));
            if(!pos_write_ok)
            {
                // cannot restore safely, abort
                count = old_count;
                return;
            }

            bool col_write_ok = vab_color->Write(backup->colors.GetData(), static_cast<uint32_t>(backup->colors.GetCount()));
            if(!col_write_ok)
            {
                // cannot restore safely, abort
                count = old_count;
                return;
            }

            if(mesh)
                mesh->SetDrawCounts(old_count*2);

            // Move access pointers to the end of existing data so subsequent writes append
            // old_count lines => old_count*2 vertices
            const uint32_t vertex_end = old_count * 2;

            if(position)
            {
                // position is VB3f* (VertexAttribDataAccess3)
                position->Seek(vertex_end);
            }

            if(color)
            {
                // color is VB1u8* (VertexAttribDataAccess1)
                color->Seek(vertex_end);
            }

            // Clear contents but do not reduce capacity
            backup->Clear();
        }
    }
}

void LineWidthBatch::AddLine(const Vector3f &from,const Vector3f &to,uint8 color_index)
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

void LineWidthBatch::UpdatePipeline(Pipeline *p)
{
    pipeline = p;
    if(mesh)
        mesh->UpdatePipeline(p);
}
