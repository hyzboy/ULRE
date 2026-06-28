#pragma once

#include <hgl/ecs/core/System.h>
#include <string>
#include <vector>

namespace hgl::graph
{
    class ShaderMaterialProgram;
}

namespace hgl::ecs
{
    class RenderDiagnosticsService : public System
    {
    private:

        class ECSContext *world = nullptr;
        uint64_t last_emit_ms = 0;

    public:

        RenderDiagnosticsService(const std::string &name = "RenderDiagnosticsService");
        ~RenderDiagnosticsService() override = default;

    public:

        void SetWorld(ECSContext *w) { world = w; }

        bool GetDescriptorContractDiagnostics(uint32_t &materials_checked,
                                              uint32_t &materials_unresolved,
                                              uint32_t &required_missing,
                                              uint32_t &optional_missing,
                                              uint32_t &fallback_hits) const;

        bool GetDescriptorContractDiagnosticsExtended(uint32_t &materials_checked,
                                                      uint32_t &materials_unresolved,
                                                      uint32_t &required_missing,
                                                      uint32_t &optional_missing,
                                                      uint32_t &fallback_hits,
                                                      uint32_t &materials_registered,
                                                      uint32_t &binding_entries) const;

        bool GetMaterialBindingRegistryStats(uint32_t &materials_registered,
                                             uint32_t &binding_entries) const;

        bool GetMaterialBindingKeys(const graph::ShaderMaterialProgram *material,
                                    std::vector<std::string> &out_keys) const;

        void Update(float deltaTime) override;
    };
}
