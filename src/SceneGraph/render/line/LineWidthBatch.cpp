#include <hgl/graph/geo/line/LineWidthBatch.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/graph/geo/GeometryCreater.h>
#include <hgl/graph/module/PrimitiveManager.h>
#include <hgl/vk/VKMaterial.h>

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
    // BufferAccessor会在析构时自动Unmap
    // 所以这里不需要手动清理，但需要解绑
    position.Bind(nullptr);
    color.Bind(nullptr);

    SAFE_CLEAR(primitive);
    SAFE_CLEAR(geometry);
}

bool LineWidthBatch::RebuildMesh()
{
    Clear();

    AnsiString name = "Line3D(Width:" + AnsiString::numberOf(line_width) + ")";

    geometry = CreateGeometry(device,mtl_inst->GetVIL(),name,max_count * 2);

    if(!geometry)
        return(false);

    primitive=DirectCreatePrimitive(geometry,mtl_inst,pipeline);

    // 直接绑定到VAB，自动Map
    position.Bind(geometry->GetVAB(geometry->GetVABIndex(VAN::Position)));
    color.Bind(geometry->GetVAB(geometry->GetVABIndex(VAN::Color)));

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

        if(position.IsValid() && color.IsValid() && vertex_count>0)
        {
            // Ensure shared backup has enough capacity (only grows)
            backup->EnsureCapacity(vertex_count);

            // 使用BufferAccessor的ReadBulk方法
            bool pos_ok = position.ReadBulk(backup->positions.data(), vertex_count);
            if(!pos_ok)
            {
                // cannot safely backup, abort expansion
                count = old_count;
                return;
            }

            bool col_ok = color.ReadBulk(backup->colors.data(), vertex_count);
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
        if(!backup->IsEmpty() && position.IsValid() && color.IsValid())
        {
            // 使用BufferAccessor的WriteBulk方法，自动标记dirty
            bool pos_write_ok = position.WriteBulk(backup->positions.data(), static_cast<uint32_t>(backup->positions.size()));
            if(!pos_write_ok)
            {
                // cannot restore safely, abort
                count = old_count;
                return;
            }

            bool col_write_ok = color.WriteBulk(backup->colors.data(), static_cast<uint32_t>(backup->colors.size()));
            if(!col_write_ok)
            {
                // cannot restore safely, abort
                count = old_count;
                return;
            }

            if(primitive)
                primitive->SetDrawCounts(old_count*2);

            // Move access pointers to the end of existing data so subsequent writes append
            const uint32_t vertex_end = old_count * 2;
            position.Seek(vertex_end);
            color.Seek(vertex_end);

            // Clear contents but do not reduce capacity
            backup->Clear();
        }
    }
}

void LineWidthBatch::AddLine(const Vector3f &from,const Vector3f &to,uint8 color_index)
{
    Expand(1);

    if(!position.IsValid())
        return;

    // Write方法会自动标记dirty
    position.Write(from);
    position.Write(to);

    color.Write(color_index);
    color.Write(color_index);

    primitive->SetDrawCounts(count*2);
}

void LineWidthBatch::AddLine(const std::vector<LineSegmentDescriptor> &lsi_list)
{
    Expand(lsi_list.size());

    if(!position.IsValid())
        return;

    for(auto &lsi:lsi_list)
    {
        // Write方法会自动标记dirty
        position.Write(lsi.from);
        position.Write(lsi.to);

        color.Write(lsi.color);
        color.Write(lsi.color);
    }

    primitive->SetDrawCounts(count*2);
}

void LineWidthBatch::Draw(RenderCmdBuffer *cmd)
{
    if(!primitive)
        return;

    // Commit会自动检查dirty状态，只在需要时才Unmap/Remap
    position.Commit();
    color.Commit();

    cmd->BindDataBuffer(primitive->GetDataBuffer());

    cmd->Draw(primitive->GetDataBuffer(),primitive->GetRenderData());
}

void LineWidthBatch::UpdatePipeline(Pipeline *p)
{
    pipeline = p;
    if(primitive)
        primitive->UpdatePipeline(p);
}
