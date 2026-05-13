#pragma once

#include <cstdint>

namespace hgl
{
    namespace graph
    {
        class Texture;
        class Sampler;
        class DomainResourceBinding;
        class ResourceDomain;
    }
}

namespace hgl::ecs
{
    struct RuntimeTextureBinding
    {
        enum class Kind
        {
            None,
            SingleTexture,
            TextureArray,
        };

        Kind kind = Kind::None;
        hgl::graph::Texture* texture = nullptr;
        hgl::graph::Sampler* sampler = nullptr;
        hgl::graph::DomainResourceBinding* domain_binding = nullptr;
        hgl::graph::ResourceDomain* domain = nullptr;
        uint32_t layer = 0xFFFFFFFFu;
        bool ready = false;

        void Reset()
        {
            kind = Kind::None;
            texture = nullptr;
            sampler = nullptr;
            domain_binding = nullptr;
            domain = nullptr;
            layer = 0xFFFFFFFFu;
            ready = false;
        }
    };
}
