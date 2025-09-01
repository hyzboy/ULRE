#include<hgl/graph/LightVizGenerators.h>
#include<hgl/math/Math.h>
#include<cmath>

namespace hgl::graph::light_viz_generators
{
    // 辅助函数：计算向量叉积
    Vector3f CrossProduct(const Vector3f& a, const Vector3f& b)
    {
        return Vector3f(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        );
    }

    // 辅助函数：归一化向量
    Vector3f Normalize(const Vector3f& v)
    {
        float length = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        if (length > 0.0001f)
        {
            return Vector3f(v.x / length, v.y / length, v.z / length);
        }
        return Vector3f(0, 0, 1); // 默认方向
    }

    void GeneratePointLightLines(LineManager* light_mgr, const PointLight& light, float radius, const Color4f& color)
    {
        if (!light_mgr) return;

        const Vector3f& center = light.position;
        
        // 绘制三个正交的圆圈表示点光源的影响范围
        const int segments = 16;
        const float step = 2.0f * PI / segments;

        // XY平面的圆圈
        for (int i = 0; i < segments; ++i)
        {
            float angle1 = i * step;
            float angle2 = (i + 1) * step;
            
            Vector3f p1(center.x + radius * cos(angle1), center.y + radius * sin(angle1), center.z);
            Vector3f p2(center.x + radius * cos(angle2), center.y + radius * sin(angle2), center.z);
            
            light_mgr->AddLine(p1, p2, color);
        }

        // XZ平面的圆圈
        for (int i = 0; i < segments; ++i)
        {
            float angle1 = i * step;
            float angle2 = (i + 1) * step;
            
            Vector3f p1(center.x + radius * cos(angle1), center.y, center.z + radius * sin(angle1));
            Vector3f p2(center.x + radius * cos(angle2), center.y, center.z + radius * sin(angle2));
            
            light_mgr->AddLine(p1, p2, color);
        }

        // YZ平面的圆圈
        for (int i = 0; i < segments; ++i)
        {
            float angle1 = i * step;
            float angle2 = (i + 1) * step;
            
            Vector3f p1(center.x, center.y + radius * cos(angle1), center.z + radius * sin(angle1));
            Vector3f p2(center.x, center.y + radius * cos(angle2), center.z + radius * sin(angle2));
            
            light_mgr->AddLine(p1, p2, color);
        }

        // 添加一个简单的十字标记指示光源中心位置
        const float cross_size = radius * 0.2f;
        light_mgr->AddLine(Vector3f(center.x - cross_size, center.y, center.z), 
                          Vector3f(center.x + cross_size, center.y, center.z), color);
        light_mgr->AddLine(Vector3f(center.x, center.y - cross_size, center.z), 
                          Vector3f(center.x, center.y + cross_size, center.z), color);
        light_mgr->AddLine(Vector3f(center.x, center.y, center.z - cross_size), 
                          Vector3f(center.x, center.y, center.z + cross_size), color);
    }

