#pragma once

#include <hgl/type/String.h>

namespace hgl::graph::mtl
{
    struct ShaderRuntimeReleaseStatus
    {
        bool development_phase = true;
        bool release_shell_ready = false;
        bool runtime_spv_only = false;
        bool cleanup_audit_done = false;
        bool final_release_gate_passed = false;
    };

    class ShaderRuntimeReleaseDocumentation
    {
        ShaderRuntimeReleaseStatus status;

    public:
        ShaderRuntimeReleaseDocumentation();

        void MarkReleaseShellReady();
        void MarkRuntimeSPVOnly();
        void MarkCleanupAuditDone();
        void MarkFinalGatePassed();

        const ShaderRuntimeReleaseStatus &GetStatus() const;
        AnsiString GetCurrentPhaseText() const;
        AnsiString GetReleaseChecklist() const;
    };
}
