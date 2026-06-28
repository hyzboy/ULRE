#pragma once

#include <cstdint>
#include <vector>

namespace hgl::graph
{
    class ShaderMaterialProgram;
    class ResourceDomain;

    using MaterialPayloadID = uint64_t;
    using ProgramBindingID = uint64_t;

    // Phase R1 skeleton: payload is a standalone instance-data object.
    struct MaterialInstancePayload
    {
        MaterialPayloadID id = 0;
        uint32_t schema_version = 0;
        uint32_t domain_id = 0xFFFFFFFFu;
        uint64_t instance_hash = 0;
        std::vector<uint8_t> bytes;
    };

    // Phase R1 skeleton: binding composes Program + Payload (+ Domain).
    struct ProgramInstanceBinding
    {
        ProgramBindingID id = 0;
        ShaderMaterialProgram *program = nullptr;
        MaterialInstancePayload *payload = nullptr;
        ResourceDomain *domain = nullptr;
        uint64_t layout_signature = 0;
    };
}
