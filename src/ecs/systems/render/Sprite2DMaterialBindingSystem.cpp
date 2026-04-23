#include<hgl/ecs/systems/render/Sprite2DMaterialBindingSystem.h>
#include<hgl/ecs/systems/render/Sprite2DResourcePrepareSystem.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/components/Sprite2DComponent.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/PrimitiveManager.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/vk/VKMaterialBindingInstance.h>
#include<cstdint>
#include<cstring>
#include<vector>

namespace hgl::ecs
{
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
            return;

        // Obtain the shared unit-square geometry from Step 2's system
        auto resource_system = world->GetSystem<Sprite2DResourcePrepareSystem>();
        if (!resource_system)
            return;

        auto* shared_geometry = resource_system->GetSharedGeometry();
        if (!shared_geometry)
            return;

        std::vector<Entity*> entities;
        world->GetAllEntities(entities);

        for (Entity* entity : entities)
        {
            if (!entity)
                continue;

            auto sprite = entity->GetComponent<Sprite2DComponent>();
            if (!sprite)
                continue;

            if (!sprite->IsVisible())
                continue;

            EnsureSpriteMaterial(sprite.get());
        }
    }

    bool Sprite2DMaterialBindingSystem::EnsureSpriteMaterial(Sprite2DComponent* sprite)
    {
        if (!sprite || !world)
            return false;

        const auto& texture_path = sprite->GetTexturePath();
        if (texture_path.IsEmpty())
            return true;

        // Skip if texture is unchanged and a sprite-specific primitive is already bound
        auto resource_system = world->GetSystem<Sprite2DResourcePrepareSystem>();
        if (!resource_system)
            return false;

        auto* shared_geometry = resource_system->GetSharedGeometry();
        if (!shared_geometry)
            return false;

        // If texture hasn't changed and a valid primitive is already assigned, skip
        if (!sprite->IsTextureDirty() && sprite->GetPrimitive() != nullptr)
            return true;

        auto* graphics_context = world->GetGraphicsContext();
        if (!graphics_context)
            return false;

        auto* material_manager = graphics_context->GetMaterialManager();
        auto* primitive_manager = graphics_context->GetPrimitiveManager();
        auto* texture_manager = graphics_context->GetTextureManager();
        if (!material_manager || !primitive_manager || !texture_manager)
            return false;

        // Load texture
        auto* texture = texture_manager->LoadTexture2D(texture_path, true);
        if (!texture)
            return false;

        // TODO (Step 4): Replace stub with real Sprite2D material creation via
        //               material_manager->ResolveOrCreateProgram(Sprite2DPreset, &cfg).
        //               For now we cannot create a Sprite2D ShaderMaterialProgram
        //               without the pipeline preset introduced in Step 4, so we
        //               simply return true without creating an MI.
        //               The sprite will not render until Step 4 wires this up.
        (void)material_manager;
        (void)primitive_manager;
        (void)texture;

        sprite->SetTextureObjects(texture, nullptr);
        sprite->SetAppliedTexturePath(texture_path);
        return true;
    }
}//namespace hgl::ecs
