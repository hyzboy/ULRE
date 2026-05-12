#pragma once

#include<hgl/ecs/core/System.h>

namespace hgl::ecs
{
    class ECSContext;
    class TextureBindingComponent;

    class TextureMaterialBindingSystem : public System
    {
    private:

        ECSContext *world = nullptr;

    public:

        TextureMaterialBindingSystem(const std::string& name = "TextureMaterialBindingSystem");
        ~TextureMaterialBindingSystem() override = default;

    public:

        void SetWorld(ECSContext *w) { world = w; }

        void Update(float deltaTime) override;

    private:

        bool EnsurePrimitiveTextureBinding(class PrimitiveComponent *primitive,
                                           TextureBindingComponent *binding);
    };
}//namespace hgl::ecs
