// example/Basic/VertexPulling_Cube.cpp
//
// §C.5 验证 Demo — 全 SSBO 顶点拉取（Vertex Pulling）
//
// 本示例演示：
//  1. Position / Normal / TexCoord0 三个顶点属性全部通过 Storage Buffer 拉取，
//     不绑定任何顶点缓冲区（vkCmdBindVertexBuffers count = 0）。
//  2. 使用 VF_V3F / VF_V2F 格式创建 VAB（prefer_storage_usage = true）。
//  3. 通过 Primitive::SetVertexStreamSource 声明流来源，
//     由 RenderDescriptorBindingSystem 在 sync 阶段统一完成 VertexStreams 绑定。
//  4. smp->SetPullingEnabled(true) 使管线 VIL 为空，
//     shader 侧 GEOMETRY_FETCH_SSBO=1 触发 SSBO fetch 路径。
//
// RenderDoc 验证点：
//  - VERTEX_STREAMS descriptor set 含 3 个 SSBO 绑定（binding 0,3,8）
//  - vkCmdBindVertexBuffers 调用次数 = 0
//  - 顶点着色器反汇编包含 ReadAttrib_Normal / ReadAttrib_TexCoord0

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/geo/GraphicsGeometryFactory.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialBindingInstanceInternalAccess.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/vk/VKIndexBuffer.h>
#include<hgl/vk/VKShaderMaterialProgram.h>
#include<hgl/mtl/Material3DCreateConfig.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

#include<numbers>
#include<memory>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

// ─────────────────────────────────────────────────────────────────────────────
// Cube vertex data — 24 unique vertices (6 faces × 4 verts), 36 indices.
// Winding is clockwise when viewed from the outside face normal direction,
// matching the current default VK_FRONT_FACE_CLOCKWISE raster state.
// ─────────────────────────────────────────────────────────────────────────────

