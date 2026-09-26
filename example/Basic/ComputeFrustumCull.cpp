// ComputeFrustumCull (Step 3)
//
// 验证内容：
// 1. 平铺 AABB 与 4-ID 输入：DrawItem4ID 候选表、WorldMatrices (L2W) 表、GeometryAABB 包围盒表
// 2. Compute Shader Frustum Culling：读取 6 个视锥平面（Vulkan RH_ZO 语义），执行投影半径包围盒剔除
// 3. GPU Compaction 紧凑化输出：通过 atomicAdd(draw_count, 1) 无锁紧凑写入 Visible 4-ID Buffer 并生成 Indirect Commands
// 4. 管线内存屏障：
//    - FillBuffer (TRANSFER_WRITE) -> COMPUTE_SHADER (SHADER_READ | SHADER_WRITE)
//    - COMPUTE_SHADER (SHADER_WRITE) -> DRAW_INDIRECT (INDIRECT_COMMAND_READ) & VERTEX/COMPUTE (SHADER_READ)
// 5. 双重对比验证：每一帧由 CPU 计算数学真值（提取自相机 MVP），与 GPU 读回的 draw_count 及 visible items 严格比对
// 6. 动态相机轨道演示与 ECS 集成：相机平滑环绕旋转，实时检验 GPU 驱动的动态视锥剔除与间接多绘制

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
#include<hgl/ecs/core/MaterialBatch.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/InstancedPrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/buffer/DeviceBuffer.h>
#include<hgl/vk/buffer/IndirectCommandBuffer.h>
#include<hgl/log/Log.h>

#include<glm/glm.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<glm/gtc/quaternion.hpp>
#include<memory>
#include<cmath>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::graph::mtl;
using namespace hgl::ecs;

namespace
{
    constexpr uint32_t GRID_DIM   = 8;
    constexpr uint32_t TOTAL_CUBES = GRID_DIM * GRID_DIM; // 64 个立方体分布在 8x8 网格中
    constexpr float    SPACING     = 4.0f;
    constexpr float    CUBE_SCALE  = 0.9f;

    struct CullPushConstants
    {
        glm::vec4 frustum_planes[6];
        uint32_t  candidate_count;
        uint32_t  mesh_group_count_x;
        uint64_t  addr_table;           // CullAddressTable 设备地址（BDA）
    };

    static_assert(sizeof(CullPushConstants) == 112, "CullPushConstants 布局必须与 GLSL push_constant block 一致");

    // 7 张表地址的表（写一次）：compute 侧只有 push constant + BDA，无描述符集；
    // 高 128B push constant 放不下 7 个地址，因此地址本身放在一张小表里，pc 只下发表的地址。
    struct CullAddressTable
    {
        uint64_t candidates;
        uint64_t world_matrices;
        uint64_t geometry_aabbs;
        uint64_t visible_items;
        uint64_t draw_count;
        uint64_t indirect_commands;
        uint64_t visible_l2w_indices;
    };

    static_assert(sizeof(CullAddressTable) == 56, "CullAddressTable 布局必须与 GLSL buffer_reference 块一致");

