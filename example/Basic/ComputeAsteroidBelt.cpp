// ComputeAsteroidBelt.cpp
//
// 1,000,000 Asteroids Mega Planetary Ring (Saturn-like Ring System)
// 
// 特性：
// 1. 中央巨型主星（Planet Sphere，半径 35.0）
// 2. 1,000,000 颗陨星环绕轨道构成的行星环（半径 70.0 ~ 220.0）
// 3. 涵盖 PBRSpheres 中的全部 10 种内建几何体（Sphere, Dome, Cone, Cylinder, Torus, HollowCylinder, HexSphere, Capsule, TaperedCapsule, Cube），每种各 100,000 颗
// 4. 10 种几何体对应 10 种独特的太空矿物色谱（通过 EmissiveSurfaceRow SSBO 区分）
// 5. GPU-Driven 全 Compute Shader 模拟：
//    - 开普勒轨道自转差速模拟（Keplerian differential rotation: 内圈快、外圈慢）
//    - 3D 多重正弦叠加波动（径向脉动 + 垂向螺旋涟漪波）
//    - 陨星各向异性三维翻滚自转
//    - 6 平面硬件视锥体剔除（Vulkan RH_ZO 语义投影半径包围盒）
//    - 无锁原子紧凑分类与 Indirect Commands 生成
// 6. 100% GPU-Driven 渲染管线挂载：通过 MaterialBatch::gpu_driven_override 零 CPU 负荷

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/ShaderProgramManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/graph/ShaderBufferSources.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/MaterialDefinitionRegistry.h>
#include<hgl/color/Color.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/core/RenderItem.h>
#include<hgl/ecs/core/MaterialBatch.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/InstancedPrimitiveComponent.h>
#include<hgl/ecs/support/DrawItemIDStorage.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/buffer/DeviceBuffer.h>
#include<hgl/vk/buffer/IndirectCommandBuffer.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/type/Constants.h>
#include<hgl/time/Time.h>
#include<hgl/log/Log.h>

#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/quaternion.hpp>
#include<math.h>
#include<stdlib.h>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::graph::mtl;
using namespace hgl::ecs;

namespace
{
    constexpr uint32_t GEOMETRY_VARIANT_COUNT = 10;
    constexpr uint32_t INSTANCES_PER_GEOM     = 100000;
    constexpr uint32_t TOTAL_ASTEROIDS        = GEOMETRY_VARIANT_COUNT * INSTANCES_PER_GEOM; // 1,000,000

    constexpr float    RING_INNER_RADIUS      = 70.0f;
    constexpr float    RING_OUTER_RADIUS      = 220.0f;
    constexpr float    PLANET_RADIUS          = 35.0f;

    struct AsteroidOrbitParam
    {
        float base_radius;
        float base_angle;
        float base_height;
        float orbit_speed;

        glm::vec4 spin_axis; // xyz = normalized axis, w = spin_speed

        float scale;
        float wave_amp;
        float wave_freq;
        float wave_phase;
    };

    struct SimulationPushConstants
    {
        glm::vec4 frustum_planes[6];
        float     time;
        uint32_t  total_asteroids;
    };

    struct FinalizePushConstants
    {
        uint32_t mesh_group_counts[GEOMETRY_VARIANT_COUNT];
    };

    struct DrawCountData
    {
        uint32_t total_visible;
        uint32_t geom_visible[GEOMETRY_VARIANT_COUNT];
    };

    // ── 计算着色器 1：开普勒轨道 + 3D正弦波 + 翻滚自转 + 视锥剔除与紧凑输出 ──
    constexpr char COMPUTE_SIM_CULL_GLSL[] = R"(#version 460
#extension GL_EXT_scalar_block_layout : require

struct AsteroidOrbitParam {
    float base_radius;
    float base_angle;
    float base_height;
    float orbit_speed;

    vec4 spin_axis; // xyz = axis, w = spin_speed

    float scale;
    float wave_amp;
    float wave_freq;
    float wave_phase;
};

struct GeometryAABB {
    vec4 center;
    vec4 extents;
};

layout(push_constant) uniform SimPushConstants {
    vec4 frustum_planes[6];
    float time;
    uint total_asteroids;
} pc;

layout(std430, set = 2, binding = 0) readonly buffer OrbitParamsBuf {
    AsteroidOrbitParam orbit_params[];
};

layout(std430, set = 2, binding = 1) readonly buffer GeometryAABBsBuf {
    GeometryAABB geometry_aabbs[];
};

layout(std430, set = 2, binding = 2) writeonly buffer WorldMatsBuf {
    mat4 world_matrices[];
};

layout(std430, set = 2, binding = 3) buffer L2WIndexBuf {
    uint l2w_indices[];
};

layout(std430, set = 2, binding = 4) buffer CountsBuf {
    uint total_visible;
    uint geom_visible[10];
};

mat3 RotationAxis(vec3 axis, float angle) {
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;
    return mat3(
        oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,
        oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,
        oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c
    );
}

layout(local_size_x = 256) in;

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= pc.total_asteroids)
        return;

    AsteroidOrbitParam p = orbit_params[idx];

    // 1. 开普勒差速环绕 + 径向正弦微波
    float angle = p.base_angle + p.orbit_speed * pc.time;
    float r = p.base_radius + p.wave_amp * 0.15 * sin(p.wave_freq * 0.5 * p.base_radius + pc.time);
    float x = r * cos(angle);
    float y = r * sin(angle);

    // 2. 纵向多频波动 (螺旋密度波与涟漪)
    float z = p.base_height +
              p.wave_amp * sin(angle * 3.0 + p.wave_phase + pc.time * 1.5) +
              (p.wave_amp * 0.5) * cos(r * 0.08 - pc.time * 2.0);

    // 3. 陨星各向异性三维自转
    float spin_angle = p.spin_axis.w * pc.time;
    mat3 R = RotationAxis(p.spin_axis.xyz, spin_angle);

    mat4 l2w = mat4(
        vec4(R[0] * p.scale, 0.0),
        vec4(R[1] * p.scale, 0.0),
        vec4(R[2] * p.scale, 0.0),
        vec4(x, y, z, 1.0)
    );

    world_matrices[idx] = l2w;

    // 4. 6 平面视锥体剔除 (Vulkan RH_ZO 语义)
    uint geom_id = idx / 100000;
    GeometryAABB aabb = geometry_aabbs[geom_id];
    vec3 world_center = (l2w * vec4(aabb.center.xyz, 1.0)).xyz;
    mat3 m = mat3(l2w);
    vec3 extents = aabb.extents.xyz;

    bool visible = true;
    for (int i = 0; i < 6; ++i) {
        vec3 n = pc.frustum_planes[i].xyz;
        float d = pc.frustum_planes[i].w;
        float pr = dot(extents, abs(n * m));
        float dist = dot(n, world_center) + d;
        if (dist < -pr) {
            visible = false;
            break;
        }
    }

    if (visible) {
        atomicAdd(total_visible, 1);
        uint slot = atomicAdd(geom_visible[geom_id], 1);
        l2w_indices[geom_id * 100000 + slot] = idx;
    }
}
)";

    // ── 计算着色器 2：间接命令动态生成 (Finalize Indirect Commands) ──
    constexpr char COMPUTE_FINALIZE_CMDS_GLSL[] = R"(#version 460