namespace
{

struct PackedFloat3
{
    float x;
    float y;
    float z;
};

struct PackedFloat2
{
    float x;
    float y;
};

static_assert(sizeof(PackedFloat3) == 12, "PackedFloat3 must be tightly packed");
static_assert(sizeof(PackedFloat2) == 8,  "PackedFloat2 must be tightly packed");

// SSBO_Vec3 expects tightly-packed 12-byte vec3 records.
// Use POD arrays instead of glm::vec3 to avoid aligned-gentype padding.
static const PackedFloat3 kPositions[24] =
{
    // +X face  (normal = 1,0,0)
    { 0.5f, -0.5f,  0.5f }, { 0.5f, -0.5f, -0.5f },
    { 0.5f,  0.5f, -0.5f }, { 0.5f,  0.5f,  0.5f },
    // -X face  (normal = -1,0,0)
    {-0.5f, -0.5f, -0.5f }, {-0.5f, -0.5f,  0.5f },
    {-0.5f,  0.5f,  0.5f }, {-0.5f,  0.5f, -0.5f },
    // +Y face  (normal = 0,1,0)
    {-0.5f,  0.5f, -0.5f }, { 0.5f,  0.5f, -0.5f },
    { 0.5f,  0.5f,  0.5f }, {-0.5f,  0.5f,  0.5f },
    // -Y face  (normal = 0,-1,0)
    {-0.5f, -0.5f,  0.5f }, { 0.5f, -0.5f,  0.5f },
    { 0.5f, -0.5f, -0.5f }, {-0.5f, -0.5f, -0.5f },
    // +Z face  (normal = 0,0,1)
    {-0.5f, -0.5f,  0.5f }, { 0.5f, -0.5f,  0.5f },
    { 0.5f,  0.5f,  0.5f }, {-0.5f,  0.5f,  0.5f },
    // -Z face  (normal = 0,0,-1)
    { 0.5f, -0.5f, -0.5f }, {-0.5f, -0.5f, -0.5f },
    {-0.5f,  0.5f, -0.5f }, { 0.5f,  0.5f, -0.5f },
};

static const PackedFloat3 kNormals[24] =
{
    { 1, 0, 0},{ 1, 0, 0},{ 1, 0, 0},{ 1, 0, 0},  // +X
    {-1, 0, 0},{-1, 0, 0},{-1, 0, 0},{-1, 0, 0},  // -X
    { 0, 1, 0},{ 0, 1, 0},{ 0, 1, 0},{ 0, 1, 0},  // +Y
    { 0,-1, 0},{ 0,-1, 0},{ 0,-1, 0},{ 0,-1, 0},  // -Y
    { 0, 0, 1},{ 0, 0, 1},{ 0, 0, 1},{ 0, 0, 1},  // +Z
    { 0, 0,-1},{ 0, 0,-1},{ 0, 0,-1},{ 0, 0,-1},  // -Z
};

static const PackedFloat2 kUVs[24] =
{
    {0,0},{1,0},{1,1},{0,1},  // +X
    {0,0},{1,0},{1,1},{0,1},  // -X
    {0,0},{1,0},{1,1},{0,1},  // +Y
    {0,0},{1,0},{1,1},{0,1},  // -Y
    {0,0},{1,0},{1,1},{0,1},  // +Z
    {0,0},{1,0},{1,1},{0,1},  // -Z
};

static constexpr uint32_t kIndices[36] =
{
     0, 1, 2,  0, 2, 3,   // +X
     4, 5, 6,  4, 6, 7,   // -X
     8,10, 9,  8,11,10,   // +Y
    12,14,13, 12,15,14,   // -Y
    16,17,18, 16,18,19,   // +Z
    20,21,22, 20,22,23,   // -Z
};

// ─────────────────────────────────────────────────────────────────────────────
// MaterialRecipe — Standard preset (PBR default), BaseColor + Normal textures.
// attribute_providers forces Normal + TexCoord0 onto SSBO fetch.
// position_provider forces Position onto SSBO fetch.
// Lambda initializer required: array indices cannot use designated-initializers.
// ─────────────────────────────────────────────────────────────────────────────

static const mtl::MaterialRecipe kPullRecipe = []()
{
    mtl::MaterialRecipe r;
    r.id       = "vertex_pulling_cube";
    r.preset   = mtl::MaterialPreset::Standard;
    r.pipeline = GraphicsPipelinePreset::Solid3D;
    r.textures = {
        { mtl::SamplerSlot::BaseColor, mtl::TextureSourceMode::Simple,
          "res/image/Brickwall/Albedo.Tex2D" },
        { mtl::SamplerSlot::Normal, mtl::TextureSourceMode::Simple,
          "res/image/Brickwall/Normal.Tex2D" },
    };
    r.attribute_providers[size_t(AttributeSemantic::Normal)]    = AttributeProviderId::SSBO_Vec3;
    r.attribute_providers[size_t(AttributeSemantic::TexCoord0)] = AttributeProviderId::SSBO_Vec2;
    r.position_provider = PositionProviderId::SSBO_PackedVec3;
    return r;
}();

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────

class VertexPullingCubeApp : public WorkObject
{
    ECSContext *  ecs_context_   = nullptr;
    Entity *      cube_entity_   = nullptr;
    Entity *      camera_entity_ = nullptr;

    Geometry * geometry_  = nullptr;
    VAB *      pos_vab_   = nullptr;
    VAB *      norm_vab_  = nullptr;
    VAB *      uv_vab_    = nullptr;

    std::shared_ptr<TransformComponent> cube_transform_;

    float theta_ = 0.0f;

private:

    // ------------------------------------------------------------------
    // Create three storage-backed VABs and an index-only Geometry.
    // ------------------------------------------------------------------
    bool CreateSSBOs()
    {
        auto *device = GetDevice();
        if (!device)
            return false;

        pos_vab_ = device->CreateVAB(VF_V3F, 24, kPositions,
                                     BufferAllocPolicy::Auto,
                                     SharingMode::Exclusive,
                                     BufferUpdateClass::Default,
                                     std::source_location::current(),
                                     /*prefer_storage_usage=*/true);
        if (!pos_vab_) return false;

        norm_vab_ = device->CreateVAB(VF_V3F, 24, kNormals,
                                      BufferAllocPolicy::Auto,
                                      SharingMode::Exclusive,
                                      BufferUpdateClass::Default,
                                      std::source_location::current(),
                                      /*prefer_storage_usage=*/true);
        if (!norm_vab_) return false;

        uv_vab_ = device->CreateVAB(VF_V2F, 24, kUVs,
                                    BufferAllocPolicy::Auto,
                                    SharingMode::Exclusive,
                                    BufferUpdateClass::Default,
                                    std::source_location::current(),
                                    /*prefer_storage_usage=*/true);
        if (!uv_vab_) return false;

        // Index-only geometry: no vertex attribute writes ({} empty list).
        geometry_ = CreateGeometry("pulling_cube", 24, 36,
                                   IndexType::U32, {}, kIndices);
        return geometry_ != nullptr;
    }