    constexpr const char COMPUTE_CULL_GLSL[] = R"(
#version 450
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

struct DrawItem4ID {
    uint transform_id;
    uint geometry_id;
    uint material_id;
    uint texture_id;
};

struct GeometryAABB {
    vec4 center;
    vec4 extents;
};

struct DrawMeshTasksIndirectCommand {
    uint groupCountX;
    uint groupCountY;
    uint groupCountZ;
};

layout(push_constant) uniform PushConstants {
    vec4 frustum_planes[6];
    uint candidate_count;
    uint mesh_group_count_x;
    uint64_t addr_table;
} pc;

// 7 张表的地址表（BDA：地址经 push constant → 表 → buffer_reference 两级寻址，无 set 无 binding）
layout(buffer_reference, scalar, buffer_reference_align=16) buffer CullAddressTableRef
{
    uint64_t candidates;
    uint64_t world_matrices;
    uint64_t geometry_aabbs;
    uint64_t visible_items;
    uint64_t draw_count;
    uint64_t indirect_commands;
    uint64_t visible_l2w_indices;
};

layout(buffer_reference, std430, buffer_reference_align=16) buffer CandidateRef     { DrawItem4ID items[]; };
layout(buffer_reference, std430, buffer_reference_align=16) buffer WorldMatricesRef { mat4 mats[]; };
layout(buffer_reference, std430, buffer_reference_align=16) buffer GeometryAABBRef  { GeometryAABB boxes[]; };
layout(buffer_reference, std430, buffer_reference_align=16) buffer VisibleItemsRef  { DrawItem4ID items[]; };
layout(buffer_reference, std430, buffer_reference_align=16) buffer DrawCountRef     { uint value; };
layout(buffer_reference, std430, buffer_reference_align=16) buffer IndirectCmdsRef  { DrawMeshTasksIndirectCommand cmds[]; };
layout(buffer_reference, std430, buffer_reference_align=16) buffer UintArrayRef     { uint values[]; };

void main() {
    uint g_idx = gl_GlobalInvocationID.x;
    if (g_idx >= pc.candidate_count)
        return;

    CullAddressTableRef table = CullAddressTableRef(pc.addr_table);

    CandidateRef     candidates           = CandidateRef(table.candidates);
    WorldMatricesRef world_matrices       = WorldMatricesRef(table.world_matrices);
    GeometryAABBRef  geometry_aabbs       = GeometryAABBRef(table.geometry_aabbs);
    VisibleItemsRef  visible_items        = VisibleItemsRef(table.visible_items);
    DrawCountRef     draw_count           = DrawCountRef(table.draw_count);
    IndirectCmdsRef  indirect_commands    = IndirectCmdsRef(table.indirect_commands);
    UintArrayRef     visible_l2w_indices  = UintArrayRef(table.visible_l2w_indices);

    DrawItem4ID item = candidates.items[g_idx];
    mat4 l2w = world_matrices.mats[item.transform_id];
    GeometryAABB aabb = geometry_aabbs.boxes[item.geometry_id];

    vec3 world_center = (l2w * vec4(aabb.center.xyz, 1.0)).xyz;
    mat3 m = mat3(l2w);
    vec3 extents = aabb.extents.xyz;

    bool visible = true;
    for (int i = 0; i < 6; ++i) {
        vec3 n = pc.frustum_planes[i].xyz;
        float d = pc.frustum_planes[i].w;

        // 包围盒在法线上的投影半径 r = dot(extents, abs(n * m))
        float r = dot(extents, abs(n * m));
        float dist = dot(n, world_center) + d;

        if (dist < -r) {
            visible = false;
            break;
        }
    }

    if (visible) {
        uint slot = atomicAdd(draw_count.value, 1);
        visible_items.items[slot] = item;
        indirect_commands.cmds[slot] = DrawMeshTasksIndirectCommand(pc.mesh_group_count_x, 1, 1);
        visible_l2w_indices.values[slot] = item.transform_id;
    }
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

    // ECS 渲染阶段系统：挂载 GPU 计算的 count_buffer
    class GPUIndirectCountHookSystem : public System
    {
        DeviceBuffer *count_buffer = nullptr;

    public:
        explicit GPUIndirectCountHookSystem(DeviceBuffer *cb)
            : System("GPUIndirectCountHookSystem")
            , count_buffer(cb)
        {
            SetExecutionPhase(ExecutionPhase::RenderFrameSync);
        }

        void Update(float /*deltaTime*/) override
        {
            if (!context || !count_buffer)
                return;

            auto &cache = context->GetRenderFrameCache();
            for (auto &pair : cache.materialBatches)
            {
                MaterialBatch *batch = pair.second.get();
                if (batch && !batch->items.empty() && batch->icb_mesh_tasks)
                {
                    batch->icb_count_buffer = count_buffer;
                    batch->icb_count_buffer_offset = 0;
                }
            }
        }
    };
} // namespace

class ComputeFrustumCullApp : public WorkObject
{
    Geometry                 *geometry = nullptr;
    GlobalSSBODataAccessor  mtl_data_ssbo_accessor{};
    MaterialRecipe            cube_recipe{};
    PrimitiveAsset            cube_asset{};