    void GenerateSpotLightLines(LineManager* light_mgr, const SpotLight& light, float length, const Color4f& color)
    {
        if (!light_mgr) return;

        const Vector3f& pos = light.position;
        const Vector3f& dir = light.direction;
        
        // 计算聚光灯的圆锥角度
        float cone_radius = length * tan(acos(light.coscutoff));
        
        // 计算圆锥端点位置
        Vector3f cone_center = Vector3f(pos.x + dir.x * length, pos.y + dir.y * length, pos.z + dir.z * length);
        
        // 计算一个垂直于方向的向量作为圆锥的"右"方向
        Vector3f right;
        if (fabs(dir.x) < 0.9f)
            right = Vector3f(1, 0, 0);
        else
            right = Vector3f(0, 1, 0);
        
        // 计算垂直于方向和右方向的"上"方向
        Vector3f up = CrossProduct(dir, right);
        up = Normalize(up);
        right = CrossProduct(up, dir);
        right = Normalize(right);

        const int segments = 12;
        const float step = 2.0f * PI / segments;

        // 绘制圆锥底面圆圈
        for (int i = 0; i < segments; ++i)
        {
            float angle1 = i * step;
            float angle2 = (i + 1) * step;
            
            Vector3f offset1 = Vector3f(right.x * (cone_radius * cos(angle1)) + up.x * (cone_radius * sin(angle1)),
                                       right.y * (cone_radius * cos(angle1)) + up.y * (cone_radius * sin(angle1)),
                                       right.z * (cone_radius * cos(angle1)) + up.z * (cone_radius * sin(angle1)));
            Vector3f offset2 = Vector3f(right.x * (cone_radius * cos(angle2)) + up.x * (cone_radius * sin(angle2)),
                                       right.y * (cone_radius * cos(angle2)) + up.y * (cone_radius * sin(angle2)),
                                       right.z * (cone_radius * cos(angle2)) + up.z * (cone_radius * sin(angle2)));
            
            Vector3f p1 = Vector3f(cone_center.x + offset1.x, cone_center.y + offset1.y, cone_center.z + offset1.z);
            Vector3f p2 = Vector3f(cone_center.x + offset2.x, cone_center.y + offset2.y, cone_center.z + offset2.z);
            
            light_mgr->AddLine(p1, p2, color);
        }

        // 绘制从光源位置到圆锥边缘的线条
        const int cone_lines = 8;
        for (int i = 0; i < cone_lines; ++i)
        {
            float angle = i * 2.0f * PI / cone_lines;
            Vector3f offset = Vector3f(right.x * (cone_radius * cos(angle)) + up.x * (cone_radius * sin(angle)),
                                      right.y * (cone_radius * cos(angle)) + up.y * (cone_radius * sin(angle)),
                                      right.z * (cone_radius * cos(angle)) + up.z * (cone_radius * sin(angle)));
            Vector3f end_point = Vector3f(cone_center.x + offset.x, cone_center.y + offset.y, cone_center.z + offset.z);
            
            light_mgr->AddLine(pos, end_point, color);
        }

        // 绘制光源位置标记
        const float marker_size = length * 0.05f;
        light_mgr->AddLine(Vector3f(pos.x - marker_size, pos.y, pos.z), 
                          Vector3f(pos.x + marker_size, pos.y, pos.z), color);
        light_mgr->AddLine(Vector3f(pos.x, pos.y - marker_size, pos.z), 
                          Vector3f(pos.x, pos.y + marker_size, pos.z), color);
        light_mgr->AddLine(Vector3f(pos.x, pos.y, pos.z - marker_size), 
                          Vector3f(pos.x, pos.y, pos.z + marker_size), color);
    }

    void GenerateRectLightLines(LineManager* light_mgr, const Vector3f& position, const Vector3f& direction, 
                              const Vector3f& up, float width, float height, const Color4f& color)
    {
        if (!light_mgr) return;

        // 计算矩形的右方向
        Vector3f right = CrossProduct(up, direction);
        right = Normalize(right);

        // 确保up向量垂直于direction
        Vector3f corrected_up = CrossProduct(direction, right);
        corrected_up = Normalize(corrected_up);

        // 计算矩形四个角的位置
        Vector3f half_width = Vector3f(right.x * (width * 0.5f), right.y * (width * 0.5f), right.z * (width * 0.5f));
        Vector3f half_height = Vector3f(corrected_up.x * (height * 0.5f), corrected_up.y * (height * 0.5f), corrected_up.z * (height * 0.5f));

        Vector3f corner1 = Vector3f(position.x - half_width.x - half_height.x, 
                                   position.y - half_width.y - half_height.y, 
                                   position.z - half_width.z - half_height.z);  // 左下
        Vector3f corner2 = Vector3f(position.x + half_width.x - half_height.x, 
                                   position.y + half_width.y - half_height.y, 
                                   position.z + half_width.z - half_height.z);  // 右下
        Vector3f corner3 = Vector3f(position.x + half_width.x + half_height.x, 
                                   position.y + half_width.y + half_height.y, 
                                   position.z + half_width.z + half_height.z);  // 右上
        Vector3f corner4 = Vector3f(position.x - half_width.x + half_height.x, 
                                   position.y - half_width.y + half_height.y, 
                                   position.z - half_width.z + half_height.z);  // 左上

        // 绘制矩形边框
        light_mgr->AddLine(corner1, corner2, color);
        light_mgr->AddLine(corner2, corner3, color);
        light_mgr->AddLine(corner3, corner4, color);
        light_mgr->AddLine(corner4, corner1, color);

        // 绘制对角线以标示中心
        light_mgr->AddLine(corner1, corner3, color);
        light_mgr->AddLine(corner2, corner4, color);

        // 绘制方向指示器
        Vector3f dir_end = Vector3f(position.x + direction.x * (width * 0.3f), 
                                   position.y + direction.y * (width * 0.3f), 
                                   position.z + direction.z * (width * 0.3f));
        light_mgr->AddLine(position, dir_end, color);

        // 在方向线末端绘制箭头
        Vector3f arrow_back = Vector3f(dir_end.x - direction.x * (width * 0.1f),
                                      dir_end.y - direction.y * (width * 0.1f),
                                      dir_end.z - direction.z * (width * 0.1f));
        Vector3f arrow_side1 = Vector3f(arrow_back.x + right.x * (width * 0.05f),
                                       arrow_back.y + right.y * (width * 0.05f),
                                       arrow_back.z + right.z * (width * 0.05f));
        Vector3f arrow_side2 = Vector3f(arrow_back.x - right.x * (width * 0.05f),
                                       arrow_back.y - right.y * (width * 0.05f),
                                       arrow_back.z - right.z * (width * 0.05f));
        Vector3f arrow_side3 = Vector3f(arrow_back.x + corrected_up.x * (width * 0.05f),
                                       arrow_back.y + corrected_up.y * (width * 0.05f),
                                       arrow_back.z + corrected_up.z * (width * 0.05f));
        Vector3f arrow_side4 = Vector3f(arrow_back.x - corrected_up.x * (width * 0.05f),
                                       arrow_back.y - corrected_up.y * (width * 0.05f),
                                       arrow_back.z - corrected_up.z * (width * 0.05f));

        light_mgr->AddLine(dir_end, arrow_side1, color);
        light_mgr->AddLine(dir_end, arrow_side2, color);
        light_mgr->AddLine(dir_end, arrow_side3, color);
        light_mgr->AddLine(dir_end, arrow_side4, color);
    }

