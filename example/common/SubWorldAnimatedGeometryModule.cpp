#include"SubWorldAnimatedGeometryModule.h"
#include"SubWorldModuleBase.h"

#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/module/MaterialAssetRegistry.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/color/Color.h>

#include<hgl/ecs/core/System.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>

#include<hgl/vk/pipeline/VKGraphicsPipelinePreset.h>
#include<hgl/vk/VKMaterial.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

using namespace hgl;
using namespace hgl::ecs;
using namespace hgl::graph;

namespace
{
    class SubWorldPulseSystem final : public System
    {
    private:
        float elapsed = 0.0f;

    public:
        SubWorldPulseSystem()
            : System("SubWorldPulseSystem")
        {
            SetSystemType(SystemType::Animation);
            SetExecutionOrder(ExecutionPhase::TickTransform);
        }

        void Update(float delta_time) override
        {
            elapsed += delta_time;

            if (!context)
                return;

            std::vector<std::shared_ptr<TransformComponent>> transforms;
            context->GetComponents(transforms);

            size_t idx = 0;
            for (const auto& tr : transforms)
            {
                if (!tr || !tr->IsMovable())
                    continue;

                const float phase = elapsed * 1.2f + static_cast<float>(idx) * 0.35f;
                const float yaw = phase;
                const float pitch = 0.35f * sinf(phase * 0.7f);
                const float pulse = 1.0f + 0.22f * sinf(phase * 1.8f);

                const glm::quat qy = glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f));
                const glm::quat qx = glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f));

                tr->SetLocalRotation(qy * qx);
                tr->SetLocalScale(glm::vec3(pulse, pulse, pulse));
                ++idx;
            }
        }
    };

    class SubWorldAnimatedGeometryModuleImpl final : public example::modules::SubWorldModuleBase
    {
    protected:
        bool OnInitializeSharedResources() override
        {
            static const mtl::MaterialAssetRecord kAnimGeomCfg {
                .id       = "subworld_anim_geom",
                .preset   = mtl::MaterialPreset::Gizmo3D,
                .pipeline = GraphicsPipelinePreset::Solid3D,
            };
            const Color4f colors[] =
            {
                GetColor4f(COLOR::BlenderAxisRed, 1.0f),
                GetColor4f(COLOR::BlenderAxisGreen, 1.0f),
                GetColor4f(COLOR::SkyBlue, 1.0f)
            };

            {
                auto* seed_mi = AcquireMI(kAnimGeomCfg, &colors[0], sizeof(colors[0]));
                if (!seed_mi) return false;
                material        = seed_mi->GetMaterial();
                material_domain = seed_mi->GetDomain();
            }

            return BuildMaterialInstances(colors, sizeof(colors) / sizeof(colors[0]));
        }

        bool OnInstallLocalSystems(ECSContext* sub_context) override
        {
            auto pulse_system = sub_context->RegisterTickSystemScoped<SubWorldPulseSystem>(
                ECSContext::SystemOwnershipScope::LocalIsolated);

            return pulse_system != nullptr;
        }

        bool OnBuildLocalScene(ECSContext* sub_context) override
        {
            if (!render_context || !graphics_context || !sub_context || material_instances.size() < 3)
                return false;

            auto* device = graphics_context->GetDevice();
            if (!device)
                return false;

            using namespace inline_geometry;
            auto pc = std::make_unique<GeometryCreater>(device, material->GetDefaultVIL());
            if (!pc)
                return false;

            CubeCreateInfo cube_ci;
            cube_ci.segments_x = 1;
            cube_ci.segments_y = 1;
            cube_ci.segments_z = 1;

            auto* cube_geom = CreateCube(pc.get(), &cube_ci);
            auto* sphere_geom = CreateSphere(pc.get(), 32);

            CylinderCreateInfo cyl_ci;
            cyl_ci.halfExtend = 0.7f;
            cyl_ci.radius = 0.45f;
            cyl_ci.numberSlices = 32;
            auto* cyl_geom = CreateCylinder(pc.get(), &cyl_ci);

            auto* cube_mesh = CreatePrimitiveMesh(cube_geom, material_instances[0]);
            auto* sphere_mesh = CreatePrimitiveMesh(sphere_geom, material_instances[1]);
            auto* cyl_mesh = CreatePrimitiveMesh(cyl_geom, material_instances[2]);

            if (!cube_mesh || !sphere_mesh || !cyl_mesh)
                return false;

            struct SpawnInfo
            {
                const char* name;
                MeshResource* mesh;
                glm::vec3 pos;
            };

            const SpawnInfo infos[] =
            {
                {"Sub_Cube", cube_mesh, glm::vec3(-2.2f, 0.0f, 0.0f)},
                {"Sub_Sphere", sphere_mesh, glm::vec3(0.0f, 0.0f, 0.0f)},
                {"Sub_Cylinder", cyl_mesh, glm::vec3(2.2f, 0.0f, 0.0f)}
            };

            for (const auto& info : infos)
            {
                Entity* e = sub_context->CreateEntity<Entity>(info.name);
                auto tr = e->AddComponent<TransformComponent>(Mobility::Movable);
                auto prim = e->AddComponent<PrimitiveComponent>();

                tr->SetLocalPosition(info.pos);
                tr->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
                tr->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
                tr->SetMovable(true);

                prim->SetPrimitive(info.mesh->primitive);
                prim->SetVisible(true);
            }

            return true;
        }
    };
}

namespace example::modules
{
    std::unique_ptr<ISubWorldModule> CreateSubWorldAnimatedGeometryModule()
    {
        return std::make_unique<SubWorldAnimatedGeometryModuleImpl>();
    }
}