    ECSContext *ecs_context   = nullptr;
    Entity     *camera_entity = nullptr;

    // GPU Compute 资源（全部经 BDA 寻址：地址在 CullAddressTable 里，pc 只下发表地址）
    DeviceBuffer           *candidate_buffer          = nullptr; // 候选 4-ID 表
    DeviceBuffer           *world_matrices_buffer     = nullptr; // L2W 表
    DeviceBuffer           *geometry_aabbs_buffer     = nullptr; // 几何 AABB 表
    DeviceBuffer           *visible_buffer            = nullptr; // 紧凑输出 4-ID
    DeviceBuffer           *count_buffer              = nullptr; // draw count
    IndirectMeshTaskBuffer *indirect_cmds_buffer      = nullptr; // 间接绘制命令（Storage | Indirect）
    DeviceBuffer           *visible_l2w_indices_buffer= nullptr; // 可见 L2W 索引
    DeviceBuffer           *mesh_draw_params_buffer   = nullptr; // MeshDrawCommand SSBO (BDA)
    DeviceBuffer           *material_data_rows_buffer = nullptr; // MaterialInstanceAddresses SSBO (BDA)
    DeviceBuffer           *address_table_buffer      = nullptr; // CullAddressTable（7 个缓冲区设备地址）

    uint64_t                address_table_addr        = 0;       // address_table_buffer 设备地址

    ComputePipeline        *compute_pipeline = nullptr;
    ComputeCmdBuffer       *compute_cmd      = nullptr;
    DeviceQueue            *compute_queue    = nullptr;

    // CPU 侧镜像（用于每帧比对真值）
    DrawItem4ID  cpu_candidates[TOTAL_CUBES];
    glm::mat4    cpu_world_matrices[TOTAL_CUBES];
    GeometryAABB cpu_geometry_aabbs[TOTAL_CUBES];

    float    camera_angle     = 0.0f;
    uint64_t tick_frame_count = 0;

private:

    bool CreateCubeGeometry()
    {
        using namespace inline_geometry;

        auto *geometry_manager = GetManager<GeometryManager>();
        auto *device = GetDevice();
        if (!geometry_manager || !device)
            return false;

        auto pc = std::make_unique<GeometryCreater>(device, CreateGizmo3DGeometryVertexFormat());

        CubeCreateInfo cci;
        cci.segments_x = 1;
        cci.segments_y = 1;
        cci.segments_z = 1;

        geometry = CreateCube(pc.get(), &cci);
        if (!geometry)
            return false;

        auto *gc = GetGraphicsContext();
        auto *pool = gc ? gc->GetGlobalSSBOBufferRegistry() : nullptr;
        if (pool && device)
            geometry->EnsureMeshDrawParams(pool, device);

        geometry_manager->Add(geometry);
        return true;
    }

    bool InitMISSBO()
    {
        auto *domain_manager = GetManager<GlobalSSBOBufferRegistry>();
        if (!domain_manager)
            return false;

        mtl_data_ssbo_accessor = domain_manager->GetAccessor<graph::ssbo::EmissiveSurfaceRow>();
        if (!mtl_data_ssbo_accessor)
            return false;

        graph::ssbo::EmissiveSurfaceRow material_data{};
        material_data.color = GetColor4f(COLOR::Cyan, 1.0f);
        return mtl_data_ssbo_accessor.Write(material_data);
    }

