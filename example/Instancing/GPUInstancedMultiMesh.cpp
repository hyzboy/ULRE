// GPUInstancedMultiMesh - one PBR recipe and data space across multiple geometries.
// Entities may use different built-in meshes, but they share one shader, one
// material recipe, and per-entity PBRSurface data rows.
// GIMM_GPU_CHECK=1 with GIMM_FRAMES>=14 validates the ten GPU mesh buckets.

#include <hgl/framework/WorkManager.h>
#include <hgl/graph/geo/VertexDataManager.h>
#include <hgl/graph/geo/GeometryCreater.h>
#include <hgl/graph/asset/PrimitiveAsset.h>
#include <hgl/graph/asset/MaterialDataBlockAsset.h>
#include <hgl/graph/module/GeometryManager.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include <hgl/mtl/MaterialResolver.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/color/Color.h>
#include <hgl/log/Log.h>
#include <hgl/type/Smart.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/entity/Entity.h>
#include <hgl/ecs/support/pipeline/RenderableBuilder.h>
#include <hgl/ecs/components/CameraControlMode.h>
#include <hgl/ecs/systems/tick/CameraSystem.h>
#include <hgl/vk/VKRenderTargetSwapchain.h>
#include "../Shared/ExampleAppBase.h"
#include "../Shared/InstancingReadback.h"

#include <cstdlib>

#include <glm/glm.hpp>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    constexpr uint GRID_SIZE = 10;
    constexpr uint MESH_VARIANT_COUNT = 10;
    constexpr float SPACING = 2.8f;
    constexpr const char * DATA_PATH =
        "mtldata:pack(ShaderLibrary):asset/material_data/pbr_matrix_10x10";

    static const char * MESH_URI[MESH_VARIANT_COUNT] =
    {
        "geo:builtin:sphere_64",
        "geo:builtin:dome_64",
        "geo:builtin:cone",
        "geo:builtin:cylinder",
        "geo:builtin:torus",
        "geo:builtin:hollow_cylinder",
        "geo:builtin:hex_sphere",
        "geo:builtin:capsule",
        "geo:builtin:tapered_capsule_wide",
        "geo:builtin:cube",
    };
}

class GPUInstancedMultiMeshApp final : public ulre_example::ExampleAppBase
{
    ECSContext *world = nullptr;
    Entity *camera_entity = nullptr;
    hgl::AutoDelete<VertexDataManager> mesh_vdm;
    Geometry *mesh_variants[MESH_VARIANT_COUNT]{};
    PrimitiveAsset mesh_primitives[MESH_VARIANT_COUNT]{};
    graph::mtl::MaterialRecipe recipe{};
    graph::mtl::MaterialDataBlockAsset *data_block = nullptr;
    bool gpu_check = false;
    bool gpu_check_scheduled = false;
    bool gpu_check_completed = false;

public:
    GPUInstancedMultiMeshApp()
    {
        SetupExampleConfig({
            "GIMM",
            "gimmdump",
            "GIMMSELFCHECK",
            8,
            8,
            { 1280, 720, 0, 2, 0x5e9ab7c2114df680ull }
        });
    }

    ~GPUInstancedMultiMeshApp() override
    {
        if (auto *registry = GetGeometryAssetRegistry())
            registry->Clear();

    }

private:
    bool InitRecipe()
    {
        const graph::mtl::MaterialRecipe *loaded =
            GetRecipe("rcp:pack(ShaderLibrary):asset/recipe/Examples/Material/PBRMatrix.Lit");
        if (!loaded)
        {
            GLogError(u8"GPUInstancedMultiMesh: PBRMatrix.Lit load failed");
            return false;
        }

        recipe = *loaded;
        return true;
    }

