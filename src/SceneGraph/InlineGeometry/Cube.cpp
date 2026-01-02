#include "InlineGeometryCommon.h"

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;

    Geometry *CreateCube(GeometryCreater *pc, const CubeCreateInfo *cci)
    {
        // 1. 参数验证
        if(!pc || !cci)
            return nullptr;

        // 2. 常量定义
        constexpr uint VERTEX_COUNT = 24;
        constexpr uint INDEX_COUNT = 36;

        // 3. 基本验证
        if(!GeometryValidator::ValidateBasicParams(pc, VERTEX_COUNT, INDEX_COUNT))
            return nullptr;

        // 4. 初始化
        if(!pc->Init("Cube", VERTEX_COUNT, INDEX_COUNT, IndexType::U16))
            return nullptr;

        // 5. 预定义顶点数据
        constexpr float positions[] = {
            -0.5f, -0.5f, -0.5f,    -0.5f, -0.5f, +0.5f,    +0.5f, -0.5f, +0.5f,    +0.5f, -0.5f, -0.5f,
            -0.5f, +0.5f, -0.5f,    -0.5f, +0.5f, +0.5f,    +0.5f, +0.5f, +0.5f,    +0.5f, +0.5f, -0.5f,
            -0.5f, -0.5f, -0.5f,    -0.5f, +0.5f, -0.5f,    +0.5f, +0.5f, -0.5f,    +0.5f, -0.5f, -0.5f,
            -0.5f, -0.5f, +0.5f,    -0.5f, +0.5f, +0.5f,    +0.5f, +0.5f, +0.5f,    +0.5f, -0.5f, +0.5f,
            -0.5f, -0.5f, -0.5f,    -0.5f, -0.5f, +0.5f,    -0.5f, +0.5f, +0.5f,    -0.5f, +0.5f, -0.5f,
            +0.5f, -0.5f, -0.5f,    +0.5f, -0.5f, +0.5f,    +0.5f, +0.5f, +0.5f,    +0.5f, +0.5f, -0.5f
        };

        constexpr float normals[] = {
            +0.0f, +1.0f, +0.0f,    +0.0f, +1.0f, +0.0f,    +0.0f, +1.0f, +0.0f,    +0.0f, +1.0f, +0.0f,
            +0.0f, -1.0f, +0.0f,    +0.0f, -1.0f, +0.0f,    +0.0f, -1.0f, +0.0f,    +0.0f, -1.0f, +0.0f,
            +0.0f, -0.0f, -1.0f,    +0.0f, -0.0f, -1.0f,    +0.0f, -0.0f, -1.0f,    +0.0f, -0.0f, -1.0f,
            +0.0f, -0.0f, +1.0f,    +0.0f, -0.0f, +1.0f,    +0.0f, -0.0f, +1.0f,    +0.0f, -0.0f, +1.0f,
            -1.0f, -0.0f, +0.0f,    -1.0f, -0.0f, +0.0f,    -1.0f, -0.0f, +0.0f,    -1.0f, -0.0f, +0.0f,
            +1.0f, -0.0f, +0.0f,    +1.0f, -0.0f, +0.0f,    +1.0f, -0.0f, +0.0f,    +1.0f, -0.0f, +0.0f
        };

        constexpr float tangents[] = {
            +1.0f, 0.0f, 0.0f,      +1.0f, 0.0f, 0.0f,      +1.0f, 0.0f, 0.0f,      +1.0f, 0.0f, 0.0f,
            +1.0f, 0.0f, 0.0f,      +1.0f, 0.0f, 0.0f,      +1.0f, 0.0f, 0.0f,      +1.0f, 0.0f, 0.0f,
            -1.0f, 0.0f, 0.0f,      -1.0f, 0.0f, 0.0f,      -1.0f, 0.0f, 0.0f,      -1.0f, 0.0f, 0.0f,
            +1.0f, 0.0f, 0.0f,      +1.0f, 0.0f, 0.0f,      +1.0f, 0.0f, 0.0f,      +1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, +1.0f,      0.0f, 0.0f, +1.0f,      0.0f, 0.0f, +1.0f,      0.0f, 0.0f, +1.0f,
            0.0f, 0.0f, -1.0f,      0.0f, 0.0f, -1.0f,      0.0f, 0.0f, -1.0f,      0.0f, 0.0f, -1.0f
        };

        constexpr float tex_coords[] = {
            0.0f, 0.0f,     0.0f, 1.0f,     1.0f, 1.0f,     1.0f, 0.0f,
            0.0f, 1.0f,     0.0f, 0.0f,     1.0f, 0.0f,     1.0f, 1.0f,
            1.0f, 0.0f,     1.0f, 1.0f,     0.0f, 1.0f,     0.0f, 0.0f,
            0.0f, 0.0f,     0.0f, 1.0f,     1.0f, 1.0f,     1.0f, 0.0f,
            0.0f, 0.0f,     1.0f, 0.0f,     1.0f, 1.0f,     0.0f, 1.0f,
            1.0f, 0.0f,     0.0f, 0.0f,     0.0f, 1.0f,     1.0f, 1.0f
        };

        constexpr uint16 indices[] = {
            0,  2,  1,      0,  3,  2,
            4,  5,  6,      4,  6,  7,
            8,  9,  10,     8,  10, 11,
            12, 15, 14,     12, 14, 13,
            16, 17, 18,     16, 18, 19,
            20, 23, 22,     20, 22, 21
        };

        // 6. 写入顶点属性
        if(!pc->WriteVAB(VAN::Position, VF_V3F, positions))
            return nullptr;

        if(cci->normal)
            if(!pc->WriteVAB(VAN::Normal, VF_V3F, normals))
                return nullptr;

        if(cci->tangent)
            if(!pc->WriteVAB(VAN::Tangent, VF_V3F, tangents))
                return nullptr;

        if(cci->tex_coord)
            if(!pc->WriteVAB(VAN::TexCoord, VF_V2F, tex_coords))
                return nullptr;

        // 7. 处理颜色（如果需要）
        if(cci->color_type != CubeCreateInfo::ColorType::NoColor)
        {
            RANGE_CHECK_RETURN_NULLPTR(cci->color_type);

            VABMap4f color(pc->GetVABMap(VAN::Color));

            if(color.IsValid())
            {
                if(cci->color_type == CubeCreateInfo::ColorType::SameColor)
                    color->RepeatWrite(cci->color[0], 24);
                else if(cci->color_type == CubeCreateInfo::ColorType::FaceColor)
                {
                    for(uint face = 0; face < 6; ++face)
                        color->RepeatWrite(cci->color[face], 4);
                }
                else if(cci->color_type == CubeCreateInfo::ColorType::VertexColor)
                    color->Write(cci->color, 24);
                else
                    return nullptr;
            }
        }

        // 8. 写入索引
        pc->WriteIBO(indices);

        // 9. 创建几何体
        Geometry *p = pc->Create();

        // 10. 设置包围体
        BoundingVolumes bv;
        bv.SetFromAABB(Vector3f(-0.5f, -0.5f, -0.5f), Vector3f(0.5f, 0.5f, 0.5f));
        p->SetBoundingVolumes(bv);

        return p;
    }
}//namespace hgl::graph::inline_geometry