    void GenerateCameraFrustumLines(LineManager* light_mgr, const Vector3f& position, const Vector3f& direction,
                                  const Vector3f& up, float fov_y, float aspect, float near_dist, float far_dist, 
                                  const Color4f& color)
    {
        if (!light_mgr) return;

        // 计算右方向
        Vector3f right = CrossProduct(direction, up);
        right = Normalize(right);

        // 确保up向量垂直于direction
        Vector3f corrected_up = CrossProduct(right, direction);
        corrected_up = Normalize(corrected_up);

        // 计算近裁剪面和远裁剪面的尺寸
        float near_height = 2.0f * near_dist * tan(fov_y * 0.5f);
        float near_width = near_height * aspect;
        float far_height = 2.0f * far_dist * tan(fov_y * 0.5f);
        float far_width = far_height * aspect;

        // 计算近裁剪面中心和四个角
        Vector3f near_center = Vector3f(position.x + direction.x * near_dist, 
                                       position.y + direction.y * near_dist, 
                                       position.z + direction.z * near_dist);
        Vector3f near_half_width = Vector3f(right.x * (near_width * 0.5f), 
                                           right.y * (near_width * 0.5f), 
                                           right.z * (near_width * 0.5f));
        Vector3f near_half_height = Vector3f(corrected_up.x * (near_height * 0.5f), 
                                            corrected_up.y * (near_height * 0.5f), 
                                            corrected_up.z * (near_height * 0.5f));

        Vector3f near_corners[4] = {
            Vector3f(near_center.x - near_half_width.x - near_half_height.x,
                    near_center.y - near_half_width.y - near_half_height.y,
                    near_center.z - near_half_width.z - near_half_height.z),  // 左下
            Vector3f(near_center.x + near_half_width.x - near_half_height.x,
                    near_center.y + near_half_width.y - near_half_height.y,
                    near_center.z + near_half_width.z - near_half_height.z),  // 右下
            Vector3f(near_center.x + near_half_width.x + near_half_height.x,
                    near_center.y + near_half_width.y + near_half_height.y,
                    near_center.z + near_half_width.z + near_half_height.z),  // 右上
            Vector3f(near_center.x - near_half_width.x + near_half_height.x,
                    near_center.y - near_half_width.y + near_half_height.y,
                    near_center.z - near_half_width.z + near_half_height.z)   // 左上
        };

        // 计算远裁剪面中心和四个角
        Vector3f far_center = Vector3f(position.x + direction.x * far_dist, 
                                      position.y + direction.y * far_dist, 
                                      position.z + direction.z * far_dist);
        Vector3f far_half_width = Vector3f(right.x * (far_width * 0.5f), 
                                          right.y * (far_width * 0.5f), 
                                          right.z * (far_width * 0.5f));
        Vector3f far_half_height = Vector3f(corrected_up.x * (far_height * 0.5f), 
                                           corrected_up.y * (far_height * 0.5f), 
                                           corrected_up.z * (far_height * 0.5f));

        Vector3f far_corners[4] = {
            Vector3f(far_center.x - far_half_width.x - far_half_height.x,
                    far_center.y - far_half_width.y - far_half_height.y,
                    far_center.z - far_half_width.z - far_half_height.z),    // 左下
            Vector3f(far_center.x + far_half_width.x - far_half_height.x,
                    far_center.y + far_half_width.y - far_half_height.y,
                    far_center.z + far_half_width.z - far_half_height.z),    // 右下
            Vector3f(far_center.x + far_half_width.x + far_half_height.x,
                    far_center.y + far_half_width.y + far_half_height.y,
                    far_center.z + far_half_width.z + far_half_height.z),    // 右上
            Vector3f(far_center.x - far_half_width.x + far_half_height.x,
                    far_center.y - far_half_width.y + far_half_height.y,
                    far_center.z - far_half_width.z + far_half_height.z)     // 左上
        };

        // 绘制近裁剪面
        for (int i = 0; i < 4; ++i)
        {
            light_mgr->AddLine(near_corners[i], near_corners[(i + 1) % 4], color);
        }

        // 绘制远裁剪面
        for (int i = 0; i < 4; ++i)
        {
            light_mgr->AddLine(far_corners[i], far_corners[(i + 1) % 4], color);
        }

        // 绘制连接近远裁剪面的边
        for (int i = 0; i < 4; ++i)
        {
            light_mgr->AddLine(near_corners[i], far_corners[i], color);
        }

        // 绘制摄像机位置标记
        const float marker_size = near_dist * 0.1f;
        light_mgr->AddLine(Vector3f(position.x - marker_size, position.y, position.z), 
                          Vector3f(position.x + marker_size, position.y, position.z), color);
        light_mgr->AddLine(Vector3f(position.x, position.y - marker_size, position.z), 
                          Vector3f(position.x, position.y + marker_size, position.z), color);
        light_mgr->AddLine(Vector3f(position.x, position.y, position.z - marker_size), 
                          Vector3f(position.x, position.y, position.z + marker_size), color);
    }

