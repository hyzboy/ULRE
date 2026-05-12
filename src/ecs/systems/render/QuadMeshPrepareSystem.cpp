#include<hgl/ecs/systems/render/QuadMeshPrepareSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/QuadMeshComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/vk/VKFormat.h>
#include<hgl/vk/VertexAttrib.h>
#include<vector>
#include<memory>

namespace hgl::ecs
{
    namespace
    {
        graph::Geometry *CreateQuadMeshGeometry(graph::VulkanDevice *device,
                                                const QuadMeshComponent &quad_mesh)
        {
            if (!device)
                return nullptr;

            const glm::vec2 size = quad_mesh.GetSize();
            const glm::vec2 pivot = quad_mesh.GetPivot();
            const glm::vec4 uv = quad_mesh.GetUVRect();
            const VkFrontFace front_face = quad_mesh.GetFrontFace();

            const float left = -pivot.x * size.x;
            const float right = left + size.x;
            const float top = -pivot.y * size.y;
            const float bottom = top + size.y;

            float position_data[12] =
            {
                left,  top,    0.0f,
                right, top,    0.0f,
                right, bottom, 0.0f,
                left,  bottom, 0.0f,
            };

            const float uv_data[8] =
            {
                uv.x, uv.y,
                uv.z, uv.y,
                uv.z, uv.w,
                uv.x, uv.w,
            };

            const float normal_data[12] =
            {
                0.0f, 0.0f, 1.0f,
                0.0f, 0.0f, 1.0f,
                0.0f, 0.0f, 1.0f,
                0.0f, 0.0f, 1.0f,
            };

            const uint16_t clockwise_indices[6] = { 0, 1, 2, 0, 2, 3 };
            const uint16_t counter_clockwise_indices[6] = { 0, 2, 1, 0, 3, 2 };
            const uint16_t *index_data = front_face == VK_FRONT_FACE_COUNTER_CLOCKWISE
                                       ? counter_clockwise_indices
                                       : clockwise_indices;

            graph::GeometryVertexFormat gvf;
            gvf.Set(graph::VAN::Position, VF_V3F);
            gvf.Set(graph::VAN::TexCoord, VF_V2F);
            gvf.Set(graph::VAN::Normal, VF_V3F);

            auto pc = std::make_unique<graph::GeometryCreater>(device, gvf);
            if (!pc->Init("QuadMesh", 4, 6, graph::IndexType::U16))
                return nullptr;

            if (!pc->WriteVAB(graph::VAN::Position, VF_V3F, position_data))
                return nullptr;

            if (!pc->WriteVAB(graph::VAN::TexCoord, VF_V2F, uv_data))
                return nullptr;

            if (!pc->WriteVAB(graph::VAN::Normal, VF_V3F, normal_data))
                return nullptr;

            if (!pc->WriteIBO(index_data))
                return nullptr;

            return pc->CreateWithAABB(
                math::Vector3f(left, top, 0.0f),
                math::Vector3f(right, bottom, 0.0f));
        }
    }

    QuadMeshPrepareSystem::QuadMeshPrepareSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::ShaderMaterialProgram);
        SetExecutionOrder(ExecutionPhase::RenderMaterialBind);
        SetRenderElementType("Primitive");
    }

    void QuadMeshPrepareSystem::Update(float)
    {
        if (!world)
            return;

        auto *device = world->GetGPUDevice();
        if (!device)
            return;

        std::vector<std::shared_ptr<QuadMeshComponent>> quad_meshes;
        world->GetComponents<QuadMeshComponent>(quad_meshes);

        for (const auto &quad_mesh : quad_meshes)
        {
            if (!quad_mesh)
                continue;

            Entity *entity = quad_mesh->GetOwner();
            if (!entity)
                continue;

            auto primitive = entity->GetComponent<PrimitiveComponent>();
            if (!primitive)
                continue;

            if (!quad_mesh->IsGeometryDirty())
            {
                if (primitive->GetUnresolvedGeometry() || primitive->GetPrimitive())
                    continue;
            }

            graph::Geometry *geometry = CreateQuadMeshGeometry(device, *quad_mesh);
            if (!geometry)
                continue;

            if (auto *old_geometry = primitive->GetUnresolvedGeometry())
                delete old_geometry;

            primitive->SetUnresolvedGeometry(geometry);
            quad_mesh->ClearGeometryDirty();
        }
    }
}//namespace hgl::ecs