#extension GL_EXT_scalar_block_layout : require

struct DrawMeshTasksIndirectCommand {
    uint groupCountX;
    uint groupCountY;
    uint groupCountZ;
};

layout(push_constant) uniform FinalizePushConstants {
    uint mesh_group_counts[10];
} pc;

layout(std430, set = 2, binding = 0) readonly buffer CountsBuf {
    uint total_visible;
    uint geom_visible[10];
};

layout(std430, set = 2, binding = 1) writeonly buffer IndirectCmdsBuf {
    DrawMeshTasksIndirectCommand indirect_commands[];
};

layout(local_size_x = 10) in;

void main() {
    uint g = gl_LocalInvocationID.x;
    if (g >= 10) return;

    indirect_commands[g].groupCountX = pc.mesh_group_counts[g];
    indirect_commands[g].groupCountY = geom_visible[g];
    indirect_commands[g].groupCountZ = 1;
}
)";

    GeometryVertexFormat CreateGizmo3DGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::Normal,   VF_V3F},
        };
        return gvf;
    }
} // namespace

class ComputeAsteroidBeltApp : public WorkObject
{
    VertexDataManager        *mesh_vdm = nullptr;

    // 主星 (Planet)
    Geometry                 *planet_geometry = nullptr;
    GlobalSSBODataAccessor  planet_mtl_accessor{};
    MaterialRecipe            planet_recipe{};
    PrimitiveAsset            planet_asset{};
    Entity                   *planet_entity   = nullptr;

    // 10 种内建几何体
    Geometry                 *builtin_geometries[GEOMETRY_VARIANT_COUNT]{};
    uint32_t                  geometry_ids[GEOMETRY_VARIANT_COUNT]{};
    uint32_t                  mesh_group_counts[GEOMETRY_VARIANT_COUNT]{};
    GeometryAABB              cpu_geometry_aabbs[GEOMETRY_VARIANT_COUNT]{};
    PrimitiveAsset            asteroid_assets[GEOMETRY_VARIANT_COUNT]{};

    // 陨星材质（使用 EmissiveSurface 彩色太空矿石表）
    GlobalSSBODataAccessor  asteroid_mtl_accessors[GEOMETRY_VARIANT_COUNT]{};
    uint32_t                  mineral_payload_indices[GEOMETRY_VARIANT_COUNT]{};
    MaterialRecipe            asteroid_recipe{};
    ShaderProgram            *asteroid_shader_program = nullptr;

    // ECS 节点（11 个实体：1 个主星 + 10 个几何体批次代表）
    ECSContext               *ecs_context   = nullptr;
    Entity                   *camera_entity = nullptr;
    Entity                   *asteroid_entities[GEOMETRY_VARIANT_COUNT]{};

    // GPU Compute 资源
    DeviceBuffer             *orbit_params_buffer       = nullptr; // 48 MB
    DeviceBuffer             *geometry_aabbs_buffer     = nullptr; // 320 B
    DeviceBuffer             *world_matrices_buffer     = nullptr; // 64 MB (Storage | BDA)
    DeviceBuffer             *l2w_index_buffer          = nullptr; // 4 MB (Storage | BDA)
    DeviceBuffer             *draw_count_buffer         = nullptr; // 44 B
    DeviceBuffer             *mesh_draw_params_buffer   = nullptr; // 80 B (Storage | BDA)
    DeviceBuffer             *material_data_rows_buffer = nullptr; // 8 MB (Storage | BDA)
    IndirectMeshTaskBuffer   *indirect_cmds_buffer      = nullptr; // 120 B (Storage | Indirect)
    DeviceBuffer             *readback_count_buffer     = nullptr; // 44 B (CPUVisible)

    // Compute Pipeline 1 (Sim + Cull)
    VkDescriptorSetLayout     sim_layout       = VK_NULL_HANDLE;
    VkDescriptorSet           sim_set          = VK_NULL_HANDLE;
    ComputePipeline          *sim_pipeline     = nullptr;

    // Compute Pipeline 2 (Finalize Indirect Cmds)
    VkDescriptorSetLayout     finalize_layout  = VK_NULL_HANDLE;
    VkDescriptorSet           finalize_set     = VK_NULL_HANDLE;
    ComputePipeline          *finalize_pipeline= nullptr;

    VkDescriptorPool          desc_pool        = VK_NULL_HANDLE;
    ComputeCmdBuffer         *compute_cmd      = nullptr;
    DeviceQueue              *compute_queue    = nullptr;

    float    camera_angle     = 0.0f;
    float    elapsed_time     = 0.0f;
    uint64_t tick_frame_count = 0;

    double   last_stat_time   = 0.0;
    uint64_t last_stat_frame  = 0;

private:

