#include<hgl/graph/geo/Revolution.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/PrimitiveCreater.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace hgl::graph::inline_geometry
{
    /**
     * 创建一个由2D轮廓旋转生成的3D几何体
     */
    Primitive *CreateRevolution(PrimitiveCreater *pc, const RevolutionCreateInfo *rci)
    {
        if(!pc || !rci || !rci->profile_points || rci->profile_count < 2 || rci->revolution_slices < 3)
            return nullptr;

        const uint profile_count = rci->profile_count;
        const uint slices = rci->revolution_slices;
        const float start_rad = deg2rad(rci->start_angle);
        const float sweep_rad = deg2rad(rci->sweep_angle);
        const float angle_step = sweep_rad / float(slices);
        const bool is_full_revolution = (fabsf(rci->sweep_angle - 360.0f) < 0.001f);

        // 计算顶点和索引数量
        uint numberVertices = profile_count * (slices + 1);
        uint numberIndices = 0;

        // 侧面三角形：每个profile段在每个slice都会生成2个三角形
        if(profile_count > 1) {
            numberIndices += (profile_count - 1) * slices * 6;
        }

        // 如果轮廓是闭合的，添加首尾连接的三角形
        if(rci->close_profile && profile_count > 2) {
            numberIndices += slices * 6;
        }

        // 如果不是完整旋转，需要添加端面
        if(!is_full_revolution) {
            if(profile_count > 2) {
                numberIndices += (profile_count - 2) * 6; // 每个端面2个三角形扇
            }
        }

        if(!pc->Init("Revolution", numberVertices, numberIndices))
            return nullptr;

        VABMapFloat vertex   (pc->GetVABMap(VAN::Position), VF_V3F);
        VABMapFloat normal   (pc->GetVABMap(VAN::Normal),   VF_V3F);
        VABMapFloat tangent  (pc->GetVABMap(VAN::Tangent),  VF_V3F);
        VABMapFloat tex_coord(pc->GetVABMap(VAN::TexCoord), VF_V2F);

        float *vp = vertex;
        float *np = normal;
        float *tp = tangent;
        float *tcp = tex_coord;

        if(!vp)
            return nullptr;

        // 用于切线计算的辅助向量和四元数
        float helpVector[3] = { 1.0f, 0.0f, 0.0f };

        // 生成顶点
        for(uint slice = 0; slice <= slices; slice++) {
            float angle = start_rad + angle_step * float(slice);
            float cos_angle = cos(angle);
            float sin_angle = sin(angle);

            for(uint p = 0; p < profile_count; p++) {
                // 从轮廓点计算3D坐标
                float radius = rci->profile_points[p].x;
                float height = rci->profile_points[p].y;

                // 绕Z轴旋转（Z轴向上）
                float x = radius * cos_angle;
                float y = radius * sin_angle;
                float z = height;

                // 应用旋转中心偏移
                x += rci->revolution_center.x;
                y += rci->revolution_center.y;
                z += rci->revolution_center.z;

                *vp++ = x;
                *vp++ = y;
                *vp++ = z;

                // 计算法线
                if(np) {
                    // 计算沿轮廓的切线方向
                    float profile_tangent_x, profile_tangent_y;
                    if(p == 0 && profile_count > 1) {
                        profile_tangent_x = rci->profile_points[1].x - rci->profile_points[0].x;
                        profile_tangent_y = rci->profile_points[1].y - rci->profile_points[0].y;
                    } else if(p == profile_count - 1 && profile_count > 1) {
                        profile_tangent_x = rci->profile_points[p].x - rci->profile_points[p-1].x;
                        profile_tangent_y = rci->profile_points[p].y - rci->profile_points[p-1].y;
                    } else if(profile_count > 2) {
                        profile_tangent_x = (rci->profile_points[p+1].x - rci->profile_points[p-1].x) * 0.5f;
                        profile_tangent_y = (rci->profile_points[p+1].y - rci->profile_points[p-1].y) * 0.5f;
                    } else {
                        profile_tangent_x = 0.0f;
                        profile_tangent_y = 1.0f; // 默认向上
                    }

                    // 归一化轮廓切线
                    float profile_len = sqrt(profile_tangent_x * profile_tangent_x + profile_tangent_y * profile_tangent_y);
                    if(profile_len > 0.001f) {
                        profile_tangent_x /= profile_len;
                        profile_tangent_y /= profile_len;
                    }

                    // 轮廓法线（垂直于轮廓切线）
                    float profile_normal_x = -profile_tangent_y;
                    float profile_normal_y = profile_tangent_x;

                    // 转换到3D空间的表面法线
                    float surface_normal_x = profile_normal_x * cos_angle;
                    float surface_normal_y = profile_normal_x * sin_angle;
                    float surface_normal_z = profile_normal_y;

                    // 归一化法线
                    float normal_len = sqrt(surface_normal_x * surface_normal_x +
                                          surface_normal_y * surface_normal_y +
                                          surface_normal_z * surface_normal_z);
                    if(normal_len > 0.001f) {
                        surface_normal_x /= normal_len;
                        surface_normal_y /= normal_len;
                        surface_normal_z /= normal_len;
                    }

                    *np++ = surface_normal_x;
                    *np++ = surface_normal_y;
                    *np++ = surface_normal_z;
                }

                // 计算纹理坐标
                if(tcp) {
                    float u = float(slice) / float(slices) * rci->uv_scale.x;
                    float v = float(p) / float(profile_count - 1) * rci->uv_scale.y;
                    *tcp++ = u;
                    *tcp++ = v;
                }

                // 计算切线
                if(tp) {
                    // 切线沿旋转方向
                    float tangent_x = -sin_angle;
                    float tangent_y = cos_angle;
                    float tangent_z = 0.0f;

                    *tp++ = tangent_x;
                    *tp++ = tangent_y;
                    *tp++ = tangent_z;
                }
            }
        }

        // 生成索引
        {
            const IndexType index_type = pc->GetIndexType();

            if(index_type == IndexType::U8)
            {
                IBTypeMap<uint8_t> ib_map(pc->GetIBMap());
                uint8_t *indices = ib_map;

                // 侧面三角形
                for(uint slice = 0; slice < slices; slice++) {
                    for(uint p = 0; p < profile_count - 1; p++) {
                        uint8_t i0 = uint8_t(slice * profile_count + p);
                        uint8_t i1 = uint8_t(slice * profile_count + p + 1);
                        uint8_t i2 = uint8_t((slice + 1) * profile_count + p);
                        uint8_t i3 = uint8_t((slice + 1) * profile_count + p + 1);

                        *indices++ = i0;
                        *indices++ = i2;
                        *indices++ = i1;

                        *indices++ = i1;
                        *indices++ = i2;
                        *indices++ = i3;
                    }

                    // 如果轮廓闭合，连接最后一个点到第一个点
                    if(rci->close_profile && profile_count > 2) {
                        uint8_t i0 = uint8_t(slice * profile_count + (profile_count - 1));
                        uint8_t i1 = uint8_t(slice * profile_count + 0);
                        uint8_t i2 = uint8_t((slice + 1) * profile_count + (profile_count - 1));
                        uint8_t i3 = uint8_t((slice + 1) * profile_count + 0);

                        *indices++ = i0;
                        *indices++ = i2;
                        *indices++ = i1;

                        *indices++ = i1;
                        *indices++ = i2;
                        *indices++ = i3;
                    }
                }

                // 端面（如果不是完整旋转）
                if(!is_full_revolution && profile_count > 2) {
                    // 起始端面
                    for(uint p = 1; p < profile_count - 1; p++) {
                        *indices++ = 0;
                        *indices++ = uint8_t(p);
                        *indices++ = uint8_t(p + 1);
                    }

                    // 结束端面
                    uint end_offset = slices * profile_count;
                    for(uint p = 1; p < profile_count - 1; p++) {
                        *indices++ = uint8_t(end_offset);
                        *indices++ = uint8_t(end_offset + p + 1);
                        *indices++ = uint8_t(end_offset + p);
                    }
                }
            }
            else if(index_type == IndexType::U16) {
                IBTypeMap<uint16> ib_map(pc->GetIBMap());
                uint16 *indices = ib_map;

                // 侧面三角形
                for(uint slice = 0; slice < slices; slice++) {
                    for(uint p = 0; p < profile_count - 1; p++) {
                        uint16 i0 = slice * profile_count + p;
                        uint16 i1 = slice * profile_count + p + 1;
                        uint16 i2 = (slice + 1) * profile_count + p;
                        uint16 i3 = (slice + 1) * profile_count + p + 1;

                        // 第一个三角形 (顺时针)
                        *indices++ = i0;
                        *indices++ = i2;
                        *indices++ = i1;

                        // 第二个三角形 (顺时针)
                        *indices++ = i1;
                        *indices++ = i2;
                        *indices++ = i3;
                    }

                    // 如果轮廓闭合，连接最后一个点到第一个点
                    if(rci->close_profile && profile_count > 2) {
                        uint16 i0 = slice * profile_count + (profile_count - 1);
                        uint16 i1 = slice * profile_count + 0;
                        uint16 i2 = (slice + 1) * profile_count + (profile_count - 1);
                        uint16 i3 = (slice + 1) * profile_count + 0;

                        *indices++ = i0;
                        *indices++ = i2;
                        *indices++ = i1;

                        *indices++ = i1;
                        *indices++ = i2;
                        *indices++ = i3;
                    }
                }

                // 端面（如果不是完整旋转）
                if(!is_full_revolution && profile_count > 2) {
                    // 起始端面
                    for(uint p = 1; p < profile_count - 1; p++) {
                        *indices++ = 0;
                        *indices++ = p;
                        *indices++ = p + 1;
                    }

                    // 结束端面
                    uint end_offset = slices * profile_count;
                    for(uint p = 1; p < profile_count - 1; p++) {
                        *indices++ = end_offset;
                        *indices++ = end_offset + p + 1;
                        *indices++ = end_offset + p;
                    }
                }
            }
            else if(index_type == IndexType::U32) {
                IBTypeMap<uint32> ib_map(pc->GetIBMap());
                uint32 *indices = ib_map;

                // 侧面三角形
                for(uint slice = 0; slice < slices; slice++) {
                    for(uint p = 0; p < profile_count - 1; p++) {
                        uint32 i0 = slice * profile_count + p;
                        uint32 i1 = slice * profile_count + p + 1;
                        uint32 i2 = (slice + 1) * profile_count + p;
                        uint32 i3 = (slice + 1) * profile_count + p + 1;

                        *indices++ = i0;
                        *indices++ = i2;
                        *indices++ = i1;

                        *indices++ = i1;
                        *indices++ = i2;
                        *indices++ = i3;
                    }

                    if(rci->close_profile && profile_count > 2) {
                        uint32 i0 = slice * profile_count + (profile_count - 1);
                        uint32 i1 = slice * profile_count + 0;
                        uint32 i2 = (slice + 1) * profile_count + (profile_count - 1);
                        uint32 i3 = (slice + 1) * profile_count + 0;

                        *indices++ = i0;
                        *indices++ = i2;
                        *indices++ = i1;

                        *indices++ = i1;
                        *indices++ = i2;
                        *indices++ = i3;
                    }
                }

                if(!is_full_revolution && profile_count > 2) {
                    for(uint p = 1; p < profile_count - 1; p++) {
                        *indices++ = 0;
                        *indices++ = p;
                        *indices++ = p + 1;
                    }

                    uint end_offset = slices * profile_count;
                    for(uint p = 1; p < profile_count - 1; p++) {
                        *indices++ = end_offset;
                        *indices++ = end_offset + p + 1;
                        *indices++ = end_offset + p;
                    }
                }
            }
            else {
                return nullptr;
            }
        }

        Primitive *p = pc->Create();

        // 计算包围盒
        if(p) {
            float max_radius = 0.0f;
            float min_height = FLT_MAX;
            float max_height = -FLT_MAX;

            for(uint i = 0; i < profile_count; i++) {
                max_radius = std::max(max_radius, fabsf(rci->profile_points[i].x));
                min_height = std::min(min_height, rci->profile_points[i].y);
                max_height = std::max(max_height, rci->profile_points[i].y);
            }

            Vector3f min_bound(rci->revolution_center.x - max_radius,
                               rci->revolution_center.y - max_radius,
                               rci->revolution_center.z + min_height);
            Vector3f max_bound(rci->revolution_center.x + max_radius,
                               rci->revolution_center.y + max_radius,
                               rci->revolution_center.z + max_height);

            p->SetBoundingBox(min_bound, max_bound);
        }

        return p;
    }
}
