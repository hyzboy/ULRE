#pragma once

#include <hgl/type/String.h>

namespace hgl::graph::mtl
{
    enum class ShaderRuntimePackagingMode
    {
        Development,
        ShellReady,
        SPVOnly
    };

    enum class ShaderReleaseShellState
    {
        Development,
        ShellPrepared,
        Finalized
    };

    class ShaderRuntimeReleaseShell
    {
        ShaderReleaseShellState state = ShaderReleaseShellState::Development;
        ShaderRuntimePackagingMode packaging_mode = ShaderRuntimePackagingMode::Development;
        bool developer_debug_compatible = true;
        bool runtime_readonly = false;

    public:
        ShaderRuntimeReleaseShell();

        void SetPackagingMode(ShaderRuntimePackagingMode mode);
        ShaderRuntimePackagingMode GetPackagingMode() const;

        void MarkShellReady();
        bool IsShellReady() const;

        bool IsDeveloperDebugCompatible() const;
        bool IsRuntimeReadOnly() const;

        AnsiString GetStatusText() const;
        AnsiString GetPendingReleaseChecklist() const;
    };
}
