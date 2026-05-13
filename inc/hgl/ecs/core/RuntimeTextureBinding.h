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

        enum class Status
        {
            None,
            Pending,
            Ready,
            Failed,
        };

        uint64_t binding_id = 0;
        uint32_t generation = 0;
        Kind kind = Kind::None;
        Status status = Status::None;
        hgl::graph::Texture* texture = nullptr;
        hgl::graph::Sampler* sampler = nullptr;
        hgl::graph::DomainResourceBinding* domain_binding = nullptr;
        hgl::graph::ResourceDomain* domain = nullptr;
        uint32_t layer = 0xFFFFFFFFu;
        bool ready = false;

        bool IsReady() const { return ready && status == Status::Ready; }
        bool HasTexture() const { return texture != nullptr; }
        bool HasSampler() const { return sampler != nullptr; }
        bool HasDomainBinding() const { return domain_binding != nullptr; }

        void Reset()
        {
            binding_id = 0;
            generation = 0;
            kind = Kind::None;
            status = Status::None;
            texture = nullptr;
            sampler = nullptr;
            domain_binding = nullptr;
            domain = nullptr;
            layer = 0xFFFFFFFFu;
            ready = false;
        }
    };
}
