// GPUInstancedDataRows - pure data-row instancing benchmark.
// 100 entities share one primitive, one recipe, one shader, and one geometry.
// Each entity changes only its PBRSurface material data row.

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

#include <cstdlib>

#include <glm/glm.hpp>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    constexpr uint GRID_SIZE = 10;
    constexpr float SPACING = 2.5f;
    constexpr const char * DATA_PATH =
        "mtldata:pack(ShaderLibrary):asset/material_data/pbr_matrix_10x10";
}

class GPUInstancedDataRowsApp final : public ulre_example::ExampleAppBase
{
    ECSContext *world = nullptr;
    Entity *camera_entity = nullptr;
    hgl::AutoDelete<VertexDataManager> mesh_vdm;
    Geometry *sphere_geometry = nullptr;
    PrimitiveAsset sphere_primitive{};
    graph::mtl::MaterialRecipe recipe{};
    graph::mtl::MaterialDataBlockAsset *data_block = nullptr;

public:
    GPUInstancedDataRowsApp()
    {
        SetupExampleConfig({
            "GID",
            "giddump",
            "GIDSELFCHECK",
            8,
            8,
            { 1280, 720, 0, 2, 0x1c55e0a3d04a7b91ull }
        });
    }

    ~GPUInstancedDataRowsApp() override
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
            GLogError(u8"GPUInstancedDataRows: PBRMatrix.Lit load failed");
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

        sphere_geometry = GetGeometry("geo:builtin:sphere_64");
        if (!sphere_geometry)
        {
            GLogError(u8"GPUInstancedDataRows: sphere geometry unavailable");
            return false;
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
            GLogError(u8"GPUInstancedDataRows: data block upload failed");
            return false;
        }

        if (data_block->GetRowCount() != GRID_SIZE * GRID_SIZE)
        {
            GLogError(u8"GPUInstancedDataRows: expected %u rows, got %u",
                      GRID_SIZE * GRID_SIZE, data_block->GetRowCount());
            return false;
        }

        const graph::GlobalSSBOBinding block_binding{
            data_block->GetGlobalSSBOType(),
            data_block->GetSSBOId(),
            data_block->RowIDAt(0)};

        if (!(recipe.material_ssbo_binding = block_binding).IsValid())
            return false;

        return true;
    }

    bool InitWorld()
    {
        world = GetECSContext();
        if (!world || !InitDataBlock())
            return false;

        sphere_primitive = PrimitiveAsset(sphere_geometry, &recipe, PrimitiveType::Triangles);
        const float offset = (GRID_SIZE - 1) * SPACING * 0.5f;

        for (uint row = 0; row < GRID_SIZE; ++row)
        {
            for (uint col = 0; col < GRID_SIZE; ++col)
            {
                const AnsiString name =
                    AnsiString("InstancedSphere_") + AnsiString::numberOf(row * GRID_SIZE + col);

                Entity *entity = RenderableBuilder(world, name.c_str())
                    .WithTransform(glm::vec3(col * SPACING - offset,
                                             row * SPACING - offset,
                                             0.0f),
                                   glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                                   glm::vec3(1.0f),
                                   Mobility::Static)
                    .WithPrimitiveAsset(&sphere_primitive)
                    .WithMaterialDataRow(ToOSString(DATA_PATH),
                                         row * GRID_SIZE + col)
                    .Build();

                if (!entity)
                {
                    GLogError(u8"GPUInstancedDataRows: entity build failed (%u,%u)", row, col);
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
        camera->distance = 40.0f;
        camera->yaw = 0.0f;
        camera->pitch = -25.0f;

        if (const char *pitch = getenv("GID_PITCH"))
            camera->pitch = (float)atof(pitch);
        if (const char *yaw = getenv("GID_YAW"))
            camera->yaw = (float)atof(yaw);
        if (const char *distance = getenv("GID_DIST"))
            camera->distance = (float)atof(distance);

        camera->is_main_camera = true;
        camera->matrix_dirty = true;
        camera->viewport_info = GetViewportInfo();
        return true;
    }

public:
    bool OnInit() override
    {
        SetClearColor(Color4f(0.08f, 0.08f, 0.08f, 1.0f));

        if (!InitRecipe())
            return false;
        if (!InitGeometry())
            return false;
        if (!InitWorld())
            return false;
        return InitCamera();
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<GPUInstancedDataRowsApp>(
        OS_TEXT("GPU Instanced Data Rows - one mesh and recipe"), argc, argv, 1280, 720);
}
