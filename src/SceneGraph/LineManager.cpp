#include<hgl/graph/LineManager.h>
#include<hgl/graph/RenderFramework.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/PrimitiveCreater.h>
#include<hgl/log/LogInfo.h>

namespace hgl::graph
{
    LineManager::LineManager(RenderFramework* rf)
        : render_framework(rf)
        , line_material(nullptr)
        , material_instance(nullptr)
        , pipeline(nullptr)
        , primitive(nullptr)
        , mesh(nullptr)
        , need_update(false)
    {
    }

    LineManager::~LineManager()
    {
        CleanupResources();
    }

    bool LineManager::Init()
    {
        if (!render_framework)
        {
            LOG_ERROR("RenderFramework is null");
            return false;
        }

        return CreateMaterial();
    }

    bool LineManager::CreateMaterial()
    {
        // 创建Line3D材质配置
        mtl::Line3DMaterialCreateConfig cfg;
        
        // 创建Line3D材质
        auto* mci = mtl::CreateLine3D(render_framework->GetDevAttr(), &cfg);
        if (!mci)
        {
            LOG_ERROR("Failed to create Line3D material create info");
            return false;
        }

        line_material = render_framework->CreateMaterial("LineManager_Line3D", mci);
        if (!line_material)
        {
            LOG_ERROR("Failed to create Line3D material");
            return false;
        }

        // 创建材质实例
        material_instance = render_framework->CreateMaterialInstance(line_material);
        if (!material_instance)
        {
            LOG_ERROR("Failed to create material instance");
            return false;
        }

        // 创建渲染管线
        pipeline = render_framework->CreatePipeline(line_material, InlinePipeline::Solid3D);
        if (!pipeline)
        {
            LOG_ERROR("Failed to create pipeline");
            return false;
        }

        return true;
    }

    void LineManager::AddLine(const Vector3f& start, const Vector3f& end, const Color4f& color)
    {
        lines.Add(LineInfo(start, end, color));
        need_update = true;
    }

    void LineManager::Clear()
    {
        lines.Clear();
        need_update = true;
    }

    bool LineManager::Update()
    {
        if (!need_update)
            return true;

        if (lines.GetCount() == 0)
        {
            // 清理现有资源
            SAFE_CLEAR(mesh);
            SAFE_CLEAR(primitive);
            need_update = false;
            return true;
        }

        return UpdatePrimitive();
    }

    bool LineManager::UpdatePrimitive()
    {
        if (!line_material)
        {
            LOG_ERROR("Line material not initialized");
            return false;
        }

        // 清理旧资源
        SAFE_CLEAR(mesh);
        SAFE_CLEAR(primitive);

        const uint32_t line_count = lines.GetCount();
        if (line_count == 0)
            return true;

        // 创建图元创建器
        const VIL* vil = line_material->GetDefaultVIL();
        if (!vil)
        {
            LOG_ERROR("Failed to get VIL from material");
            return false;
        }

        PrimitiveCreater pc(render_framework->GetDevice(), vil);
        
        // 初始化图元创建器 - 每条线对应一个点（几何着色器会将点转换为线）
        if (!pc.Init("LineManager_Lines", line_count))
        {
            LOG_ERROR("Failed to initialize PrimitiveCreater");
            return false;
        }

        // 准备顶点数据
        auto start_positions = new Vector3f[line_count];
        auto end_positions = new Vector3f[line_count];
        auto colors = new Color4f[line_count];

        if (!start_positions || !end_positions || !colors)
        {
            LOG_ERROR("Failed to allocate vertex data memory");
            delete[] start_positions;
            delete[] end_positions;
            delete[] colors;
            return false;
        }

        // 填充顶点数据
        for (uint32_t i = 0; i < line_count; ++i)
        {
            const LineInfo& line = lines[i];
            start_positions[i] = line.startPos;
            end_positions[i] = line.endPos;
            colors[i] = line.color;
        }

        // 写入顶点属性缓冲区
        bool success = true;
        success &= pc.WriteVAB(VAN::Position, VF_V3F, start_positions);  // Position用起点
        success &= pc.WriteVAB(VAN::StartPosition, VF_V3F, start_positions);
        success &= pc.WriteVAB(VAN::EndPosition, VF_V3F, end_positions);
        success &= pc.WriteVAB(VAN::Color, VF_V4F, colors);

        // 清理临时内存
        delete[] start_positions;
        delete[] end_positions;
        delete[] colors;

        if (!success)
        {
            LOG_ERROR("Failed to write vertex attribute data");
            return false;
        }

        // 创建图元
        primitive = pc.Create();
        if (!primitive)
        {
            LOG_ERROR("Failed to create primitive");
            return false;
        }

        // 创建网格
        mesh = CreateMesh(primitive, material_instance, pipeline);
        if (!mesh)
        {
            LOG_ERROR("Failed to create mesh");
            return false;
        }

        need_update = false;
        return true;
    }

    void LineManager::CleanupResources()
    {
        SAFE_CLEAR(mesh);
        SAFE_CLEAR(primitive);
        SAFE_CLEAR(pipeline);
        SAFE_CLEAR(material_instance);
        SAFE_CLEAR(line_material);
    }
}