// example/Basic/VertexPulling_Sphere.cpp
//
// Vertex pulling validation sample (sphere):
//  - Position / Normal / TexCoord0 are fetched from SSBO streams.
//  - No vertex buffers are bound at draw time.

#include<hgl/framework/WorkManager.h>
#include<hgl/graph/geo/GraphicsGeometryFactory.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/MaterialBindingInstanceInternalAccess.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/vk/VKIndexBuffer.h>
#include<hgl/vk/VKShaderMaterialProgram.h>
#include<hgl/vk/VKMaterialParameters.h>
#include<hgl/mtl/Material3DCreateConfig.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/components/PrimitiveComponent.h>
#include<hgl/ecs/components/CameraComponent.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

#include<cmath>
#include<cstdint>
#include<numbers>
#include<vector>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

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

static const mtl::MaterialRecipe kPullRecipe = []()
{
    mtl::MaterialRecipe r;
    r.id       = "vertex_pulling_sphere";
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
    r.use_mesh_shader = true;
    return r;
}();

} // anonymous namespace

class VertexPullingSphereApp : public WorkObject
{
    ECSContext *  ecs_context_   = nullptr;
    Entity *      sphere_entity_ = nullptr;
    Entity *      camera_entity_ = nullptr;

    Geometry * geometry_  = nullptr;
    VAB *      pos_vab_   = nullptr;
    VAB *      norm_vab_  = nullptr;
    VAB *      uv_vab_    = nullptr;

    std::vector<PackedFloat3> positions_;
    std::vector<PackedFloat3> normals_;
    std::vector<PackedFloat2> uvs_;
    std::vector<uint32_t>     indices_;

    std::shared_ptr<TransformComponent> sphere_transform_;

    float theta_ = 0.0f;

private:

    bool BuildSphereMeshData(uint32_t slices, uint32_t stacks, float radius)
    {
        if (slices < 3 || stacks < 3)
            return false;

        const uint32_t ring = slices + 1;
        const uint32_t vertex_count = ring * (stacks + 1);

        if (vertex_count > 65535u)
            return false;

        positions_.resize(vertex_count);
        normals_.resize(vertex_count);
        uvs_.resize(vertex_count);

        const float kTwoPi = 2.0f * std::numbers::pi_v<float>;

        for (uint32_t y = 0; y <= stacks; ++y)
        {
            const float v = float(y) / float(stacks);
            const float phi = v * std::numbers::pi_v<float>;
            const float sin_phi = std::sin(phi);
            const float cos_phi = std::cos(phi);

            for (uint32_t x = 0; x <= slices; ++x)
            {
                const float u = float(x) / float(slices);
                const float theta = u * kTwoPi;
                const float sin_theta = std::sin(theta);
                const float cos_theta = std::cos(theta);

                const float nx = sin_phi * cos_theta;
                const float ny = cos_phi;
                const float nz = sin_phi * sin_theta;

                const uint32_t i = y * ring + x;

                normals_[i] = { nx, ny, nz };
                positions_[i] = { nx * radius, ny * radius, nz * radius };
                uvs_[i] = { u, v };
            }
        }

        indices_.clear();
        indices_.reserve(slices * (stacks - 1) * 6);

        // Triangles are generated with the same winding convention that now works
        // with the project's current VK_FRONT_FACE_CLOCKWISE setup.
        for (uint32_t y = 0; y < stacks; ++y)
        {
            for (uint32_t x = 0; x < slices; ++x)
            {
                const uint32_t i0 = y * ring + x;
                const uint32_t i1 = i0 + 1;
                const uint32_t i2 = i0 + ring;
                const uint32_t i3 = i2 + 1;

                if (y != 0)
                {
                    indices_.push_back(static_cast<uint32_t>(i0));
                    indices_.push_back(static_cast<uint32_t>(i1));
                    indices_.push_back(static_cast<uint32_t>(i2));
                }

                if (y != stacks - 1)
                {
                    indices_.push_back(static_cast<uint32_t>(i1));
                    indices_.push_back(static_cast<uint32_t>(i3));
                    indices_.push_back(static_cast<uint32_t>(i2));
                }
            }
        }

        return !indices_.empty();
    }

    bool CreateSSBOs()
    {
        if (!BuildSphereMeshData(48, 24, 0.8f))
            return false;

        auto *device = GetDevice();
        if (!device)
            return false;

        pos_vab_ = device->CreateVAB(VF_V3F,
                                     static_cast<uint32_t>(positions_.size()),
                                     positions_.data(),
                                     BufferAllocPolicy::Auto,
                                     SharingMode::Exclusive,
                                     BufferUpdateClass::Default,
                                     std::source_location::current(),
                                     /*prefer_storage_usage=*/true);
        if (!pos_vab_)
            return false;

        norm_vab_ = device->CreateVAB(VF_V3F,
                                      static_cast<uint32_t>(normals_.size()),
                                      normals_.data(),
                                      BufferAllocPolicy::Auto,
                                      SharingMode::Exclusive,
                                      BufferUpdateClass::Default,
                                      std::source_location::current(),
                                      /*prefer_storage_usage=*/true);
        if (!norm_vab_)
            return false;

        uv_vab_ = device->CreateVAB(VF_V2F,
                                    static_cast<uint32_t>(uvs_.size()),
                                    uvs_.data(),
                                    BufferAllocPolicy::Auto,
                                    SharingMode::Exclusive,
                                    BufferUpdateClass::Default,
                                    std::source_location::current(),
                                    /*prefer_storage_usage=*/true);
        if (!uv_vab_)
            return false;

        geometry_ = CreateGeometry("pulling_sphere",
                                   static_cast<uint32_t>(positions_.size()),
                                   static_cast<uint32_t>(indices_.size()),
                                   IndexType::U32,
                                   {},
                                   indices_.data());

        return geometry_ != nullptr;
    }

