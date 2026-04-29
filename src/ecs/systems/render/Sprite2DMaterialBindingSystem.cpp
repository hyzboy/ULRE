#include<hgl/ecs/systems/render/Sprite2DMaterialBindingSystem.h>
#include<hgl/ecs/systems/render/Sprite2DResourcePrepareSystem.h>
#include<hgl/ecs/systems/render/RenderDescriptorBindingSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/Sprite2DComponent.h>
#include<hgl/mtl/Sprite2DMaterialCreateConfig.h>
#include<hgl/mtl/MaterialPreset.h>
#include<hgl/mtl/SamplerSlot.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/ShaderMaterialProgramManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/vk/VKMaterialBindingInstance.h>
#include<hgl/vk/pipeline/VKGraphicsPipelinePreset.h>
#include<cstdint>
#include<cstring>
#include<vector>
#include<cstdio>

namespace hgl::ecs
{
    // ─── Local helpers (mirror QuadMaterialBindingSystem) ─────────────────────

    struct SpriteResolvedMaterialState
    {
        graph::MaterialBindingInstance*  binding_instance = nullptr;
        graph::ShaderMaterialProgram*    material         = nullptr;
    };

    static SpriteResolvedMaterialState ResolveMaterialInstanceState(
        graph::MaterialBindingInstance* mi,
        graph::ShaderMaterialProgram*   expected_material = nullptr)
    {
        SpriteResolvedMaterialState state{};
        state.binding_instance = mi;
        state.material = expected_material;
        return state;
    }

    static graph::ShaderMaterialProgram* ResolvePrimitiveMaterialStateFirst(graph::Primitive* prim)
    {
        if (!prim) return nullptr;
        auto* mi = prim->GetResolvedBindingInstance();
        return ResolveMaterialInstanceState(mi).material;
    }

    static graph::ResourceDomain* ResolveDomainForMaterial(graph::GraphicsContext* gc,
                                                           graph::ShaderMaterialProgram* material,
                                                           uint32_t domain_id)
    {
        if (!material) return nullptr;
        auto* rdm = gc ? gc->GetResourceDomainManager() : nullptr;
        if (!rdm)   return nullptr;

        const auto schema = material->GetShaderDataSchema();
        if (auto* domain = rdm->Get(schema, domain_id))
            return domain;

        graph::ResourceDomainCreateInfo ci;
        ci.schema           = schema;
        ci.domain_id        = domain_id;
        ci.initial_capacity = 256;
        return rdm->Create(ci);
    }

    // ──────────────────────────────────────────────────────────────────────────

    /// Per-instance shader data written to the Sprite2DTransform SSBO.
    /// std430 layout — must match schema_sprite2d_transform.glsl exactly.
    /// Total: 32 bytes.
    struct Sprite2DTransform
    {
        glm::vec2   size;           //  8 bytes  offset  0
        glm::vec2   pivot;          //  8 bytes  offset  8
        float       rotation;       //  4 bytes  offset 16
        uint32_t    tint_rgba8;     //  4 bytes  offset 20
        uint32_t    flags;          //  4 bytes  offset 24
        uint32_t    _pad0;          //  4 bytes  offset 28
    };

    static_assert(sizeof(Sprite2DTransform) == 32, "Sprite2DTransform must be 32 bytes");

    // Pack RGBA u8vec4 into a single uint32 (R in LSB).
    static inline uint32_t PackRGBA8(const glm::u8vec4& c)
    {
        return (static_cast<uint32_t>(c.r))
             | (static_cast<uint32_t>(c.g) << 8)
             | (static_cast<uint32_t>(c.b) << 16)
             | (static_cast<uint32_t>(c.a) << 24);
    }

    Sprite2DMaterialBindingSystem::Sprite2DMaterialBindingSystem(const std::string& name)
        : System(name)
    {
        SetSystemType(SystemType::ShaderMaterialProgram);
        SetExecutionOrder(ExecutionPhase::RenderMaterialBind);
        SetRenderElementType("Sprite2D");
        AddDependency<Sprite2DResourcePrepareSystem>();
    }

    void Sprite2DMaterialBindingSystem::Update(float deltaTime)
    {
        if (!world)
        {
            std::fprintf(stderr, "[Sprite2DBindSys] Update: world is null\n");
            return;
        }

        // Obtain the shared unit-square geometry from Step 2's system
        auto resource_system = world->GetSystem<Sprite2DResourcePrepareSystem>();
        if (!resource_system)
        {
            std::fprintf(stderr, "[Sprite2DBindSys] Update: Sprite2DResourcePrepareSystem not found\n");
            return;
        }

        auto* shared_geometry = resource_system->GetSharedGeometry();
        if (!shared_geometry)
        {
            std::fprintf(stderr, "[Sprite2DBindSys] Update: shared_geometry is null\n");
            return;
        }

        std::vector<Entity*> entities;
        world->GetAllEntities(entities);

        std::fprintf(stderr, "[Sprite2DBindSys] Update: %zu entities total\n", entities.size());

        int sprite_count = 0;
        int processed_count = 0;
        for (Entity* entity : entities)
        {
            if (!entity)
                continue;

            auto sprite = entity->GetComponent<Sprite2DComponent>();
            if (!sprite)
                continue;

            ++sprite_count;

            if (!sprite->IsVisible())
            {
                std::fprintf(stderr, "[Sprite2DBindSys] Update: sprite entity not visible, skipping\n");
                continue;
            }

            ++processed_count;
            const bool ok = EnsureSpriteMaterial(sprite.get());
            std::fprintf(stderr, "[Sprite2DBindSys] Update: EnsureSpriteMaterial -> %s\n", ok ? "OK" : "FAILED");
        }

        std::fprintf(stderr, "[Sprite2DBindSys] Update: sprite_count=%d processed=%d\n",
                     sprite_count, processed_count);
    }