    bool InitGeometry()
    {
        auto *buffer_manager = GetManager<BufferManager>();
        if (!buffer_manager)
            return false;

        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2F},
            {VertexSemantic::Normal,   VF_V3F},
        };

        mesh_vdm = new VertexDataManager(buffer_manager, gvf);
        if (!mesh_vdm || !mesh_vdm->Init(HGL_SIZE_1MB, HGL_SIZE_1MB, IndexType::U32))
            return false;

        GeometryCreater pc(mesh_vdm);
        SharedCreaterScope scope(GetGeometryAssetRegistry(), &pc);

        for (uint i = 0; i < MESH_VARIANT_COUNT; ++i)
        {
            mesh_variants[i] = GetGeometry(MESH_URI[i]);
            if (!mesh_variants[i])
            {
                GLogError(u8"GPUInstancedMultiMesh: geometry unavailable %u", i);
                return false;
            }
        }

        return true;
    }

    bool InitDataBlock()
    {
        auto *graphics = GetGraphicsContext();
        auto *pool = GetManager<GlobalSSBOBufferRegistry>();
        if (!graphics || !pool)
            return false;

        data_block = graphics->GetMaterialDataBlockRegistry()->GetOrLoadURI(ToOSString(DATA_PATH));
        if (!data_block || !data_block->Upload(pool))
        {
            GLogError(u8"GPUInstancedMultiMesh: data block upload failed");
            return false;
        }

        if (data_block->GetRowCount() != GRID_SIZE * GRID_SIZE)
        {
            GLogError(u8"GPUInstancedMultiMesh: expected %u rows, got %u",
                      GRID_SIZE * GRID_SIZE, data_block->GetRowCount());
            return false;
        }

        const graph::GlobalSSBOBinding block_binding{
            data_block->GetGlobalSSBOType(),
            data_block->GetSSBOId(),
            data_block->RowIDAt(0)};

        if (!(recipe.material_ssbo_binding = block_binding).IsValid())
            return false;

        for (uint i = 0; i < MESH_VARIANT_COUNT; ++i)
        {
            mesh_primitives[i] = PrimitiveAsset(
                mesh_variants[i], &recipe, PrimitiveType::Triangles);
        }

        return true;
    }

    bool InitWorld()
    {
        world = GetECSContext();
        if (!world || !InitDataBlock())
            return false;

        const float offset = (GRID_SIZE - 1) * SPACING * 0.5f;

        for (uint row = 0; row < GRID_SIZE; ++row)
        {
            for (uint col = 0; col < GRID_SIZE; ++col)
            {
                const uint mesh_index = col;
                const AnsiString name =
                    AnsiString("InstancedMultiMesh_") +
                    AnsiString::numberOf(row * GRID_SIZE + col);

                Entity *entity = RenderableBuilder(world, name.c_str())
                    .WithTransform(glm::vec3(col * SPACING - offset,
                                             row * SPACING - offset,
                                             0.0f),
                                   glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                   glm::vec3(1.0f),
                                   Mobility::Static)
                    .WithPrimitiveAsset(&mesh_primitives[mesh_index])
                    .WithMaterialDataRow(ToOSString(DATA_PATH),
                                         row * GRID_SIZE + col)
                    .Build();

                if (!entity)
                {
                    GLogError(u8"GPUInstancedMultiMesh: entity build failed (%u,%u)", row, col);
                    return false;
                }
            }
        }

        return true;
    }

    bool InitCamera()
    {
        if (!world->EnsureCameraSystem())
            return false;

        camera_entity = world->CreateEntity<Entity>("MainCamera");
        auto *camera = world->GetOrCreateCamera(camera_entity);

        camera->control_mode = CameraControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance = 46.0f;
        camera->yaw = 0.0f;
        camera->pitch = -25.0f;

        if (const char *pitch = getenv("GIMM_PITCH"))
            camera->pitch = (float)atof(pitch);
        if (const char *yaw = getenv("GIMM_YAW"))
            camera->yaw = (float)atof(yaw);
        if (const char *distance = getenv("GIMM_DIST"))
            camera->distance = (float)atof(distance);

        camera->is_main_camera = true;
        camera->matrix_dirty = true;
        camera->viewport_info = GetViewportInfo();
        return true;
    }

public:
    bool OnInit() override
    {
        gpu_check = getenv("GIMM_GPU_CHECK") != nullptr;
        if (gpu_check && example_perf_frames < 14u)
        {
            GLogError(u8"[GIMM] GPU check requires GIMM_FRAMES >= 14");
            return false;
        }
        SetClearColor(Color4f(0.08f, 0.08f, 0.08f, 1.0f));

        if (!InitRecipe())
            return false;
        if (!InitGeometry())
            return false;
        if (!InitWorld())
            return false;
        return InitCamera();
    }

    void OnTick(double) override
    {
        if (gpu_check && example_frame_seen >= example_perf_frames && !gpu_check_completed)
        {
            GLogError(u8"[GIMM] GPU check did not complete before frame limit");
            exit(1);
        }
        if (!gpu_check || gpu_check_scheduled || example_frame_seen < 12u)
            return;

        auto *target = dynamic_cast<SwapchainRenderTarget *>(world->GetRenderTarget());
        if (!target)
        {
            GLogError(u8"[GIMM] GPU check requires a swapchain");
            exit(1);
        }
        gpu_check_scheduled = true;
        target->SetInFrameReadbackHook([this]()
        {
            if (!ulre_example::CheckInstancingRows(world, data_block,
                                                  ulre_example::InstancingGroups::MeshColumns, "GIMM"))
                exit(1);
            gpu_check_completed = true;
            GLogInfo(u8"[GIMM] GPU instance row validation passed");
        });
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<GPUInstancedMultiMeshApp>(
        OS_TEXT("GPU Instanced Multi-Mesh - shared PBR recipe"), argc, argv, 1280, 720);
}
