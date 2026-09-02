#include <hgl/mtl/ShaderRuntimeReleaseShell.h>

#ifdef ULRE_ENABLE_RELEASE_SHELL
#include <hgl/log/Log.h>
#endif

namespace hgl::graph::mtl
{
    ShaderRuntimeReleaseShell::ShaderRuntimeReleaseShell()
        : state(ShaderReleaseShellState::Development)
        , packaging_mode(ShaderRuntimePackagingMode::Development)
        , developer_debug_compatible(true)
        , runtime_readonly(false)
    {
    }

    void ShaderRuntimeReleaseShell::SetPackagingMode(ShaderRuntimePackagingMode mode)
    {
        packaging_mode = mode;

        switch(mode)
        {
        case ShaderRuntimePackagingMode::Development:
            state = ShaderReleaseShellState::Development;
            runtime_readonly = false;
            developer_debug_compatible = true;
            break;

        case ShaderRuntimePackagingMode::ShellReady:
            state = ShaderReleaseShellState::ShellPrepared;
            runtime_readonly = false;
            developer_debug_compatible = true;
            break;

        case ShaderRuntimePackagingMode::SPVOnly:
            state = ShaderReleaseShellState::Finalized;
            runtime_readonly = true;
            developer_debug_compatible = false;
            break;
        }
    }

    ShaderRuntimePackagingMode ShaderRuntimeReleaseShell::GetPackagingMode() const
    {
        return packaging_mode;
    }

    void ShaderRuntimeReleaseShell::MarkShellReady()
    {
        state = ShaderReleaseShellState::ShellPrepared;
        packaging_mode = ShaderRuntimePackagingMode::ShellReady;
        developer_debug_compatible = true;
        runtime_readonly = false;

#ifdef ULRE_ENABLE_RELEASE_SHELL
        GLogNotice(u8"Release shell prepared: development-first packaging guard is active.");
#endif
    }

    bool ShaderRuntimeReleaseShell::IsShellReady() const
    {
        return state != ShaderReleaseShellState::Development;
    }

    bool ShaderRuntimeReleaseShell::IsDeveloperDebugCompatible() const
    {
        return developer_debug_compatible;
    }

    bool ShaderRuntimeReleaseShell::IsRuntimeReadOnly() const
    {
        return runtime_readonly;
    }

    AnsiString ShaderRuntimeReleaseShell::GetStatusText() const
    {
        switch(state)
        {
        case ShaderReleaseShellState::Development:
            return "Development-first release shell: runtime packaging remains flexible while shader generation continues.";

        case ShaderReleaseShellState::ShellPrepared:
            return "Release shell prepared: future SPV-only transition remains a planned post-development step.";

        case ShaderReleaseShellState::Finalized:
            return "Finalized release shell: runtime is expected to be SPV-only and read-only.";
        }

        return "Unknown release shell state.";
    }

    AnsiString ShaderRuntimeReleaseShell::GetPendingReleaseChecklist() const
    {
        return AnsiString("1. Keep ShaderDocument and ShaderCodeModule available for dev/debug\n")
             + "2. Preserve ShaderLegacyDocumentCompare and regression gates\n"
             + "3. Add release packaging guard before actual runtime split\n"
             + "4. Only execute Legacy cleanup after zero-reference audit and full rebuild\n"
             + "5. Switch to SPV-only runtime only near final release freeze";
    }
}
