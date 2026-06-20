#pragma once

#include <hgl/mtl/StaticMaterialDef.h>
#include <hgl/mtl/MaterialResourceManifest.h>
#include <hgl/mtl/MaterialPrunePolicy.h>
#include <string>
#include <vector>

namespace hgl::graph::mtl
{
    struct SFMAnnotationRecord
    {
        std::string key;
        std::vector<std::string> args;
        uint32_t line = 0;
    };

    struct SFMAnnotationIssue
    {
        uint32_t error_code = 0;
        uint32_t line = 0;
        std::string key;
        std::string detail;
    };

    struct SFMAnnotationScanReport
    {
        std::vector<SFMAnnotationRecord> records;
        std::vector<SFMAnnotationIssue> issues;

        bool HasErrors() const noexcept { return !issues.empty(); }
    };

    /// Parse `@sfm:` annotation lines from GLSL source. The parser is comment-based
    /// and accepts both `// @sfm:key ...` and lines starting with `@sfm:key ...`.
    ///
    /// The parser performs Phase2 checks:
    /// - key whitelist validation
    /// - duplicate line detection
    /// - basic contradictory directive detection
    /// - derive subset validation against require/optional tokens
    ///
    /// Returns true when the source is syntactically valid. Any issues are
    /// appended to out_report. If diagnostics is non-null, a human-readable
    /// summary is also appended.
    bool ParseSFMAnnotationsFromGLSL(const std::string &source,
                                     SFMAnnotationScanReport &out_report,
                                     std::string *diagnostics = nullptr) noexcept;

    bool CollectShaderAutoRequirements(const StaticMaterialDef &base_def,
                                       const std::string &shader_library_path,
                                       const std::string &vertex_glsl,
                                       const std::string &fragment_glsl,
                                       MaterialResourceManifest &out_requirements,
                                       std::string *diagnostics = nullptr);

    /// Builds a merged manifest from base_def and auto-scanned requirements.
    /// Equivalent to FromStaticDef(base_def).MergeKeepFirst(auto_requirements).
    MaterialResourceManifest MergeManifestWithAutoRequirements(
        const StaticMaterialDef &base_def,
        const MaterialResourceManifest &auto_requirements);

    // ─────────────────────────────────────────────────────────────────────────
    // Phase 5: manifest ↔ effective-policy operations
    // ─────────────────────────────────────────────────────────────────────────

    /// Prune a manifest so that any resource disallowed by the effective policy is removed.
    /// Returns the pruned manifest (the original is not modified).
    /// This is the primary Phase 5 operation; call it after CollectShaderAutoRequirements
    /// and before passing the manifest to the descriptor builder.
    MaterialResourceManifest PruneManifestByPolicy(
        const MaterialResourceManifest     &manifest,
        const MaterialResourceRequirements &effective_policy);

    /// Validate that a manifest does not contain resources the effective policy forbids.
    /// - PolicyForbids violations are appended to *diagnostics (if non-null) and cause
    ///   the function to return false.
    /// - PolicyRequires cases (reflection pruned an allowed resource) are logged as
    ///   informational messages but do NOT cause a failure.
    /// Returns true if no PolicyForbids violations were found.
    bool ValidateManifestAgainstPolicy(
        const MaterialResourceManifest     &manifest,
        const MaterialResourceRequirements &effective_policy,
        const char                         *material_name = nullptr,
        std::string                        *diagnostics   = nullptr);
}
