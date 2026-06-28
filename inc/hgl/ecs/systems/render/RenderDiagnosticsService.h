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
    public:

        struct DiagnosticsSnapshot
        {
            bool valid = false;
            uint32_t materials_checked = 0;
            uint32_t materials_unresolved = 0;
            uint32_t required_missing = 0;
            uint32_t optional_missing = 0;
            uint32_t fallback_hits = 0;
            uint32_t materials_registered = 0;
            uint32_t binding_entries = 0;

            uint32_t strict_total = 0;
            uint32_t strict_prebuild = 0;
            uint32_t strict_spv = 0;
            uint32_t strict_vertex = 0;
            uint32_t strict_descriptor = 0;
            uint32_t strict_materials = 0;
        };

    private:

        class ECSContext *world = nullptr;
        uint64_t last_emit_ms = 0;
        mutable DiagnosticsSnapshot snapshot;
        mutable uint32_t snapshot_frame_index = ~0u;

    private:

        bool RefreshSnapshot() const;

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

        bool GetDiagnosticsSnapshot(DiagnosticsSnapshot &out_snapshot) const;

        void Update(float deltaTime) override;
    };
}