    bool BindMaterial()
    {
        mtl::StandardMaterialInstance mi_data{};
        mi_data.base_color = 0xFFFFFFFFu;
        mi_data.metallic = 0.0f;
        mi_data.roughness = 0.9f;
        mi_data.normal_scale = 0.0f;

        auto *mi = ResolveOrCreateBindingInstance(kPullRecipe, &mi_data, sizeof(mi_data));
        if (!mi)
            return false;

        auto *smp = MaterialBindingInstanceInternalAccess::GetShaderMaterialProgram(mi);
        if (!smp)
            return false;

        bool material_uses_mesh_stage = false;
        for (const auto &stage_ci : smp->GetStageList())
        {
            if (stage_ci.stage == VK_SHADER_STAGE_MESH_BIT_EXT)
            {
                material_uses_mesh_stage = true;
                break;
            }
        }

        smp->SetPullingEnabled(true);

        auto *mp = smp->GetMP(SET_TYPE_VERTEX_STREAMS);
        if (!mp)
            return false;

        if (!mp->BindAttribSSBO(AttributeSemantic::BuiltinCount, pos_vab_->GetGPUBuffer()))
            return false;

        if (!smp->BindAttribStream(AttributeSemantic::Normal, norm_vab_->GetGPUBuffer(), 0, 12))
            return false;

        if (!smp->BindAttribStream(AttributeSemantic::TexCoord0, uv_vab_->GetGPUBuffer(), 0, 8))
            return false;

        if (material_uses_mesh_stage)
        {
            auto *ibo = geometry_ ? geometry_->GetIBO() : nullptr;
            if (!ibo || !smp->BindMeshIndexStream(ibo->GetGPUBuffer()))
                return false;
        }

        auto *pm = GetPrimitiveManager();
        if (!pm)
            return false;

        auto *prim = pm->CreatePrimitive(geometry_, mi);
        if (!prim)
            return false;

        if (material_uses_mesh_stage)
        {
            const uint32_t mesh_group_count_x = geometry_->GetIndexCount() / 3u;
            if (mesh_group_count_x == 0 || !prim->SetMeshTaskGroupCounts(mesh_group_count_x, 1u, 1u))
                return false;
        }

        auto prim_comp = sphere_entity_->GetComponent<PrimitiveComponent>();
        if (!prim_comp)
            return false;

        prim_comp->SetPrimitive(prim);
        prim_comp->SetVisible(true);
        return true;
    }

    bool InitECS()
    {
        ecs_context_ = GetECSContext();
        if (!ecs_context_)
            return false;

        sphere_entity_ = ecs_context_->CreateEntity<Entity>("VPSphereEntity");

        auto transform = sphere_entity_->AddComponent<TransformComponent>(Mobility::Static);
        transform->SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
        transform->SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        transform->SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
        transform->SetMovable(true);
        sphere_transform_ = transform;

        sphere_entity_->AddComponent<PrimitiveComponent>();

        return true;
    }

    bool InitCamera()
    {
        if (!ecs_context_ || !ecs_context_->EnsureCameraSystem())
            return false;

        camera_entity_ = ecs_context_->CreateEntity<Entity>("MainCamera");
        auto camera = camera_entity_->AddComponent<CameraComponent>();

        camera->control_mode = CameraComponent::ControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance = 3.5f;
        camera->yaw = 35.0f;
        camera->pitch = -15.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->camera_data = GetCamera();
        camera->camera_info = const_cast<graph::CameraInfo *>(GetCameraInfo());
        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:

    bool Init() override
    {
        SetClearColor(Color4f(0.18f, 0.18f, 0.20f, 1.0f));

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
        theta_ += float(delta_time) * 0.6f;
        theta_ = std::fmod(theta_, 2.0f * std::numbers::pi_v<float>);

        if (sphere_transform_)
        {
            auto rot_y = glm::angleAxis(theta_, glm::vec3(0.0f, 1.0f, 0.0f));
            auto rot_x = glm::angleAxis(theta_ * 0.25f, glm::vec3(1.0f, 0.0f, 0.0f));
            sphere_transform_->SetLocalRotation(rot_y * rot_x);
        }

        WorkObject::Render(delta_time);
    }
};

int os_main(int argc, os_char **argv)
{
    return RunFramework<VertexPullingSphereApp>(OS_TEXT("Vertex Pulling Sphere"), argc, argv, 1280, 720);
}
