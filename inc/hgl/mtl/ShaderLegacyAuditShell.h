#pragma once

#include <hgl/type/String.h>
#include <hgl/type/ManagedArray.h>

namespace hgl::graph::mtl
{
    enum class ShaderLegacyAuditState
    {
        NotStarted,
        InProgress,
        ReadyForCleanup,
        Blocked
    };

    struct ShaderLegacyAuditEntry
    {
        AnsiString symbol_name;
        AnsiString file_path;
        bool is_runtime_reference = false;
        bool is_test_reference = false;
        bool is_compatibility_only = false;
    };

    class ShaderLegacyAuditShell
    {
        ShaderLegacyAuditState state = ShaderLegacyAuditState::NotStarted;
        hgl::ManagedArray<ShaderLegacyAuditEntry> entries;
        bool runtime_reference_found = false;
        bool cleanup_blocked = false;

    public:
        ShaderLegacyAuditShell();

        void BeginAudit();
        void AddEntry(const ShaderLegacyAuditEntry &entry);
        void SetRuntimeReferenceFound(bool found);
        void SetCleanupBlocked(bool blocked);
        void CompleteAudit();

        ShaderLegacyAuditState GetState() const;
        const hgl::ManagedArray<ShaderLegacyAuditEntry> &GetEntries() const;
        bool IsCleanupSafe() const;

        AnsiString GetStatusText() const;
        AnsiString GetAuditChecklist() const;
    };
}