    bool Sprite2DMaterialBindingSystem::EnsureSpriteMaterial(Sprite2DComponent* sprite)
    {
        if (!sprite || !world)
        {
            std::fprintf(stderr, "[Sprite2DBindSys] EnsureSpriteMaterial: sprite or world is null\n");
            return false;
        }

        const auto& texture_path = sprite->GetTexturePath();
        if (texture_path.IsEmpty())
        {
            std::fprintf(stderr, "[Sprite2DBindSys] EnsureSpriteMaterial: texture_path is empty, skip\n");
            return true;
        }

        std::fprintf(stderr, "[Sprite2DBindSys] EnsureSpriteMaterial: texture_path='%ls'\n",
                     texture_path.c_str());

        auto resource_system = world->GetSystem<Sprite2DResourcePrepareSystem>();
        if (!resource_system)
        {
            std::fprintf(stderr, "[Sprite2DBindSys] EnsureSpriteMaterial: no Sprite2DResourcePrepareSystem\n");
            return false;
        }

        auto* shared_geometry = resource_system->GetSharedGeometry();
        if (!shared_geometry)
        {
            std::fprintf(stderr, "[Sprite2DBindSys] EnsureSpriteMaterial: shared_geometry is null\n");
            return false;
        }

        // Skip if texture is unchanged and a valid primitive is already assigned
        if (!sprite->IsTextureDirty() && sprite->GetPrimitive() != nullptr)
        {
            std::fprintf(stderr, "[Sprite2DBindSys] EnsureSpriteMaterial: not dirty & primitive exists, skip\n");
            return true;
        }

        auto* graphics_context = world->GetGraphicsContext();
        if (!graphics_context)
        {
            std::fprintf(stderr, "[Sprite2DBindSys] EnsureSpriteMaterial: graphics_context is null\n");
            return false;
        }

        auto* material_manager  = graphics_context->GetMaterialManager();
        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        auto* texture_manager   = graphics_context->GetTextureManager();
        if (!material_manager || !primitive_manager || !texture_manager)
        {
            std::fprintf(stderr, "[Sprite2DBindSys] EnsureSpriteMaterial: missing manager: mat=%p prim=%p tex=%p\n",
                         (void*)material_manager, (void*)primitive_manager, (void*)texture_manager);
            return false;
        }

        // Load texture
        auto* texture = texture_manager->LoadTexture2D(texture_path, true);
        if (!texture)
        {
            std::fprintf(stderr, "[Sprite2DBindSys] EnsureSpriteMaterial: LoadTexture2D FAILED for '%ls'\n",
                         texture_path.c_str());
            return false;
        }
        std::fprintf(stderr, "[Sprite2DBindSys] EnsureSpriteMaterial: texture loaded OK (%p)\n", (void*)texture);

        // Get shared sampler
        auto* shared_sampler = resource_system->GetSharedSampler();
        if (!shared_sampler)
        {
            std::fprintf(stderr, "[Sprite2DBindSys] EnsureSpriteMaterial: shared_sampler is null\n");
            return false;
        }

        // Build material config
        const bool fixed = sprite->IsFixedSize();
        std::fprintf(stderr, "[Sprite2DBindSys] EnsureSpriteMaterial: fixed_size=%d\n", (int)fixed);

        graph::mtl::Sprite2DMaterialCreateConfig cfg{};
        cfg.fixed_size         = fixed;
        cfg.axis_locked        = fixed;
        cfg.blend_mode         = graph::RenderAlphaMode::Transparent;
        cfg.base_color_channel = graph::TextureChannelHint::RGBA;

        const auto preset = fixed
            ? graph::mtl::MaterialPreset::Sprite2DAxisLocked
            : graph::mtl::MaterialPreset::Sprite2DCameraFacing;

        std::fprintf(stderr, "[Sprite2DBindSys] EnsureSpriteMaterial: calling ResolveOrCreateProgram preset=%d\n",
                     (int)preset);
        auto* sprite_material = material_manager->ResolveOrCreateProgram(preset, &cfg);
        if (!sprite_material)
        {
            std::fprintf(stderr, "[Sprite2DBindSys] EnsureSpriteMaterial: ResolveOrCreateProgram FAILED\n");
            return false;
        }
        std::fprintf(stderr, "[Sprite2DBindSys] EnsureSpriteMaterial: ResolveOrCreateProgram OK (%p)\n",
                     (void*)sprite_material);

        graph::MaterialInstanceSpec spec;
        spec.material = sprite_material;
        spec.domain   = ResolveDomainForMaterial(graphics_context, sprite_material, 2u);
        spec.preset   = graph::GraphicsPipelinePreset::Alpha3D;
        std::fprintf(stderr, "[Sprite2DBindSys] EnsureSpriteMaterial: domain=%p, calling AcquireMaterialInstance\n",
                     (void*)spec.domain);
        auto* mi = material_manager->AcquireMaterialInstance(spec);
        if (!mi)
        {
            std::fprintf(stderr, "[Sprite2DBindSys] EnsureSpriteMaterial: AcquireMaterialInstance FAILED\n");
            return false;
        }
        std::fprintf(stderr, "[Sprite2DBindSys] EnsureSpriteMaterial: AcquireMaterialInstance OK (%p)\n", (void*)mi);

        const auto mi_state = ResolveMaterialInstanceState(mi, sprite_material);
        if (!mi_state.material)
        {
            std::fprintf(stderr, "[Sprite2DBindSys] EnsureSpriteMaterial: mi_state.material is null\n");
            return false;
        }

        graph::ShaderMaterialProgram* previous_material = nullptr;
        {
            auto* current_prim = sprite->GetPrimitive();
            if (current_prim)
                previous_material = ResolvePrimitiveMaterialStateFirst(current_prim);
        }

        mi->SetRenderPreset(graph::GraphicsPipelinePreset::Alpha3D);

        auto* current_primitive = sprite->GetPrimitive();
        graph::Primitive* sprite_primitive = nullptr;

        if (current_primitive
         && ResolvePrimitiveMaterialStateFirst(current_primitive) == mi_state.material)
        {
            if (!current_primitive->ChangeMaterialInstance(mi))
                return false;
            sprite_primitive = current_primitive;
        }
        else
        {
            sprite_primitive = primitive_manager->CreatePrimitive(shared_geometry, mi);
            if (!sprite_primitive)
                return false;

            if (current_primitive)
                primitive_manager->Release(current_primitive);
        }

        graph::ShaderMaterialProgram* material = mi_state.material;

        std::fprintf(stderr,
            "[Sprite2DBindSys] BindResourceSampler: material=%p texture=%p sampler=%p\n",
            (void*)material,
            (void*)texture,
            (void*)shared_sampler);

        if (!material->BindResourceSampler(graph::mtl::SamplerSlot::BaseColor, texture, shared_sampler))
        {
            std::fprintf(stderr, "[Sprite2DBindSys] EnsureSpriteMaterial: BindResourceSampler FAILED"
                " (material=%p texture=%p sampler=%p"
                " slot=BaseColor(0))\n",
                (void*)material, (void*)texture, (void*)shared_sampler);
            return false;
        }
        std::fprintf(stderr, "[Sprite2DBindSys] EnsureSpriteMaterial: BindResourceSampler OK\n");

        if (auto descriptor_binding_system = world->GetSystem<RenderDescriptorBindingSystem>())
        {
            if (previous_material && previous_material != material)
                descriptor_binding_system->ClearMaterialBindings(previous_material);

            descriptor_binding_system->RegisterMaterialTextureSampler(
                material,
                graph::mtl::SamplerSlot::BaseColor,
                texture,
                shared_sampler);
        }

        // Write per-sprite transform to MI data
        Sprite2DTransform transform{};
        transform.size       = fixed
            ? glm::vec2(float(sprite->GetPixelSize().x), float(sprite->GetPixelSize().y))
            : sprite->GetWorldSize();
        transform.pivot      = sprite->GetPivot();
        transform.rotation   = sprite->GetRotation();
        transform.tint_rgba8 = PackRGBA8(sprite->GetTint());
        transform.flags      = fixed ? 1u : 0u;
        transform._pad0      = 0u;
        mi->WriteMIData(transform);

        std::fprintf(stderr, "[Sprite2DBindSys] EnsureSpriteMaterial: SetPrimitive...\n");
        sprite->SetPrimitive(sprite_primitive);
        std::fprintf(stderr, "[Sprite2DBindSys] EnsureSpriteMaterial: SetTextureObjects...\n");
        sprite->SetTextureObjects(texture, shared_sampler);
        std::fprintf(stderr, "[Sprite2DBindSys] EnsureSpriteMaterial: SetAppliedTexturePath...\n");
        sprite->SetAppliedTexturePath(texture_path);
        std::fprintf(stderr, "[Sprite2DBindSys] EnsureSpriteMaterial: SUCCESS primitive=%p\n",
                     (void*)sprite_primitive);
        return true;
    }
}//namespace hgl::ecs