    bool InitVDM()
    {
        auto *buffer_manager = GetManager<BufferManager>();
        if (!buffer_manager)
            return false;

        mesh_vdm = new VertexDataManager(buffer_manager, CreateGizmo3DGeometryVertexFormat());
        if (!mesh_vdm)
            return false;

        return mesh_vdm->Init(2 * HGL_SIZE_1MB, 2 * HGL_SIZE_1MB, IndexType::U32);
    }

    bool CreateGeometries()
    {
        using namespace inline_geometry;

        auto create_geometry = [this](auto &&creator) -> Geometry *
        {
            GeometryCreater pc(mesh_vdm);
            return creator(&pc);
        };

        // 1. 创建主星巨型球体 (半径 35.0)
        planet_geometry = create_geometry([](GeometryCreater *pc)
        {
            return CreateSphere(pc, 96);
        });
        if (!planet_geometry)
            return false;

        // 2. 创建 10 种代表陨星群的内建几何体 (来自 PBRSpheres)
        builtin_geometries[0] = create_geometry([](GeometryCreater *pc)
        {
            return CreateSphere(pc, 48);
        });

        builtin_geometries[1] = create_geometry([](GeometryCreater *pc)
        {
            return CreateDome(pc, 48);
        });

        builtin_geometries[2] = create_geometry([](GeometryCreater *pc)
        {
            ConeCreateInfo cci;
            cci.radius       = 1.0f;
            cci.halfExtend   = 1.0f;
            cci.numberSlices = 48;
            cci.numberStacks = 4;
            return CreateCone(pc, &cci);
        });

        builtin_geometries[3] = create_geometry([](GeometryCreater *pc)
        {
            CylinderCreateInfo cci;
            cci.halfExtend   = 1.0f;
            cci.numberSlices = 32;
            cci.radius       = 1.0f;
            return CreateCylinder(pc, &cci);
        });

        builtin_geometries[4] = create_geometry([](GeometryCreater *pc)
        {
            TorusCreateInfo tci;
            tci.innerRadius  = 0.65f;
            tci.outerRadius  = 1.25f;
            tci.numberSlices = 64;
            tci.numberStacks = 16;
            return CreateTorus(pc, &tci);
        });

        builtin_geometries[5] = create_geometry([](GeometryCreater *pc)
        {
            HollowCylinderCreateInfo hcci;
            hcci.halfExtend   = 1.0f;
            hcci.innerRadius  = 0.6f;
            hcci.outerRadius  = 1.0f;
            hcci.numberSlices = 48;
            return CreateHollowCylinder(pc, &hcci);
        });

        builtin_geometries[6] = create_geometry([](GeometryCreater *pc)
        {
            HexSphereCreateInfo hsci;
            hsci.subdivisions = 3;
            return CreateHexSphere(pc, &hsci);
        });

        builtin_geometries[7] = create_geometry([](GeometryCreater *pc)
        {
            CapsuleCreateInfo cci;
            return CreateCapsule(pc, &cci);
        });

        builtin_geometries[8] = create_geometry([](GeometryCreater *pc)
        {
            TaperedCapsuleCreateInfo tcci;
            tcci.topRadius = 0.25f;
            return CreateTaperedCapsule(pc, &tcci);
        });

        builtin_geometries[9] = create_geometry([](GeometryCreater *pc)
        {
            CubeCreateInfo cci;
            cci.segments_x = 1;
            cci.segments_y = 1;
            cci.segments_z = 1;
            return CreateCube(pc, &cci);
        });

        auto *gc   = GetGraphicsContext();
        auto *pool = gc ? gc->GetGlobalSSBOBufferRegistry() : nullptr;
        auto *dev  = GetDevice();

        planet_geometry->EnsureMeshDrawParams(pool, dev);

        // 设置每种几何体的包围盒与 meshlet 分组
        const glm::vec4 default_extents[GEOMETRY_VARIANT_COUNT] = {
            glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), // Sphere
            glm::vec4(1.0f, 1.0f, 0.6f, 0.0f), // Dome
            glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), // Cone
            glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), // Cylinder
            glm::vec4(1.3f, 1.3f, 0.4f, 0.0f), // Torus
            glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), // HollowCylinder
            glm::vec4(1.0f, 1.0f, 1.0f, 0.0f), // HexSphere
            glm::vec4(1.0f, 1.0f, 2.0f, 0.0f), // Capsule
            glm::vec4(1.0f, 1.0f, 2.0f, 0.0f), // TaperedCapsule
            glm::vec4(0.5f, 0.5f, 0.5f, 0.0f), // Cube
        };

        for (uint32_t i = 0; i < GEOMETRY_VARIANT_COUNT; ++i)
        {
            if (!builtin_geometries[i])
            {
                GLogError(u8"[ComputeAsteroidBelt] Failed to create geometry variant %u", i);
                return false;
            }

            builtin_geometries[i]->EnsureMeshDrawParams(pool, dev);
            geometry_ids[i] = builtin_geometries[i]->GetGeometryID();

            const uint32_t total_verts = static_cast<uint32_t>(
                builtin_geometries[i]->GetIndexCount() > 0 ? builtin_geometries[i]->GetIndexCount()
                                                           : builtin_geometries[i]->GetVertexCount());
            mesh_group_counts[i] = CalcMeshGroupCount(false, total_verts);

            cpu_geometry_aabbs[i].center[0] = 0.0f;
            cpu_geometry_aabbs[i].center[1] = 0.0f;
            cpu_geometry_aabbs[i].center[2] = 0.0f;
            cpu_geometry_aabbs[i].center[3] = 0.0f;

            cpu_geometry_aabbs[i].extents[0] = default_extents[i].x;
            cpu_geometry_aabbs[i].extents[1] = default_extents[i].y;
            cpu_geometry_aabbs[i].extents[2] = default_extents[i].z;
            cpu_geometry_aabbs[i].extents[3] = 0.0f;
        }

        return true;
    }

    bool InitMaterials()
    {
        auto *domain_manager = GetManager<GlobalSSBOBufferRegistry>();
        if (!domain_manager)
            return false;

        // 1. 主星发光材质（深邃恒星/气态巨行星金黄琥珀色谱）
        planet_mtl_accessor = domain_manager->GetAccessor<graph::ssbo::EmissiveSurfaceRow>();
        if (!planet_mtl_accessor)
            return false;

        graph::ssbo::EmissiveSurfaceRow planet_data{};
        planet_data.color = Color4f(1.0f, 0.72f, 0.25f, 1.0f);
        if (!planet_mtl_accessor.Write(planet_data))
            return false;

        planet_recipe.recipe_name = "ComputeAsteroidBelt.PlanetMaterial";
        planet_recipe.mtl_def_id  = "builtin/pure_color";
        planet_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();
        planet_recipe.material_ssbo_binding = planet_mtl_accessor.GetGlobalSSBOBinding();

        // 2. 陨星群材质（10 种太空矿物色系）
        const Color4f mineral_palette[GEOMETRY_VARIANT_COUNT] = {
            Color4f(0.85f, 0.88f, 0.92f, 1.0f), // 0: 镍铁陨石 (Silvery Nickel-Iron)
            Color4f(0.82f, 0.74f, 0.58f, 1.0f), // 1: 硅酸盐岩 (Desert Silicate)
            Color4f(0.40f, 0.85f, 0.95f, 1.0f), // 2: 冰晶彗星核 (Glacial Ice)
            Color4f(0.55f, 0.55f, 0.58f, 1.0f), // 3: 玄武深岩 (Basalt Rock)
            Color4f(0.55f, 0.82f, 0.42f, 1.0f), // 4: 橄榄石原晶 (Peridot Olivine)
            Color4f(0.92f, 0.65f, 0.32f, 1.0f), // 5: 赤铜合金 (Bronze Pyrite)
            Color4f(0.78f, 0.45f, 0.60f, 1.0f), // 6: 碳质玫瑰金 (Rose Carbonaceous)
            Color4f(0.95f, 0.80f, 0.30f, 1.0f), // 7: 琥珀金红石 (Amber Rutile)
            Color4f(0.35f, 0.50f, 0.90f, 1.0f), // 8: 钴蓝刚玉 (Cobalt Corundum)
            Color4f(0.90f, 0.35f, 0.35f, 1.0f), // 9: 红宝石晶簇 (Ruby Crystalline)
        };

        for (uint32_t i = 0; i < GEOMETRY_VARIANT_COUNT; ++i)
        {
            asteroid_mtl_accessors[i] = domain_manager->GetAccessor<graph::ssbo::EmissiveSurfaceRow>();
            if (!asteroid_mtl_accessors[i])
                return false;

            graph::ssbo::EmissiveSurfaceRow asteroid_data{};
            asteroid_data.color = mineral_palette[i];
            if (!asteroid_mtl_accessors[i].Write(asteroid_data))
                return false;

            mineral_payload_indices[i] = asteroid_mtl_accessors[i].GetRowID();
        }

        asteroid_recipe.recipe_name = "ComputeAsteroidBelt.AsteroidMaterial";
        asteroid_recipe.mtl_def_id  = "DebugNormalColor";
        asteroid_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();
        asteroid_recipe.material_ssbo_binding = asteroid_mtl_accessors[0].GetGlobalSSBOBinding();

        return true;
    }

    bool InitECSScene()
    {
        ecs_context = GetECSContext();
        if (!ecs_context)
            return false;

        // 1. 创建主星实体 (大球，居中，半径 35.0)
        planet_asset = PrimitiveAsset(planet_geometry, &planet_recipe, PrimitiveType::Triangles);
        planet_entity = ecs_context->CreateEntity<Entity>("PlanetSphere");
        auto planet_transform = planet_entity->AddComponent<TransformComponent>(Mobility::Static);
        planet_transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        planet_transform->SetLocalScale(glm::vec3(PLANET_RADIUS));
        planet_transform->SetMovable(false);

        auto planet_prim = planet_entity->AddComponent<PrimitiveComponent>();
        planet_prim->SetPrimitiveAsset(&planet_asset);
        PrimitiveComponent::MaterialDataAuthoringResource p_res{};
        p_res = planet_mtl_accessor.GetGlobalSSBOBinding();
        planet_prim->SetMaterialDataResource(p_res);
        planet_prim->SetVisible(true);

        // 2. 创建 10 个陨星批次代表实体（每个代表 100,000 颗同几何体的陨星）
        for (uint32_t i = 0; i < GEOMETRY_VARIANT_COUNT; ++i)
        {
            asteroid_assets[i] = PrimitiveAsset(builtin_geometries[i], &asteroid_recipe, PrimitiveType::Triangles);

            AnsiString name = "AsteroidGroup_" + AnsiString::numberOf(i);
            auto *e = ecs_context->CreateEntity<Entity>(name.c_str());
            asteroid_entities[i] = e;

            auto transform = e->AddComponent<TransformComponent>(Mobility::Static);
            transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            transform->SetLocalScale(glm::vec3(1.0f));
            transform->SetMovable(false);

            auto prim = e->AddComponent<InstancedPrimitiveComponent>();
            prim->SetPrimitiveAsset(&asteroid_assets[i]);
            PrimitiveComponent::MaterialDataAuthoringResource a_res{};
            a_res = asteroid_mtl_accessors[i].GetGlobalSSBOBinding();
            prim->SetMaterialDataResource(a_res);
            prim->SetInstanceCount(INSTANCES_PER_GEOM);
            prim->SetMaxInstances(INSTANCES_PER_GEOM);
            prim->AllocateContiguousInstances(INSTANCES_PER_GEOM);
            prim->SetAllInstances4ID(i * INSTANCES_PER_GEOM, geometry_ids[i], mineral_payload_indices[i], 0, true);
            prim->SetL2WBuffer(world_matrices_buffer);
            prim->SetL2WIndexBuffer(l2w_index_buffer);
            prim->SetMeshDrawParamsBuffer(mesh_draw_params_buffer);
            prim->SetMaterialDataRowsBuffer(material_data_rows_buffer);
            prim->SetIndirectMeshTaskBuffer(indirect_cmds_buffer);
            prim->SetGPUDriven(true);
            prim->SetVisible(true);
        }

        return true;
    }

    bool InitCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        auto camera = camera_entity->AddComponent<CameraComponent>();

        // 广角鸟瞰宏伟土星环系统
        camera->control_mode     = CameraComponent::ControlMode::ViewModel;
        camera->target           = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance         = 340.0f;
        camera->min_distance     = 45.0f;    // 刚好在行星表面(半径35)之外
        camera->max_distance     = 2000.0f;  // 允许拉远到全景宏观宇宙鸟瞰
        camera->near_plane       = 1.0f;
        camera->far_plane        = 10000.0f;
        camera->zoom_sensitivity = 0.12f;
        camera->yaw              = 0.0f;
        camera->pitch            = -22.0f;
        camera->is_main_camera   = true;
        camera->matrix_dirty     = true;

        camera->camera_data   = GetCamera();
        camera->camera_info   = const_cast<graph::CameraInfo *>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        auto camera_system = ecs_context->EnsureCameraSystem();
        if (camera_system)
            camera_system->Update(0.0f);

        return true;
    }

    bool CreateComputeResources(VulkanDevice *dev)
    {
        GLogInfo(u8"[ComputeAsteroidBelt] Generating orbital parameters for %u asteroids...", TOTAL_ASTEROIDS);

        // 1. 初始化 1,000,000 颗陨星的开普勒轨道物理与波动参数
        auto *orbit_params = new AsteroidOrbitParam[TOTAL_ASTEROIDS];

        for (uint32_t i = 0; i < TOTAL_ASTEROIDS; ++i)
        {
            // 均匀面积分布随机半径 r in [RING_INNER_RADIUS, RING_OUTER_RADIUS]
            const float u1 = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
            const float u2 = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
            const float u3 = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
            const float u4 = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);

            const float r = sqrtf(u1 * (RING_OUTER_RADIUS * RING_OUTER_RADIUS - RING_INNER_RADIUS * RING_INNER_RADIUS) + RING_INNER_RADIUS * RING_INNER_RADIUS);
            const float angle = u2 * 6.28318530718f;
            const float height = (u3 - 0.5f) * 2.4f; // 初始薄盘厚度 [-1.2, +1.2]

            // 开普勒差速环绕：角速度 omega = sqrt(mu / r^3)
            constexpr float MU = 12000.0f;
            const float orbit_speed = sqrtf(MU / (r * r * r));

            // 自转轴与自转角速度
            glm::vec3 axis(
                (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) - 0.5f,
                (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) - 0.5f,
                (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) - 0.5f
            );
            if (glm::length(axis) < 0.01f) axis = glm::vec3(0.0f, 0.0f, 1.0f);
            axis = glm::normalize(axis);
            const float spin_speed = ((static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) - 0.5f) * 4.0f;

            // 陨石尺寸、波动幅度与相位
            const float scale = 0.20f + 0.55f * (u4 * u4);
            const float wave_amp = 0.8f + 1.8f * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX));
            const float wave_freq = 0.05f + 0.15f * (static_cast<float>(rand()) / static_cast<float>(RAND_MAX));
            const float wave_phase = static_cast<float>(rand()) / static_cast<float>(RAND_MAX) * 6.2831853f;

            orbit_params[i].base_radius = r;
            orbit_params[i].base_angle  = angle;
            orbit_params[i].base_height = height;
            orbit_params[i].orbit_speed = orbit_speed;
            orbit_params[i].spin_axis   = glm::vec4(axis, spin_speed);
            orbit_params[i].scale       = scale;
            orbit_params[i].wave_amp    = wave_amp;
            orbit_params[i].wave_freq   = wave_freq;
            orbit_params[i].wave_phase  = wave_phase;
        }

        orbit_params_buffer = dev->CreateBuffer(
            "Asteroids.OrbitParams",
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            sizeof(AsteroidOrbitParam) * TOTAL_ASTEROIDS,
            sizeof(AsteroidOrbitParam) * TOTAL_ASTEROIDS,
            orbit_params,
            BufferAllocPolicy::CPUVisible
        );
        delete[] orbit_params;

        // 2. 包围盒表 (320 B)
        geometry_aabbs_buffer = dev->CreateBuffer(
            "Asteroids.GeometryAABBs",
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            sizeof(GeometryAABB) * GEOMETRY_VARIANT_COUNT,
            sizeof(GeometryAABB) * GEOMETRY_VARIANT_COUNT,
            cpu_geometry_aabbs,
            BufferAllocPolicy::CPUVisible
        );

        // 3. 世界矩阵表 (64 MB，必须具备 SHADER_DEVICE_ADDRESS 以供 Mesh Shader BDA 寻址)
        world_matrices_buffer = dev->CreateBuffer(
            "Asteroids.WorldMatrices",
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            sizeof(glm::mat4) * TOTAL_ASTEROIDS,
            sizeof(glm::mat4) * TOTAL_ASTEROIDS,
            nullptr,
            BufferAllocPolicy::GPUOnly
        );

        // 4. L2W 实例索引表 (4 MB，具备 SHADER_DEVICE_ADDRESS 供 pc_root.addr_l2w_index)
        l2w_index_buffer = dev->CreateBuffer(
            "Asteroids.L2WIndices",
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            sizeof(uint32_t) * TOTAL_ASTEROIDS,
            sizeof(uint32_t) * TOTAL_ASTEROIDS,
            nullptr,
            BufferAllocPolicy::GPUOnly
        );

        // 5. 计数缓冲 (44 B，含 1 个总可见数 + 10 个分几何体可见数)
        draw_count_buffer = dev->CreateBuffer(
            "Asteroids.DrawCounts",
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            sizeof(DrawCountData),
            sizeof(DrawCountData),
            nullptr,
            BufferAllocPolicy::GPUOnly
        );

        // 回读缓冲 (44 B，CPU 可见)
        readback_count_buffer = dev->CreateBuffer(
            "Asteroids.ReadbackCounts",
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            sizeof(DrawCountData),
            sizeof(DrawCountData),
            nullptr,
            BufferAllocPolicy::CPUVisible
        );

        // 6. MeshDrawCommands 表 (80 B，存 10 行 {geometry_id, first_instance})
        MeshDrawCommand initial_mesh_cmds[GEOMETRY_VARIANT_COUNT]{};
        for (uint32_t i = 0; i < GEOMETRY_VARIANT_COUNT; ++i)
        {
            initial_mesh_cmds[i].geometry_id    = geometry_ids[i];
            initial_mesh_cmds[i].first_instance = i * INSTANCES_PER_GEOM;
        }

        mesh_draw_params_buffer = dev->CreateBuffer(
            "Asteroids.MeshDrawParams",
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            sizeof(MeshDrawCommand) * GEOMETRY_VARIANT_COUNT,
            sizeof(MeshDrawCommand) * GEOMETRY_VARIANT_COUNT,
            initial_mesh_cmds,
            BufferAllocPolicy::CPUVisible
        );

        // 7. 材质实例数据索引行表 (8 MB，1,000,000 实例各映射对应矿物色行号)
        material_data_rows_buffer = dev->CreateBuffer(
            "Asteroids.MaterialDataRows",
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            sizeof(mtl::MaterialInstanceAddresses) * TOTAL_ASTEROIDS,
            sizeof(mtl::MaterialInstanceAddresses) * TOTAL_ASTEROIDS,
            nullptr,
            BufferAllocPolicy::Auto
        );

        auto *rows_gpu = material_data_rows_buffer->GetGPUBuffer();
        if (rows_gpu)
        {
            auto *rows_ptr = static_cast<mtl::MaterialInstanceAddresses *>(
                rows_gpu->Map(0, sizeof(mtl::MaterialInstanceAddresses) * TOTAL_ASTEROIDS));
            if (rows_ptr)
            {
                for (uint32_t g = 0; g < GEOMETRY_VARIANT_COUNT; ++g)
                {
                    const uint32_t payload_idx = mineral_payload_indices[g];
                    const uint32_t start = g * INSTANCES_PER_GEOM;
                    const uint32_t end   = start + INSTANCES_PER_GEOM;
                    for (uint32_t i = start; i < end; ++i)
                    {
                        rows_ptr[i].payload_index = payload_idx;
                        rows_ptr[i].texture_reference_index = 0;
                    }
                }
                rows_gpu->Unmap();
            }
        }

        // 8. 间接绘制命令缓冲 (10 条命令，具备 STORAGE_BUFFER_BIT 供 CS 写入)
        indirect_cmds_buffer = dev->CreateIndirectMeshTaskBuffer(
            GEOMETRY_VARIANT_COUNT,
            BufferAllocPolicy::GPUOnly,
            ObjectNameBuilder("Asteroids.IndirectMeshTaskBuffer"),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        );

        // 8. 描述符集与管线构建
        // 8a. 创建 Descriptor Pool (7 个 StorageBuffer 描述符)
        VkDescriptorPoolSize pool_size{};
        pool_size.type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        pool_size.descriptorCount = 10;

        VkDescriptorPoolCreateInfo pool_info{};
        pool_info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.maxSets       = 2;
        pool_info.poolSizeCount = 1;
        pool_info.pPoolSizes    = &pool_size;

        if (vkCreateDescriptorPool(dev->GetDevice(), &pool_info, nullptr, &desc_pool) != VK_SUCCESS)
            return false;

        // 8b. Layout 1: Simulation & Cull (5 Storage Buffers)
        VkDescriptorSetLayoutBinding sim_bindings[5]{};
        for (uint32_t b = 0; b < 5; ++b)
        {
            sim_bindings[b].binding         = b;
            sim_bindings[b].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            sim_bindings[b].descriptorCount = 1;
            sim_bindings[b].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
        }

        VkDescriptorSetLayoutCreateInfo sim_layout_info{};
        sim_layout_info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        sim_layout_info.bindingCount = 5;
        sim_layout_info.pBindings    = sim_bindings;

        if (vkCreateDescriptorSetLayout(dev->GetDevice(), &sim_layout_info, nullptr, &sim_layout) != VK_SUCCESS)
            return false;

        // 8c. Layout 2: Finalize Indirect Commands (2 Storage Buffers)
        VkDescriptorSetLayoutBinding fin_bindings[2]{};
        for (uint32_t b = 0; b < 2; ++b)
        {
            fin_bindings[b].binding         = b;
            fin_bindings[b].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            fin_bindings[b].descriptorCount = 1;
            fin_bindings[b].stageFlags      = VK_SHADER_STAGE_COMPUTE_BIT;
        }

        VkDescriptorSetLayoutCreateInfo fin_layout_info{};
        fin_layout_info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        fin_layout_info.bindingCount = 2;
        fin_layout_info.pBindings    = fin_bindings;

        if (vkCreateDescriptorSetLayout(dev->GetDevice(), &fin_layout_info, nullptr, &finalize_layout) != VK_SUCCESS)
            return false;

        // 8d. 分配并更新描述符集
        VkDescriptorSetAllocateInfo alloc_info{};
        alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        alloc_info.descriptorPool     = desc_pool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pSetLayouts        = &sim_layout;
        vkAllocateDescriptorSets(dev->GetDevice(), &alloc_info, &sim_set);

        alloc_info.pSetLayouts        = &finalize_layout;
        vkAllocateDescriptorSets(dev->GetDevice(), &alloc_info, &finalize_set);

        // 更新 Set 0 (Sim + Cull)
        VkDescriptorBufferInfo sim_buf_infos[5]{
            *orbit_params_buffer->GetBufferInfo(),
            *geometry_aabbs_buffer->GetBufferInfo(),
            *world_matrices_buffer->GetBufferInfo(),
            *l2w_index_buffer->GetBufferInfo(),
            *draw_count_buffer->GetBufferInfo(),
        };

        VkWriteDescriptorSet sim_writes[5]{};
        for (uint32_t b = 0; b < 5; ++b)
        {
            sim_writes[b].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            sim_writes[b].dstSet          = sim_set;
            sim_writes[b].dstBinding      = b;
            sim_writes[b].descriptorCount = 1;
            sim_writes[b].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            sim_writes[b].pBufferInfo     = &sim_buf_infos[b];
        }
        vkUpdateDescriptorSets(dev->GetDevice(), 5, sim_writes, 0, nullptr);

        // 更新 Set 1 (Finalize Commands)
        VkDescriptorBufferInfo fin_buf_infos[2]{
            *draw_count_buffer->GetBufferInfo(),
            *indirect_cmds_buffer->GetBufferInfo(),
        };

        VkWriteDescriptorSet fin_writes[2]{};
        for (uint32_t b = 0; b < 2; ++b)
        {
            fin_writes[b].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            fin_writes[b].dstSet          = finalize_set;
            fin_writes[b].dstBinding      = b;
            fin_writes[b].descriptorCount = 1;
            fin_writes[b].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            fin_writes[b].pBufferInfo     = &fin_buf_infos[b];
        }
        vkUpdateDescriptorSets(dev->GetDevice(), 2, fin_writes, 0, nullptr);

        // 8e. 创建 Compute Pipelines
        GraphicsContext *gc = GetGraphicsContext();
        if (!gc || !gc->GetMaterialManager())
            return false;

        sim_pipeline = gc->GetMaterialManager()->CreateComputePipeline(
            "Asteroids.SimPipeline",
            COMPUTE_SIM_CULL_GLSL,
            sim_layout,
            sizeof(SimulationPushConstants)
        );

        finalize_pipeline = gc->GetMaterialManager()->CreateComputePipeline(
            "Asteroids.FinalizePipeline",
            COMPUTE_FINALIZE_CMDS_GLSL,
            finalize_layout,
            sizeof(FinalizePushConstants)
        );

        if (!sim_pipeline || !finalize_pipeline)
        {
            GLogError(u8"[ComputeAsteroidBelt] Failed to create compute pipelines");
            return false;
        }

        // 9. 命令缓冲与队列
        compute_cmd   = dev->CreateComputeCommandBuffer("Asteroids.ComputeCmd");
        compute_queue = dev->CreateQueue("Asteroids.ComputeQueue");

        return compute_cmd && compute_queue;
    }