    // ------------------------------------------------------------------
    // Create the MI, configure SSBO pulling on the SMP, and wire up the
    // Primitive onto the pre-created ECS PrimitiveComponent.
    // ------------------------------------------------------------------
    bool BindMaterial()
    {
        mtl::StandardMaterialInstance mi_data{};
        mi_data.base_color = 0xFFFFFFFFu;  // white
        mi_data.metallic   = 0.0f;
        mi_data.roughness  = 0.9f;
        mi_data.normal_scale = 0.0f;

        auto *mi = ResolveOrCreateBindingInstance(kPullRecipe, &mi_data, sizeof(mi_data));
        if (!mi)
            return false;

        auto *smp = MaterialBindingInstanceInternalAccess::GetShaderMaterialProgram(mi);
        if (!smp)
            return false;

        // Pipeline side: VIL returns nullptr → vkCmdBindVertexBuffers count = 0.
        smp->SetPullingEnabled(true);

        auto *pm = GetPrimitiveManager();
        if (!pm)
            return false;

        auto *prim = pm->CreatePrimitive(geometry_, mi);
        if (!prim)
            return false;

        if (!prim->SetVertexStreamSource(AttributeSemantic::BuiltinCount, pos_vab_->GetGPUBuffer(), 0, 12))
            return false;
        if (!prim->SetVertexStreamSource(AttributeSemantic::Normal, norm_vab_->GetGPUBuffer(), 0, 12))
            return false;
        if (!prim->SetVertexStreamSource(AttributeSemantic::TexCoord0, uv_vab_->GetGPUBuffer(), 0, 8))
            return false;

        auto prim_comp = cube_entity_->GetComponent<PrimitiveComponent>();
        if (!prim_comp)
            return false;

        prim_comp->SetPrimitive(prim);
        prim_comp->SetVisible(true);
        return true;
    }

    // ------------------------------------------------------------------
    // ECS setup: cube entity with an animated TransformComponent.
    // PrimitiveComponent is added here; its Primitive is set in BindMaterial.
    // ------------------------------------------------------------------
    bool InitECS()
    {
        ecs_context_ = GetECSContext();
        if (!ecs_context_)
            return false;

        cube_entity_ = ecs_context_->CreateEntity<Entity>("VPCubeEntity");

        auto transform = cube_entity_->AddComponent<TransformComponent>(Mobility::Static);
        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(true);  // Allow SetLocalRotation() in Render()
        cube_transform_ = transform;

        cube_entity_->AddComponent<PrimitiveComponent>();

        return true;
    }

    // ------------------------------------------------------------------
    // ViewModel camera orbiting the cube from a comfortable angle.
    // ------------------------------------------------------------------
    bool InitCamera()
    {
        if (!ecs_context_ || !ecs_context_->EnsureCameraSystem())
            return false;

        camera_entity_ = ecs_context_->CreateEntity<Entity>("MainCamera");
        auto camera = camera_entity_->AddComponent<CameraComponent>();

        camera->control_mode   = CameraComponent::ControlMode::ViewModel;
        camera->target         = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance       = 4.0f;
        camera->yaw            = 45.0f;
        camera->pitch          = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty   = true;

        camera->camera_data    = GetCamera();
        camera->camera_info    = const_cast<graph::CameraInfo *>(GetCameraInfo());
        camera->viewport_info  = GetViewportInfo();

        return true;
    }

public:

    bool Init() override
    {
        SetClearColor(Color4f(0.18f, 0.18f, 0.20f, 1.0f));

        // InitECS must run first so cube_entity_ exists before BindMaterial.
        if (!InitECS())
            return false;

        if (!CreateSSBOs())
            return false;

        if (!BindMaterial())
            return false;

        if (!InitCamera())
            return false;

        return true;
    }

    void Render(double delta_time) override
    {
        theta_ += float(delta_time) * 0.8f;
        theta_  = std::fmod(theta_, 2.0f * std::numbers::pi_v<float>);

        if (cube_transform_)
        {
            auto rot_z = glm::angleAxis(theta_,        glm::vec3(0.0f, 0.0f, 1.0f));
            auto rot_x = glm::angleAxis(theta_ * 0.5f, glm::vec3(1.0f, 0.0f, 0.0f));
            cube_transform_->SetLocalRotation(rot_z * rot_x);
        }

        WorkObject::Render(delta_time);
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<VertexPullingCubeApp>(OS_TEXT("Vertex Pulling Cube (§C.5)"), argc, argv, 1280, 720);
}
