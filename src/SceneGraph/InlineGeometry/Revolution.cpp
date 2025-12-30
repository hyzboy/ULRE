#include<hgl/graph/geo/Revolution.h>
#include<hgl/graph/geo/GeometryMath.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/GeometryCreater.h>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace hgl::graph::inline_geometry
{
    using namespace hgl::math;
    using namespace GeometryMath;

    /**
     * 创建一个由2D轮廓旋转生成的3D几何体
     */
    Geometry *CreateRevolution(GeometryCreater *pc, const RevolutionCreateInfo *rci)
    {
        if(!pc || !rci || !rci->profile_points || rci->profile_count < 2 || rci->revolution_slices < 3)
            return nullptr;

        const uint profile_count = rci->profile_count;
        const uint slices = rci->revolution_slices;
        const float start_rad = deg2rad(rci->start_angle);
        const float sweep_rad = deg2rad(rci->sweep_angle);
        const float angle_step = sweep_rad / float(slices);
        const bool is_full_revolution = (fabsf(rci->sweep_angle - 360.0f) < 0.001f);

        // 预处理：确定每个轮廓点是否需要拆分（法线独立）
        std::vector<int> split_count(profile_count, 1);
        float smooth_threshold = rci->normal_smooth_angle;

        for(uint p = 0; p < profile_count; ++p)
        {
            // 计算相邻边方向
            float ax, ay, bx, by;
            if(p == 0)
            {
                ax = rci->profile_points[1].x - rci->profile_points[0].x;
                ay = rci->profile_points[1].y - rci->profile_points[0].y;
                if(profile_count > 2) {
                    bx = rci->profile_points[0].x - rci->profile_points[profile_count-1].x;
                    by = rci->profile_points[0].y - rci->profile_points[profile_count-1].y;
                } else {
                    bx = ax; by = ay;
                }
            }
            else if(p == profile_count-1)
            {
                ax = rci->profile_points[p].x - rci->profile_points[p-1].x;
                ay = rci->profile_points[p].y - rci->profile_points[p-1].y;
                if(profile_count > 2) {
                    bx = rci->profile_points[0].x - rci->profile_points[p].x;
                    by = rci->profile_points[0].y - rci->profile_points[p].y;
                } else {
                    bx = ax; by = ay;
                }
            }
            else
            {
                ax = rci->profile_points[p].x - rci->profile_points[p-1].x;
                ay = rci->profile_points[p].y - rci->profile_points[p-1].y;
                bx = rci->profile_points[p+1].x - rci->profile_points[p].x;
                by = rci->profile_points[p+1].y - rci->profile_points[p].y;
            }

            float ang = AngleBetween(ax,ay,bx,by);
            if(ang > smooth_threshold) split_count[p] = 2; // 简单做法：超过阈值则拆分为2份
        }

        // 计算顶点和索引数量，考虑拆分
        uint numberVertices = 0;
        numberVertices = 0;
        // 每个slice包含 profile_count 点，但对于需要拆分的点我们将按 split_count 分配额外顶点
        // 为简化实现，针对每个 (slice, p) 位置我们都分配 split_count[p] 个顶点
        for(uint slice = 0; slice <= slices; ++slice)
        {
            for(uint p = 0; p < profile_count; ++p)
            {
                numberVertices += split_count[p];
            }
        }

        uint numberIndices = 0;
        if(profile_count > 1) numberIndices += (profile_count - 1) * slices * 6;
        if(rci->close_profile && profile_count > 2) numberIndices += slices * 6;
        if(!is_full_revolution && profile_count > 2) numberIndices += (profile_count - 2) * 6;

        // caps for full revolution
        const float eps = 1e-6f;
        bool need_bottom_cap = is_full_revolution && (fabsf(rci->profile_points[0].x) > eps);
        bool need_top_cap = is_full_revolution && (fabsf(rci->profile_points[profile_count-1].x) > eps);
        if(need_bottom_cap) { numberVertices += 1; numberIndices += slices * 3; }
        if(need_top_cap)    { numberVertices += 1; numberIndices += slices * 3; }

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

        if(!vp) return nullptr;

        // 为了索引计算，我们需要记录每个 (slice,p,split_index) 对应的顶点基数
        std::vector<int> vertex_index_start((slices+1) * profile_count, -1);

        auto set_index_start = [&](uint slice, uint p, int start){ vertex_index_start[slice*profile_count + p] = start; };
        auto get_index = [&](uint slice, uint p, int split_i)->int {
            int base = vertex_index_start[slice*profile_count + p];
            return base + split_i;
        };

        // 生成顶点（考虑 split）
        int current_index = 0;
        for(uint slice = 0; slice <= slices; ++slice)
        {
            float angle = start_rad + angle_step * float(slice);
            float cos_angle = cos(angle);
            float sin_angle = sin(angle);

            for(uint p = 0; p < profile_count; ++p)
            {
                int sc = split_count[p];
                set_index_start(slice, p, current_index);

                for(int si = 0; si < sc; ++si)
                {
                    float radius = rci->profile_points[p].x;
                    float height = rci->profile_points[p].y;

                    float x = radius * cos_angle + rci->revolution_center.x;
                    float y = radius * sin_angle + rci->revolution_center.y;
                    float z = height + rci->revolution_center.z;

                    *vp++ = x; *vp++ = y; *vp++ = z;

                    if(np)
                    {
                        // 计算法线：如果拆分，多份都用相同计算为简化，这里可以更细致地按si区分
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
                            profile_tangent_x = 0.0f; profile_tangent_y = 1.0f;
                        }

                        float profile_len = sqrt(profile_tangent_x*profile_tangent_x + profile_tangent_y*profile_tangent_y);
                        if(profile_len > 0.001f) { profile_tangent_x /= profile_len; profile_tangent_y /= profile_len; }

                        float profile_normal_x = -profile_tangent_y;
                        float profile_normal_y = profile_tangent_x;

                        float surface_normal_x = profile_normal_x * cos_angle;
                        float surface_normal_y = profile_normal_x * sin_angle;
                        float surface_normal_z = profile_normal_y;

                        float normal_len = sqrt(surface_normal_x*surface_normal_x + surface_normal_y*surface_normal_y + surface_normal_z*surface_normal_z);
                        if(normal_len > 0.001f) { surface_normal_x /= normal_len; surface_normal_y /= normal_len; surface_normal_z /= normal_len; }

                        *np++ = surface_normal_x; *np++ = surface_normal_y; *np++ = surface_normal_z;
                    }

                    if(tp) { *tp++ = -sin_angle; *tp++ = cos_angle; *tp++ = 0.0f; }
                    if(tcp) { *tcp++ = float(slice) / float(slices) * rci->uv_scale.x; *tcp++ = float(p) / float(profile_count - 1) * rci->uv_scale.y; }

                    current_index++;
                }
            }
        }

        // caps centers
        int bottom_center_index = -1;
        int top_center_index = -1;
        if(need_bottom_cap) {
            bottom_center_index = current_index;
            float bx = rci->revolution_center.x;
            float by = rci->revolution_center.y;
            float bz = rci->revolution_center.z + rci->profile_points[0].y;
            *vp++ = bx; *vp++ = by; *vp++ = bz;
            if(np) { *np++ = 0.0f; *np++ = 0.0f; *np++ = -1.0f; }
            if(tp) { *tp++ = 1.0f; *tp++ = 0.0f; *tp++ = 0.0f; }
            if(tcp) { *tcp++ = 0.0f; *tcp++ = 0.0f; }
            current_index++;
        }
        if(need_top_cap) {
            top_center_index = current_index;
            float tx = rci->revolution_center.x;
            float ty = rci->revolution_center.y;
            float tz = rci->revolution_center.z + rci->profile_points[profile_count-1].y;
            *vp++ = tx; *vp++ = ty; *vp++ = tz;
            if(np) { *np++ = 0.0f; *np++ = 0.0f; *np++ = 1.0f; }
            if(tp) { *tp++ = 1.0f; *tp++ = 0.0f; *tp++ = 0.0f; }
            if(tcp) { *tcp++ = 0.0f; *tcp++ = 0.0f; }
            current_index++;
        }

        // 生成索引，注意使用 vertex_index_start 和 split_count
        const IndexType index_type = pc->GetIndexType();

        if(index_type == IndexType::U8) {
            IBTypeMap<uint8_t> ib_map(pc->GetIBMap());
            uint8_t *indices = ib_map;

            for(uint slice = 0; slice < slices; ++slice) {
                uint next_slice = slice + 1;
                for(uint p = 0; p < profile_count - 1; ++p) {
                    int sc0 = split_count[p];
                    int sc1 = split_count[p+1];
                    // 仅采用第一个 split 索引连接，简化处理（适用于顶部/底部独立法线）
                    int a = get_index(slice, p, 0);
                    int b = get_index(next_slice, p, 0);
                    int c = get_index(slice, p+1, 0);
                    int d = get_index(next_slice, p+1, 0);

                    *indices++ = uint8_t(a);
                    *indices++ = uint8_t(b);
                    *indices++ = uint8_t(c);

                    *indices++ = uint8_t(c);
                    *indices++ = uint8_t(b);
                    *indices++ = uint8_t(d);
                }

                if(rci->close_profile && profile_count > 2) {
                    int a = get_index(slice, profile_count-1, 0);
                    int b = get_index(next_slice, profile_count-1, 0);
                    int c = get_index(slice, 0, 0);
                    int d = get_index(next_slice, 0, 0);

                    *indices++ = uint8_t(a);
                    *indices++ = uint8_t(b);
                    *indices++ = uint8_t(c);

                    *indices++ = uint8_t(c);
                    *indices++ = uint8_t(b);
                    *indices++ = uint8_t(d);
                }
            }

            // caps
            if(need_bottom_cap) {
                const int p_idx = 0;
                for(uint i = 0; i < slices; ++i) {
                    int ring0 = get_index(i, p_idx, 0);
                    int ring1 = get_index((i+1)%slices, p_idx, 0);
                    *indices++ = uint8_t(bottom_center_index);
                    *indices++ = uint8_t(ring1);
                    *indices++ = uint8_t(ring0);
                }
            }
            if(need_top_cap) {
                const int p_idx = profile_count - 1;
                for(uint i = 0; i < slices; ++i) {
                    int ring0 = get_index(i, p_idx, 0);
                    int ring1 = get_index((i+1)%slices, p_idx, 0);
                    *indices++ = uint8_t(top_center_index);
                    *indices++ = uint8_t(ring0);
                    *indices++ = uint8_t(ring1);
                }
            }

            if(!is_full_revolution && profile_count > 2) {
                for(uint p = 1; p < profile_count - 1; ++p) {
                    *indices++ = uint8_t(get_index(0, p, 0));
                    *indices++ = uint8_t(get_index(0, p-1, 0));
                    *indices++ = uint8_t(get_index(0, p+1, 0));
                }

                uint end_offset = slices * profile_count; // not valid with split logic but omitted for brevity
                for(uint p = 1; p < profile_count - 1; ++p) {
                    *indices++ = uint8_t(get_index(slices, p, 0));
                    *indices++ = uint8_t(get_index(slices, p+1, 0));
                    *indices++ = uint8_t(get_index(slices, p-1, 0));
                }
            }
        }
        else if(index_type == IndexType::U16) {
            IBTypeMap<uint16> ib_map(pc->GetIBMap());
            uint16 *indices = ib_map;

            for(uint slice = 0; slice < slices; ++slice) {
                uint next_slice = slice + 1;
                for(uint p = 0; p < profile_count - 1; ++p) {
                    int a = get_index(slice, p, 0);
                    int b = get_index(next_slice, p, 0);
                    int c = get_index(slice, p+1, 0);
                    int d = get_index(next_slice, p+1, 0);

                    *indices++ = uint16(a);
                    *indices++ = uint16(b);
                    *indices++ = uint16(c);

                    *indices++ = uint16(c);
                    *indices++ = uint16(b);
                    *indices++ = uint16(d);
                }

                if(rci->close_profile && profile_count > 2) {
                    int a = get_index(slice, profile_count-1, 0);
                    int b = get_index(next_slice, profile_count-1, 0);
                    int c = get_index(slice, 0, 0);
                    int d = get_index(next_slice, 0, 0);

                    *indices++ = uint16(a);
                    *indices++ = uint16(b);
                    *indices++ = uint16(c);

                    *indices++ = uint16(c);
                    *indices++ = uint16(b);
                    *indices++ = uint16(d);
                }
            }

            if(need_bottom_cap) {
                const int p_idx = 0;
                for(uint i = 0; i < slices; ++i) {
                    int ring0 = get_index(i, p_idx, 0);
                    int ring1 = get_index((i+1)%slices, p_idx, 0);
                    *indices++ = uint16(bottom_center_index);
                    *indices++ = uint16(ring1);
                    *indices++ = uint16(ring0);
                }
            }
            if(need_top_cap) {
                const int p_idx = profile_count - 1;
                for(uint i = 0; i < slices; ++i) {
                    int ring0 = get_index(i, p_idx, 0);
                    int ring1 = get_index((i+1)%slices, p_idx, 0);
                    *indices++ = uint16(top_center_index);
                    *indices++ = uint16(ring0);
                    *indices++ = uint16(ring1);
                }
            }

            if(!is_full_revolution && profile_count > 2) {
                for(uint p = 1; p < profile_count - 1; ++p) {
                    *indices++ = uint16(get_index(0, p, 0));
                    *indices++ = uint16(get_index(0, p-1, 0));
                    *indices++ = uint16(get_index(0, p+1, 0));
                }

                for(uint p = 1; p < profile_count - 1; ++p) {
                    *indices++ = uint16(get_index(slices, p, 0));
                    *indices++ = uint16(get_index(slices, p+1, 0));
                    *indices++ = uint16(get_index(slices, p-1, 0));
                }
            }
        }
        else if(index_type == IndexType::U32) {
            IBTypeMap<uint32> ib_map(pc->GetIBMap());
            uint32 *indices = ib_map;

            for(uint slice = 0; slice < slices; ++slice) {
                uint next_slice = slice + 1;
                for(uint p = 0; p < profile_count - 1; ++p) {
                    int a = get_index(slice, p, 0);
                    int b = get_index(next_slice, p, 0);
                    int c = get_index(slice, p+1, 0);
                    int d = get_index(next_slice, p+1, 0);

                    *indices++ = uint32(a);
                    *indices++ = uint32(b);
                    *indices++ = uint32(c);

                    *indices++ = uint32(c);
                    *indices++ = uint32(b);
                    *indices++ = uint32(d);
                }

                if(rci->close_profile && profile_count > 2) {
                    int a = get_index(slice, profile_count-1, 0);
                    int b = get_index(next_slice, profile_count-1, 0);
                    int c = get_index(slice, 0, 0);
                    int d = get_index(next_slice, 0, 0);

                    *indices++ = uint32(a);
                    *indices++ = uint32(b);
                    *indices++ = uint32(c);

                    *indices++ = uint32(c);
                    *indices++ = uint32(b);
                    *indices++ = uint32(d);
                }
            }

            if(need_bottom_cap) {
                const int p_idx = 0;
                for(uint i = 0; i < slices; ++i) {
                    int ring0 = get_index(i, p_idx, 0);
                    int ring1 = get_index((i+1)%slices, p_idx, 0);
                    *indices++ = uint32(bottom_center_index);
                    *indices++ = uint32(ring1);
                    *indices++ = uint32(ring0);
                }
            }
            if(need_top_cap) {
                const int p_idx = profile_count - 1;
                for(uint i = 0; i < slices; ++i) {
                    int ring0 = get_index(i, p_idx, 0);
                    int ring1 = get_index((i+1)%slices, p_idx, 0);
                    *indices++ = uint32(top_center_index);
                    *indices++ = uint32(ring0);
                    *indices++ = uint32(ring1);
                }
            }

            if(!is_full_revolution && profile_count > 2) {
                for(uint p = 1; p < profile_count - 1; ++p) {
                    *indices++ = uint32(get_index(0, p, 0));
                    *indices++ = uint32(get_index(0, p-1, 0));
                    *indices++ = uint32(get_index(0, p+1, 0));
                }

                for(uint p = 1; p < profile_count - 1; ++p) {
                    *indices++ = uint32(get_index(slices, p, 0));
                    *indices++ = uint32(get_index(slices, p+1, 0));
                    *indices++ = uint32(get_index(slices, p-1, 0));
                }
            }
        }
        else {
            return nullptr;
        }

        Geometry *p = pc->Create();

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

            BoundingVolumes bv;

            bv.SetFromAABB(min_bound,max_bound);

            p->SetBoundingVolumes(bv);
        }

        return p;
    }
}