    void InitCPUSceneData()
    {
        for (uint32_t i = 0; i < TOTAL_CUBES; ++i)
        {
            const float col = static_cast<float>(i % GRID_DIM);
            const float row = static_cast<float>(i / GRID_DIM);
            const float x = (col - static_cast<float>(GRID_DIM - 1) * 0.5f) * SPACING;
            const float y = (row - static_cast<float>(GRID_DIM - 1) * 0.5f) * SPACING;
            const float z = 0.0f;

            // CPU 侧镜像初始化
            cpu_candidates[i].transform_id = i;
            cpu_candidates[i].geometry_id  = 0;
            cpu_candidates[i].material_id  = 0;
            cpu_candidates[i].texture_id   = 0;

            glm::mat4 t = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
            glm::mat4 s = glm::scale(glm::mat4(1.0f), glm::vec3(CUBE_SCALE));
            cpu_world_matrices[i] = t * s;

            // 立方体局部 AABB：中心 (0,0,0)，半长宽高 0.5
            cpu_geometry_aabbs[i].center[0] = 0.0f;
            cpu_geometry_aabbs[i].center[1] = 0.0f;
            cpu_geometry_aabbs[i].center[2] = 0.0f;
            cpu_geometry_aabbs[i].center[3] = 0.0f;

            cpu_geometry_aabbs[i].extents[0] = 0.5f;
            cpu_geometry_aabbs[i].extents[1] = 0.5f;
            cpu_geometry_aabbs[i].extents[2] = 0.5f;
            cpu_geometry_aabbs[i].extents[3] = 0.0f;
        }
    }