    void GenerateCommonLightingSetup(LineManager* light_mgr, const Vector3f& center_pos, float scale)
    {
        if (!light_mgr) return;

        // 主要点光源（模拟太阳光）
        PointLight main_light;
        main_light.position = Vector3f(center_pos.x + scale * 2, center_pos.y + scale * 3, center_pos.z + scale * 1);
        GeneratePointLightLines(light_mgr, main_light, scale * 1.5f, Color4f(1, 1, 0.8f, 1)); // 暖白色

        // 补充聚光灯（模拟重点照明）
        SpotLight accent_light;
        accent_light.position = Vector3f(center_pos.x - scale * 2, center_pos.y + scale * 2, center_pos.z + scale * 2);
        accent_light.direction = Vector3f(0.3f, -0.6f, -0.4f);
        accent_light.coscutoff = 0.85f;
        GenerateSpotLightLines(light_mgr, accent_light, scale * 3, Color4f(0.8f, 0.9f, 1, 1)); // 冷白色

        // 环境面光源（模拟柔光）
        Vector3f area_pos = Vector3f(center_pos.x, center_pos.y + scale * 4, center_pos.z - scale * 1);
        Vector3f area_dir = Normalize(Vector3f(0, -0.8f, 0.2f));
        Vector3f area_up = Normalize(Vector3f(0, 0.2f, 1));
        GenerateRectLightLines(light_mgr, area_pos, area_dir, area_up, 
                              scale * 2, scale * 1.5f, Color4f(0.9f, 0.9f, 1, 1)); // 柔和蓝白

        // 摄像机视角参考
        Vector3f cam_pos = Vector3f(center_pos.x + scale * 4, center_pos.y + scale * 3, center_pos.z + scale * 4);
        Vector3f cam_dir = Normalize(Vector3f(-1, -0.5f, -1));
        Vector3f cam_up(0, 1, 0);
        GenerateCameraFrustumLines(light_mgr, cam_pos, cam_dir, cam_up,
                                 PI * 0.25f, 16.0f / 9.0f, scale * 0.5f, scale * 8,
                                 Color4f(0.6f, 0.6f, 0.6f, 1)); // 灰色
    }

    void GenerateDebugAxisAtPosition(LineManager* light_mgr, const Vector3f& position, float size, float alpha)
    {
        if (!light_mgr) return;

        // X轴 - 红色
        light_mgr->AddLine(position, 
                          Vector3f(position.x + size, position.y, position.z), 
                          Color4f(1, 0, 0, alpha));
        
        // Y轴 - 绿色
        light_mgr->AddLine(position, 
                          Vector3f(position.x, position.y + size, position.z), 
                          Color4f(0, 1, 0, alpha));
        
        // Z轴 - 蓝色
        light_mgr->AddLine(position, 
                          Vector3f(position.x, position.y, position.z + size), 
                          Color4f(0, 0, 1, alpha));
    }
}