public:

    ComputeAsteroidBeltApp() = default;

    ~ComputeAsteroidBeltApp() override
    {
        if (sim_pipeline)             delete sim_pipeline;
        if (finalize_pipeline)        delete finalize_pipeline;

        auto *dev = GetDevice();
        if (dev)
        {
            if (sim_layout)               vkDestroyDescriptorSetLayout(dev->GetDevice(), sim_layout, nullptr);
            if (finalize_layout)          vkDestroyDescriptorSetLayout(dev->GetDevice(), finalize_layout, nullptr);
            if (desc_pool)                vkDestroyDescriptorPool(dev->GetDevice(), desc_pool, nullptr);
            if (compute_cmd)              delete compute_cmd;
            if (orbit_params_buffer)      delete orbit_params_buffer;
            if (geometry_aabbs_buffer)    delete geometry_aabbs_buffer;
            if (world_matrices_buffer)    delete world_matrices_buffer;
            if (l2w_index_buffer)         delete l2w_index_buffer;
            if (draw_count_buffer)        delete draw_count_buffer;
            if (mesh_draw_params_buffer)  delete mesh_draw_params_buffer;
            if (material_data_rows_buffer)delete material_data_rows_buffer;
            if (indirect_cmds_buffer)     delete indirect_cmds_buffer;
            if (readback_count_buffer)    delete readback_count_buffer;
        }

        for (uint32_t i = 0; i < GEOMETRY_VARIANT_COUNT; ++i)
        {
            if (builtin_geometries[i])
                delete builtin_geometries[i];
        }
        if (planet_geometry)
            delete planet_geometry;
        if (mesh_vdm)
            delete mesh_vdm;
    }

    bool Init() override
    {
        auto *dev = GetDevice();
        if (!dev)
            return false;

        SetClearColor(Color4f(0.015f, 0.015f, 0.025f, 1.0f)); // 深邃外太空星空背景

        GLogInfo(u8"[ComputeAsteroidBelt] ================================================================");
        GLogInfo(u8"[ComputeAsteroidBelt] Initializing 1,000,000 Asteroids Saturn-like Ring Mega Example!");
        GLogInfo(u8"[ComputeAsteroidBelt] 10 Builtin Geometries x 100,000 Instances each = 1,000,000 Total");
        GLogInfo(u8"[ComputeAsteroidBelt] ================================================================");

        if (!InitVDM())                   return false;
        if (!CreateGeometries())          return false;
        if (!InitMaterials())             return false;
        if (!CreateComputeResources(dev)) return false;
        if (!InitECSScene())              return false;
        if (!InitCamera())                return false;

        GLogInfo(u8"[ComputeAsteroidBelt] Initialization completed successfully! 1,000,000 asteroids ready.");
        return true;
    }

    void Tick(double delta_time) override
    {
        elapsed_time += static_cast<float>(delta_time);
        tick_frame_count++;

        // 动态摄像机缓慢环绕漫游（与鼠标交互增量融合）
        if (camera_entity)
        {
            auto camera = camera_entity->GetComponent<CameraComponent>();
            if (camera)
            {
                camera->yaw += static_cast<float>(delta_time) * 3.0f;
                camera->matrix_dirty = true;
            }
        }

        // 1. 获取当前摄像机 6 个视锥平面 (RH_ZO 约定)
        const CameraInfo *ci = GetCameraInfo();
        SimulationPushConstants sim_pc{};
        if (ci)
        {
            for (int p = 0; p < 6; ++p)
            {
                sim_pc.frustum_planes[p] = glm::vec4(ci->frustum_planes[p].x,
                                                     ci->frustum_planes[p].y,
                                                     ci->frustum_planes[p].z,
                                                     ci->frustum_planes[p].w);
            }
        }
        sim_pc.time            = elapsed_time;
        sim_pc.total_asteroids = TOTAL_ASTEROIDS;

        FinalizePushConstants fin_pc{};
        for (uint32_t i = 0; i < GEOMETRY_VARIANT_COUNT; ++i)
        {
            fin_pc.mesh_group_counts[i] = mesh_group_counts[i];
        }

        // 2. 录制并提交 Compute Shader 模拟与剔除
        compute_cmd->Begin();

        // 2a. 清空计数缓冲 (44 B) 并设置屏障
        compute_cmd->FillBuffer(draw_count_buffer->GetBuffer(), 0, sizeof(DrawCountData), 0);
        compute_cmd->BufferMemoryBarrier(
            draw_count_buffer->GetBuffer(),
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
        );

        // 2b. 分派 Simulation & Culling (1,000,000 线程，3907 工作组)
        compute_cmd->BindPipeline(sim_pipeline);
        compute_cmd->BindDescriptorSets(sim_pipeline->GetPipelineLayout(), 2, &sim_set, 1);
        compute_cmd->PushConstants(sim_pipeline->GetPipelineLayout(), &sim_pc, sizeof(sim_pc));
        compute_cmd->Dispatch((TOTAL_ASTEROIDS + 255) / 256, 1, 1);

        // 2c. 屏障：确保计数写入完成后再生成间接命令
        compute_cmd->BufferMemoryBarrier(
            draw_count_buffer->GetBuffer(),
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT
        );

        // 2d. 分派 Finalize Indirect Commands (1 工作组，10 线程)
        compute_cmd->BindPipeline(finalize_pipeline);
        compute_cmd->BindDescriptorSets(finalize_pipeline->GetPipelineLayout(), 2, &finalize_set, 1);
        compute_cmd->PushConstants(finalize_pipeline->GetPipelineLayout(), &fin_pc, sizeof(fin_pc));
        compute_cmd->Dispatch(1, 1, 1);

        // 2e. 周期性回读计数用于性能与统计日志 (每 60 帧)
        const bool need_readback = (tick_frame_count % 60 == 0);
        if (need_readback)
        {
            compute_cmd->BufferMemoryBarrier(
                draw_count_buffer->GetBuffer(),
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_SHADER_WRITE_BIT,
                VK_ACCESS_TRANSFER_READ_BIT
            );

            VkBufferCopy copy_region{};
            copy_region.srcOffset = 0;
            copy_region.dstOffset = 0;
            copy_region.size      = sizeof(DrawCountData);
            vkCmdCopyBuffer(
                *compute_cmd,
                draw_count_buffer->GetBuffer(),
                readback_count_buffer->GetBuffer(),
                1,
                &copy_region
            );
        }

        // 2f. 终态屏障：对 Draw Indirect 以及 Mesh Shader Read 完全可见
        compute_cmd->BufferMemoryBarrier(
            indirect_cmds_buffer->GetBuffer(),
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_INDIRECT_COMMAND_READ_BIT
        );

        compute_cmd->BufferMemoryBarrier(
            world_matrices_buffer->GetBuffer(),
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT
        );

        compute_cmd->BufferMemoryBarrier(
            l2w_index_buffer->GetBuffer(),
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT
        );

        compute_cmd->End();

        compute_queue->Submit(compute_cmd, nullptr, nullptr);
        compute_queue->WaitFence();

        // 3. 输出实时吞吐与视锥剔除诊断数据
        if (need_readback)
        {
            auto *counts = static_cast<const DrawCountData *>(readback_count_buffer->Map(0, sizeof(DrawCountData)));
            if (counts)
            {
                const double current_time = GetTimeSec();
                double fps = 0.0;
                double frame_ms = 0.0;
                if (last_stat_time > 0.0)
                {
                    const double dur = current_time - last_stat_time;
                    const uint64_t frames = tick_frame_count - last_stat_frame;
                    fps = static_cast<double>(frames) / dur;
                    frame_ms = (dur / static_cast<double>(frames)) * 1000.0;
                }
                last_stat_time  = current_time;
                last_stat_frame = tick_frame_count;

                const float vis_pct  = (static_cast<float>(counts->total_visible) / static_cast<float>(TOTAL_ASTEROIDS)) * 100.0f;
                const float cull_pct = 100.0f - vis_pct;

                GLogInfo(u8"[AsteroidBelt] Frame %llu | Total: %u | Visible: %u (%.1f%%) | Culled: %u (%.1f%%) | FPS: %.1f (%.2f ms)",
                         tick_frame_count,
                         TOTAL_ASTEROIDS,
                         counts->total_visible,
                         vis_pct,
                         TOTAL_ASTEROIDS - counts->total_visible,
                         cull_pct,
                         fps,
                         frame_ms);

                GLogInfo(u8"  [Breakdown] Sph:%u Dom:%u Con:%u Cyl:%u Tor:%u Hol:%u Hex:%u Cap:%u Tap:%u Cub:%u",
                         counts->geom_visible[0], counts->geom_visible[1], counts->geom_visible[2], counts->geom_visible[3],
                         counts->geom_visible[4], counts->geom_visible[5], counts->geom_visible[6], counts->geom_visible[7],
                         counts->geom_visible[8], counts->geom_visible[9]);

                readback_count_buffer->Unmap();
            }
        }

        if (ecs_context)
        {
            if (auto *id_storage = ecs_context->GetDrawItemIDStorage())
            {
                id_storage->SetExternalGPUBuffer(l2w_index_buffer, GetDevice());
            }
        }

        WorkObject::Tick(delta_time);
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<ComputeAsteroidBeltApp>(
        OS_TEXT("ULRE - 1,000,000 Asteroids Saturn Ring (GPU-Driven Orbit, Waves & Frustum Culling)"),
        argc,
        argv,
        1280,
        720
    );
}
