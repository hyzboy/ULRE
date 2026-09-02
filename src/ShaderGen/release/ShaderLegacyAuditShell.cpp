#include <hgl/mtl/ShaderLegacyAuditShell.h>

namespace hgl::graph::mtl
{
    ShaderLegacyAuditShell::ShaderLegacyAuditShell()
        : state(ShaderLegacyAuditState::NotStarted)
        , runtime_reference_found(false)
        , cleanup_blocked(false)
    {
    }

    void ShaderLegacyAuditShell::BeginAudit()
    {
        state = ShaderLegacyAuditState::InProgress;
        runtime_reference_found = false;
        cleanup_blocked = false;
        entries.Clear();
    }

    void ShaderLegacyAuditShell::AddEntry(const ShaderLegacyAuditEntry &entry)
    {
        if (entry.symbol_name.IsEmpty())
            return;

        ShaderLegacyAuditEntry *new_entry = entries.Create();
        if (!new_entry)
            return;

        *new_entry = entry;
    }

    void ShaderLegacyAuditShell::SetRuntimeReferenceFound(bool found)
    {
        runtime_reference_found = found;
    }

    void ShaderLegacyAuditShell::SetCleanupBlocked(bool blocked)
    {
        cleanup_blocked = blocked;
    }

    void ShaderLegacyAuditShell::CompleteAudit()
    {
        if (cleanup_blocked || runtime_reference_found)
            state = ShaderLegacyAuditState::Blocked;
        else
            state = ShaderLegacyAuditState::ReadyForCleanup;
    }

    ShaderLegacyAuditState ShaderLegacyAuditShell::GetState() const
    {
        return state;
    }

    const hgl::ManagedArray<ShaderLegacyAuditEntry> &ShaderLegacyAuditShell::GetEntries() const
    {
        return entries;
    }

    bool ShaderLegacyAuditShell::IsCleanupSafe() const
    {
        return !runtime_reference_found && !cleanup_blocked && state == ShaderLegacyAuditState::ReadyForCleanup;
    }

    AnsiString ShaderLegacyAuditShell::GetStatusText() const
    {
        switch(state)
        {
        case ShaderLegacyAuditState::NotStarted:
            return "Legacy audit not started; no cleanup decision should be made yet.";
        case ShaderLegacyAuditState::InProgress:
            return "Legacy audit in progress: scan runtime references, tests, and compatibility-only use sites.";
        case ShaderLegacyAuditState::ReadyForCleanup:
            return "Legacy audit indicates all references are accounted for and cleanup may be considered after full regression.";
        case ShaderLegacyAuditState::Blocked:
            return "Legacy cleanup is blocked by runtime references or audit findings that require a safe rollback.";
        }

        return "Legacy audit state unknown.";
    }

    AnsiString ShaderLegacyAuditShell::GetAuditChecklist() const
    {
        return AnsiString("1. Audit runtime references to ShaderDocumentLegacyAdapter and legacy shader entry points\n")
             + "2. Separate compatibility-only uses from real runtime contracts\n"
             + "3. Require zero runtime references before deleting any safety wrapper\n"
             + "4. Run full Debug/Release regression after every cleanup step\n"
             + "5. Keep this shell as pre-delete guard until final release freeze";
    }
}