    bool InitECSScene()
    {
        ecs_context = GetECSContext();
        if (!ecs_context)
            return false;

        cube_recipe.recipe_name = "ComputeFrustumCull.CubeMaterial";
        cube_recipe.mtl_def_id  = "DebugNormalColor";
        cube_recipe.render_state_overrides.pipeline_config = mtl::MakeSolid3DConfig();
        if (!(cube_recipe.material_ssbo_binding = mtl_data_ssbo_accessor.GetGlobalSSBOBinding()).IsValid())
            return false;

        cube_asset = PrimitiveAsset(geometry, &cube_recipe, PrimitiveType::Triangles);

        for (uint32_t i = 0; i < TOTAL_CUBES; ++i)
        {
            const float col = static_cast<float>(i % GRID_DIM);
            const float row = static_cast<float>(i / GRID_DIM);
            const float x = (col - static_cast<float>(GRID_DIM - 1) * 0.5f) * SPACING;
            const float y = (row - static_cast<float>(GRID_DIM - 1) * 0.5f) * SPACING;
            const float z = 0.0f;

            auto *e = ecs_context->CreateEntity<Entity>(("Cube_" + AnsiString::numberOf(i)).c_str());

            auto transform = e->AddComponent<TransformComponent>(Mobility::Static);
            transform->SetLocalPosition(glm::vec3(x, y, z));
            transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            transform->SetLocalScale(glm::vec3(CUBE_SCALE));
            transform->SetMovable(false);

            auto prim = e->AddComponent<InstancedPrimitiveComponent>();
            prim->SetPrimitiveAsset(&cube_asset);
            PrimitiveComponent::MaterialDataAuthoringResource named_struct{};
            named_struct = mtl_data_ssbo_accessor.GetGlobalSSBOBinding();
            prim->SetMaterialDataResource(named_struct);
            prim->SetInstanceCount(1);
            prim->SetMaxInstances(1);
            prim->AllocateContiguousInstances(1);
            prim->SetInstance4ID(0, i, geometry->GetGeometryID(), mtl_data_ssbo_accessor.GetRowID(), 0);
            prim->SetL2WBuffer(world_matrices_buffer);
            prim->SetL2WIndexBuffer(visible_l2w_indices_buffer);
            prim->SetMeshDrawParamsBuffer(mesh_draw_params_buffer);
            prim->SetMaterialDataRowsBuffer(material_data_rows_buffer);
            prim->SetIndirectMeshTaskBuffer(indirect_cmds_buffer);
            prim->SetIndirectCountBuffer(count_buffer, 0);
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

        camera->control_mode  = CameraComponent::ControlMode::ViewModel;
        camera->target        = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance      = 18.0f;
        camera->yaw           = 0.0f;
        camera->pitch         = -18.0f;
        camera->is_main_camera= true;
        camera->matrix_dirty  = true;

        camera->camera_data   = GetCamera();
        camera->camera_info   = const_cast<graph::CameraInfo *>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

    bool CreateComputeResources(VulkanDevice *dev)
    {
        // 1. 创建 CandidateBuffer、WorldMatricesBuffer、GeometryAABBBuffer (输入)
        candidate_buffer = dev->CreateBuffer(
            "ComputeFrustumCull.CandidateBuf",
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            sizeof(DrawItem4ID) * TOTAL_CUBES,
            sizeof(DrawItem4ID) * TOTAL_CUBES,
            cpu_candidates,
            BufferAllocPolicy::CPUVisible
        );

        world_matrices_buffer = dev->CreateBuffer(
            "ComputeFrustumCull.WorldMatsBuf",
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            sizeof(glm::mat4) * TOTAL_CUBES,
            sizeof(glm::mat4) * TOTAL_CUBES,
            cpu_world_matrices,
            BufferAllocPolicy::CPUVisible
        );

        geometry_aabbs_buffer = dev->CreateBuffer(
            "ComputeFrustumCull.AABBsBuf",
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            sizeof(GeometryAABB) * TOTAL_CUBES,
            sizeof(GeometryAABB) * TOTAL_CUBES,
            cpu_geometry_aabbs,
            BufferAllocPolicy::CPUVisible
        );

        // 2. 创建 VisibleBuffer (紧凑输出 4-ID)
        visible_buffer = dev->CreateBuffer(
            "ComputeFrustumCull.VisibleBuf",
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            sizeof(DrawItem4ID) * TOTAL_CUBES,
            sizeof(DrawItem4ID) * TOTAL_CUBES,
            nullptr,
            BufferAllocPolicy::CPUVisible
        );

        // 3. 创建 DrawCountBuffer (支持 Compute Shader 写入 + DrawIndirectCount 读取，带 BDA)
        count_buffer = dev->CreateDrawCountBuffer("ComputeFrustumCull.CountBuffer", 1, BufferAllocPolicy::CPUVisible);

        // 4. 创建 IndirectCommandsBuffer (支持 Compute Shader 写入 + DrawMeshTasks 读取)
        indirect_cmds_buffer = dev->CreateIndirectMeshTaskBuffer(
            TOTAL_CUBES,
            BufferAllocPolicy::CPUVisible,
            "ComputeFrustumCull.IndirectCmds",
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
        );

        // 5. 创建 VisibleL2WIndicesBuffer（由 Compute Shader 紧凑写入，Mesh Shader 通过 BDA 寻址）
        visible_l2w_indices_buffer = dev->CreateBuffer(
            "ComputeFrustumCull.VisibleL2WIndices",
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            sizeof(uint32_t) * TOTAL_CUBES,
            sizeof(uint32_t) * TOTAL_CUBES,
            nullptr,
            BufferAllocPolicy::CPUVisible
        );

        // 6. 创建 MeshDrawCommand 表 (存 64 行 {geometry_id, first_instance = i})
        MeshDrawCommand initial_mesh_cmds[TOTAL_CUBES]{};
        for (uint32_t i = 0; i < TOTAL_CUBES; ++i)
        {
            initial_mesh_cmds[i].geometry_id    = geometry->GetGeometryID();
            initial_mesh_cmds[i].first_instance = i;
        }

        mesh_draw_params_buffer = dev->CreateBuffer(
            "ComputeFrustumCull.MeshDrawParams",
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            sizeof(MeshDrawCommand) * TOTAL_CUBES,
            sizeof(MeshDrawCommand) * TOTAL_CUBES,
            initial_mesh_cmds,
            BufferAllocPolicy::CPUVisible
        );

        // 7. 创建 MaterialDataRows 表 (存 64 行材质实例数据行地址映射)
        mtl::MaterialInstanceAddresses initial_mat_rows[TOTAL_CUBES]{};
        for (uint32_t i = 0; i < TOTAL_CUBES; ++i)
        {
            initial_mat_rows[i].payload_index           = mtl_data_ssbo_accessor.GetRowID();
            initial_mat_rows[i].texture_reference_index = 0;
        }

        material_data_rows_buffer = dev->CreateBuffer(
            "ComputeFrustumCull.MaterialDataRows",
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            sizeof(mtl::MaterialInstanceAddresses) * TOTAL_CUBES,
            sizeof(mtl::MaterialInstanceAddresses) * TOTAL_CUBES,
            initial_mat_rows,
            BufferAllocPolicy::CPUVisible
        );

        if (!candidate_buffer || !world_matrices_buffer || !geometry_aabbs_buffer ||
            !visible_buffer || !count_buffer || !indirect_cmds_buffer ||
            !visible_l2w_indices_buffer || !mesh_draw_params_buffer || !material_data_rows_buffer)
        {
            GLogError(u8"[ComputeFrustumCull] Failed to create compute buffers");
            return false;
        }

        // 8. 地址表（BDA）：7 个缓冲区设备地址写一次——compute 侧只经 push constant 拿到表地址
        CullAddressTable address_table{};
        address_table.candidates          = dev->GetBufferDeviceAddressAligned16(candidate_buffer          ->GetBuffer());
        address_table.world_matrices      = dev->GetBufferDeviceAddressAligned16(world_matrices_buffer     ->GetBuffer());
        address_table.geometry_aabbs      = dev->GetBufferDeviceAddressAligned16(geometry_aabbs_buffer     ->GetBuffer());
        address_table.visible_items       = dev->GetBufferDeviceAddressAligned16(visible_buffer            ->GetBuffer());
        address_table.draw_count          = dev->GetBufferDeviceAddressAligned16(count_buffer              ->GetBuffer());
        address_table.indirect_commands   = dev->GetBufferDeviceAddressAligned16(indirect_cmds_buffer      ->GetBuffer());
        address_table.visible_l2w_indices = dev->GetBufferDeviceAddressAligned16(visible_l2w_indices_buffer->GetBuffer());

        if (address_table.candidates == 0 || address_table.world_matrices == 0 || address_table.geometry_aabbs == 0 ||
            address_table.visible_items == 0 || address_table.draw_count == 0 || address_table.indirect_commands == 0 ||
            address_table.visible_l2w_indices == 0)
        {
            GLogError(u8"[ComputeFrustumCull] 缓冲区设备地址无效（BDA 16B 对齐承诺失败）");
            return false;
        }

        address_table_buffer = dev->CreateBuffer(
            "ComputeFrustumCull.AddressTable",
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            sizeof(CullAddressTable), sizeof(CullAddressTable),
            &address_table,
            BufferAllocPolicy::CPUVisible
        );

        if (!address_table_buffer)
        {
            GLogError(u8"[ComputeFrustumCull] 地址表缓冲区创建失败");
            return false;
        }

        address_table_addr = dev->GetBufferDeviceAddressAligned16(address_table_buffer->GetBuffer());
        if (address_table_addr == 0)
            return false;

        // 9. 创建 Compute Pipeline（用户数据全走 BDA，无第三集描述符集）
        GraphicsContext *gc = GetGraphicsContext();
        if (!gc || !gc->GetMaterialManager())
            return false;

        compute_pipeline = gc->GetMaterialManager()->CreateComputePipeline(
            "ComputeFrustumCull.Pipeline",
            COMPUTE_CULL_GLSL,
            sizeof(CullPushConstants)
        );

        if (!compute_pipeline)
        {
            GLogError(u8"[ComputeFrustumCull] Failed to create compute pipeline");
            return false;
        }

        // 7. 命令缓冲与队列
        compute_cmd   = dev->CreateComputeCommandBuffer("ComputeFrustumCull.Cmd");
        compute_queue = dev->CreateQueue("ComputeFrustumCull.Queue");

        return compute_cmd && compute_queue;
    }

public:

    ~ComputeFrustumCullApp() override
    {
        auto *dev = GetDevice();
        if (dev)
        {
            if (compute_cmd)                delete compute_cmd;
            if (candidate_buffer)           delete candidate_buffer;
            if (world_matrices_buffer)      delete world_matrices_buffer;
            if (geometry_aabbs_buffer)      delete geometry_aabbs_buffer;
            if (visible_buffer)             delete visible_buffer;
            if (count_buffer)               delete count_buffer;
            if (indirect_cmds_buffer)       delete indirect_cmds_buffer;
            if (visible_l2w_indices_buffer) delete visible_l2w_indices_buffer;
            if (mesh_draw_params_buffer)    delete mesh_draw_params_buffer;
            if (material_data_rows_buffer)  delete material_data_rows_buffer;
            if (address_table_buffer)       delete address_table_buffer;
        }
    }

    bool Init() override
    {
        auto *dev = GetDevice();
        if (!dev)
            return false;

        if (!dev->SupportDrawIndirectCount())
        {
            GLogError(u8"[ComputeFrustumCull] FATAL: DrawIndirectCount is not supported by driver or VulkanDevice!");
            return false;
        }

        GLogInfo(u8"[ComputeFrustumCull] Step 3 Initializing: Frustum Culling + 4-ID Compaction + Indirect Count");

        if (!CreateCubeGeometry()) return false;
        if (!InitMISSBO())         return false;
        InitCPUSceneData();
        if (!CreateComputeResources(dev)) return false;
        if (!InitECSScene())       return false;
        if (!InitCamera())         return false;

        // 注册 ECS 挂载系统
        ecs_context->RegisterRenderSystem<GPUIndirectCountHookSystem>(count_buffer);

        GLogInfo(u8"[ComputeFrustumCull] Init completed successfully. %u cubes initialized for GPU Frustum Culling!", TOTAL_CUBES);
        return true;
    }

    void Tick(double delta_time) override
    {
        // 动态平滑旋转相机
        camera_angle += static_cast<float>(delta_time) * 15.0f;
        if (camera_entity)
        {
            auto camera = camera_entity->GetComponent<CameraComponent>();
            if (camera)
            {
                camera->yaw = camera_angle;
                camera->matrix_dirty = true;
            }
        }

        // 1. 获取当前摄像机视锥体 6 个平面
        const CameraInfo *ci = GetCameraInfo();
        CullPushConstants pc{};
        if (ci)
        {
            for (int p = 0; p < 6; ++p)
            {
                pc.frustum_planes[p] = glm::vec4(ci->frustum_planes[p].x,
                                                 ci->frustum_planes[p].y,
                                                 ci->frustum_planes[p].z,
                                                 ci->frustum_planes[p].w);
            }
        }
        pc.candidate_count    = TOTAL_CUBES;
        pc.mesh_group_count_x = 1; // 1 个 64 线程 meshlet 处理 36 个顶点
        pc.addr_table         = address_table_addr;

        // 2. Compute Shader 分派：先清零 count_buffer，然后执行视锥剔除与紧凑输出
        compute_cmd->Begin();

        // 2a. 清零 count_buffer 并设置内存屏障
        compute_cmd->FillBuffer(count_buffer->GetBuffer(), 0, sizeof(uint32_t), 0);
        compute_cmd->BufferMemoryBarrier(
            count_buffer->GetBuffer(),
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT
        );

        // 2b. 分派剔除计算着色器
        compute_cmd->BindPipeline(compute_pipeline);
        compute_cmd->PushConstants(compute_pipeline->GetPipelineLayout(), &pc, sizeof(pc));
        compute_cmd->Dispatch((TOTAL_CUBES + 63) / 64, 1, 1);

        // 2c. 管线内存屏障：确保写入完全完成，对 DrawIndirect 及 Shader Read 完全可见
        compute_cmd->BufferMemoryBarrier(
            count_buffer->GetBuffer(),
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_INDIRECT_COMMAND_READ_BIT
        );

        compute_cmd->BufferMemoryBarrier(
            indirect_cmds_buffer->GetBuffer(),
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_INDIRECT_COMMAND_READ_BIT
        );

        compute_cmd->BufferMemoryBarrier(
            visible_l2w_indices_buffer->GetBuffer(),
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT
        );

        compute_cmd->BufferMemoryBarrier(
            world_matrices_buffer->GetBuffer(),
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT
        );

        compute_cmd->BufferMemoryBarrier(
            visible_buffer->GetBuffer(),
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT
        );

        compute_cmd->End();

        compute_queue->Submit(compute_cmd, nullptr, 0, nullptr, 0);
        compute_queue->WaitFence();

        // 3. CPU 侧真值评估与比对验证
        uint32_t cpu_visible_count = 0;
        bool cpu_visible_flags[TOTAL_CUBES]{};

        for (uint32_t i = 0; i < TOTAL_CUBES; ++i)
        {
            glm::vec3 world_center = glm::vec3(cpu_world_matrices[i] * glm::vec4(cpu_geometry_aabbs[i].center[0],
                                                                                 cpu_geometry_aabbs[i].center[1],
                                                                                 cpu_geometry_aabbs[i].center[2],
                                                                                 1.0f));
            glm::mat3 m = glm::mat3(cpu_world_matrices[i]);
            glm::vec3 ext(cpu_geometry_aabbs[i].extents[0],
                          cpu_geometry_aabbs[i].extents[1],
                          cpu_geometry_aabbs[i].extents[2]);

            bool visible = true;
            for (int p = 0; p < 6; ++p)
            {
                glm::vec3 n(pc.frustum_planes[p]);
                const float d = pc.frustum_planes[p].w;

                const float r = glm::dot(ext, glm::abs(n * m));
                const float dist = glm::dot(n, world_center) + d;

                if (dist < -r)
                {
                    visible = false;
                    break;
                }
            }

            cpu_visible_flags[i] = visible;
            if (visible)
                ++cpu_visible_count;
        }

        // 4. GPU 结果读回比对
        uint32_t gpu_visible_count = 0;
        void *cnt_ptr = count_buffer->GetGPUBuffer()->Map(0, sizeof(uint32_t));
        if (cnt_ptr)
        {
            gpu_visible_count = *reinterpret_cast<uint32_t *>(cnt_ptr);
            count_buffer->GetGPUBuffer()->Unmap();
        }

        void *vis_ptr = visible_buffer->GetGPUBuffer()->Map(0, sizeof(DrawItem4ID) * (gpu_visible_count > 0 ? gpu_visible_count : 1));
        bool all_items_valid = true;
        if (vis_ptr && gpu_visible_count > 0)
        {
            auto *vis_items = reinterpret_cast<DrawItem4ID *>(vis_ptr);
            for (uint32_t v = 0; v < gpu_visible_count; ++v)
            {
                const uint32_t tid = vis_items[v].transform_id;
                if (tid >= TOTAL_CUBES || !cpu_visible_flags[tid])
                {
                    all_items_valid = false;
                    break;
                }
            }
            visible_buffer->GetGPUBuffer()->Unmap();
        }
        else if (vis_ptr)
        {
            visible_buffer->GetGPUBuffer()->Unmap();
        }

        ++tick_frame_count;
        if (tick_frame_count % 60 == 1)
        {
            if (gpu_visible_count == cpu_visible_count && all_items_valid)
            {
                GLogInfo(u8"[ComputeFrustumCull] PASS: Frame %llu, GPU culling visible=%u/%u (CPU ground truth=%u, culled=%u, items verified!)",
                         tick_frame_count, gpu_visible_count, TOTAL_CUBES, cpu_visible_count, TOTAL_CUBES - gpu_visible_count);
            }
            else
            {
                GLogError(u8"[ComputeFrustumCull] MISMATCH: Frame %llu, GPU visible=%u, CPU visible=%u, items_valid=%d",
                          tick_frame_count, gpu_visible_count, cpu_visible_count, all_items_valid ? 1 : 0);
            }
        }

        // 5. ECS 帧渲染驱动
        WorkObject::Tick(delta_time);
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<ComputeFrustumCullApp>(OS_TEXT("Compute Frustum Culling (Step 3)"), argc, argv);
}
