#pragma once

#include<hgl/type/Vector3f.h>
#include<hgl/color/Color4f.h>
#include<hgl/type/List.h>
#include<hgl/graph/Mesh.h>
#include<hgl/graph/Primitive.h>
#include<hgl/graph/PrimitiveCreater.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKPipeline.h>

namespace hgl::graph
{
    class RenderFramework;

    /**
     * 线条信息结构
     */
    struct LineInfo
    {
        Vector3f startPos;   // 线段起点
        Vector3f endPos;     // 线段终点
        Color4f  color;      // 线段颜色
        
        LineInfo() = default;
        LineInfo(const Vector3f& start, const Vector3f& end, const Color4f& c)
            : startPos(start), endPos(end), color(c) {}
    };

    /**
     * 线条绘制管理器
     * 使用Line3D材质统一管理和绘制多条线段
     */
    class LineManager
    {
    private:
        List<LineInfo> lines;                    // 存储所有线条信息
        
        RenderFramework*    render_framework;    // 渲染框架
        Material*           line_material;       // Line3D材质
        MaterialInstance*   material_instance;   // 材质实例
        
        Primitive*          primitive;           // 图元对象
        Mesh*               mesh;                // 网格对象
        
        bool                need_update;         // 是否需要更新网格
        
    private:
        
        bool CreateMaterial();                   // 创建Line3D材质
        bool UpdatePrimitive(Pipeline* pipeline); // 更新图元数据
        void CleanupResources();                 // 清理资源
        
    public:
        
        LineManager(RenderFramework* rf);
        ~LineManager();
        
        bool Init();                             // 初始化
        
        /**
         * 添加一条线段
         * @param start 起点坐标
         * @param end 终点坐标  
         * @param color 线段颜色
         */
        void AddLine(const Vector3f& start, const Vector3f& end, const Color4f& color);
        
        /**
         * 清除所有线段
         */
        void Clear();
        
        /**
         * 获取线段数量
         */
        uint32_t GetLineCount() const { return lines.GetCount(); }
        
        /**
         * 更新网格数据（在添加线段后调用）
         * @param pipeline 用于渲染的管线
         */
        bool Update(Pipeline* pipeline);
        
        /**
         * 获取用于渲染的网格对象
         */
        Mesh* GetMesh() { return mesh; }
        
        /**
         * 获取材质实例
         */
        MaterialInstance* GetMaterialInstance() { return material_instance; }
        
        /**
         * 检查是否有线段数据
         */
        bool HasLines() const { return lines.GetCount() > 0; }
    };
}