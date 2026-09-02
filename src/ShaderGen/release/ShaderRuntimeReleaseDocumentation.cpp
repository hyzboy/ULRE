#include <hgl/mtl/ShaderRuntimeReleaseDocumentation.h>

namespace hgl::graph::mtl
{
    ShaderRuntimeReleaseDocumentation::ShaderRuntimeReleaseDocumentation()
        : status{}
    {
    }

    void ShaderRuntimeReleaseDocumentation::MarkReleaseShellReady()
    {
        status.development_phase = true;
        status.release_shell_ready = true;
    }

    void ShaderRuntimeReleaseDocumentation::MarkRuntimeSPVOnly()
    {
        status.development_phase = false;
        status.runtime_spv_only = true;
    }

    void ShaderRuntimeReleaseDocumentation::MarkCleanupAuditDone()
    {
        status.cleanup_audit_done = true;
    }

    void ShaderRuntimeReleaseDocumentation::MarkFinalGatePassed()
    {
        status.final_release_gate_passed = true;
    }

    const ShaderRuntimeReleaseStatus &ShaderRuntimeReleaseDocumentation::GetStatus() const
    {
        return status;
    }

    AnsiString ShaderRuntimeReleaseDocumentation::GetCurrentPhaseText() const
    {
        if(status.final_release_gate_passed)
            return "Final release gate passed: runtime is ready for production release decisions.";

        if(status.runtime_spv_only)
            return "Release phase reached: SPV-only runtime is expected, but the final gate still needs formal verification.";

        if(status.release_shell_ready)
            return "Development phase remains active: release shell is prepared, but runtime remains debug-friendly and not final.";

        return "Development phase active: shipping is intentionally deferred and the release shell is only a preparation layer.";
    }

    AnsiString ShaderRuntimeReleaseDocumentation::GetReleaseChecklist() const
    {
        return AnsiString("- Development phase: keep ShaderDocument and ShaderCodeModule usable for debugging\n")
             + "- Release shell: keep guard interfaces and preflight documentation in place\n"
             + "- Final packaging gate: only when runtime is validated as SPV-only and cleanup audit is complete\n"
             + "- Legacy cleanup: execute only after zero-reference audit and full regression pass";
    }
}
