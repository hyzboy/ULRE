#include<hgl/graph/geo/GeometryBuilder.h>
#include<hgl/graph/geo/VKGeometry.h>

namespace hgl::graph::inline_geometry
{
    GeometryBuilder::GeometryBuilder(GeometryCreater *pc)
        : creater(pc)
    {
        if(!pc)
            return;

        // 绑定 BufferAccessor 到 VAB
        VertexAttribBuffer *vab;
        const int32_t vertex_offset = pc->GetVertexOffset();
        const uint32_t vertex_count = pc->GetVertexCount();

        vab = pc->GetVAB(VAN::Position);
        if(vab)
            accessor_position.Bind(vab, vertex_offset, vertex_count);

        vab = pc->GetVAB(VAN::Normal);
        if(vab)
        {
            // 压缩格式分派：RG16F（R16G16_SFLOAT）→ 2 分量半浮点访问器（xy，z 重建）
            if(vab->GetFormat() == VK_FORMAT_R16G16_SFLOAT)
                accessor_normal_2hf.Bind(vab, vertex_offset, vertex_count);
            else
                accessor_normal.Bind(vab, vertex_offset, vertex_count);
        }

        vab = pc->GetVAB(VAN::Tangent);
        if(vab)
            accessor_tangent.Bind(vab, vertex_offset, vertex_count);

        vab = pc->GetVAB(VAN::TexCoord);
        if(vab)
            accessor_texcoord.Bind(vab, vertex_offset, vertex_count);
    }

    GeometryBuilder::~GeometryBuilder()
    {
        // BufferAccessor 自动管理生命周期，无需手动清理
    }
}
