#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <hgl/type/Smart.h>

#include <hgl/graph/geo/GeometryVertexFormat.h>
#include <hgl/mtl/MaterialDefinitionRegistry.h>
#include <hgl/mtl/MaterialShaderCompiler.h>
#include <hgl/mtl/SamplerPreset.h>
#include <hgl/mtl/ShaderBuildContext.h>
#include <hgl/mtl/ShaderCreateInfo.h>
#include <hgl/mtl/ShaderKeyUtility.h>
#include <hgl/mtl/ShaderLibraryPath.h>
#include <hgl/filesystem/Path.h>

#include <cstring>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::graph::mtl;

namespace
{
    struct Fixture
    {
        const char *name;
        const char *material_id;
        ShaderProgramPurpose purpose;
        bool requires_geometry;
    };

    const char *GetBlockKindName(const ShaderDocumentBlockKind kind)
    {
        switch (kind)
        {
        case ShaderDocumentBlockKind::Version:  return "version";
        case ShaderDocumentBlockKind::Extension:return "extension";
        case ShaderDocumentBlockKind::Define:   return "define";
        case ShaderDocumentBlockKind::Resource: return "resource";
        case ShaderDocumentBlockKind::Interface:return "interface";
        case ShaderDocumentBlockKind::Module:   return "module";
        case ShaderDocumentBlockKind::Function: return "function";
        case ShaderDocumentBlockKind::MainBody: return "main";
        case ShaderDocumentBlockKind::Raw:      return "raw";
        }
        return "unknown";
    }

    int FindFirstDifference(const AnsiString &lhs, const char *rhs, const int rhs_length)
    {
        const int common_length = lhs.Length() < rhs_length
            ? lhs.Length()
            : rhs_length;
        for (int i = 0; i < common_length; ++i)
            if (lhs[i] != rhs[i])
                return i;
        return lhs.Length() == rhs_length ? -1 : common_length;
    }

    int GetLineNumber(const char *text, const int length, const int offset)
    {
        int line = 1;
        for (int i = 0; i < offset && i < length; ++i)
            if (text[i] == '\n')
                ++line;
        return line;
    }

    AnsiString GetContext(const char *text, const int length, const int offset)
    {
        const int begin = offset > 32 ? offset - 32 : 0;
        const int end = offset + 32 < length ? offset + 32 : length;
        AnsiString result;
        for (int i = begin; i < end; ++i)
        {
            if (text[i] == '\n')
                result += "\\n";
            else if (text[i] == '\r')
                result += "\\r";
            else
                result += AnsiString::charOf(text[i]);
        }
        return result;
    }

    int CountText(const AnsiString &text, const char *needle)
    {
        int count = 0;
        const int needle_length = int(std::strlen(needle));
        for (int offset = 0; offset + needle_length <= text.Length(); ++offset)
        {
            if (std::memcmp(text.c_str() + offset, needle, needle_length) == 0)
                ++count;
        }
        return count;
    }

    const ShaderDocumentBlock *FindBlockAtOffset(
        const ShaderDocument &document, const int offset, int &out_index)
    {
        int begin = 0;
        for (int i = 0; i < document.GetBlockCount(); ++i)
        {
            const ShaderDocumentBlock &block = document.GetBlock(i);
            const int end = begin + block.text.Length();
            if (offset < end || (offset == end && i + 1 == document.GetBlockCount()))
            {
                out_index = i;
                return &block;
            }
            begin = end;
        }
        out_index = -1;
        return nullptr;
    }

    bool ReportSerializationMismatch(
        const char *fixture,
        const char *stage,
        const ShaderDocument &document,
        const AnsiString &serialized,
        const char *actual,
        const int actual_length)
    {
        const int offset = FindFirstDifference(serialized, actual, actual_length);
        if (offset < 0)
            return true;

        int block_index = -1;
        const ShaderDocumentBlock *block =
            FindBlockAtOffset(document, offset, block_index);
        const ShaderDocumentSource empty_source{};
        const ShaderDocumentSource &source = block
            ? block->source
            : empty_source;
        const int document_line = GetLineNumber(
            serialized.c_str(), serialized.Length(), offset);
        const int actual_line = GetLineNumber(actual, actual_length, offset);
        const AnsiString document_context = GetContext(
            serialized.c_str(), serialized.Length(), offset);
        const AnsiString actual_context = GetContext(
            actual, actual_length, offset);
        GLogError(
            "[ShaderLegacyDocumentCompare] fixture=%s stage=%s GLSL mismatch "
            "offset=%d document(line=%d,length=%d,context=\"%s\") "
            "actual(line=%d,length=%d,context=\"%s\") "
            "block=%d kind=%s source={material=%s,stage=%s,module=%s,path=%s,name=%s}",
            fixture, stage, offset,
            document_line, serialized.Length(), document_context.c_str(),
            actual_line, actual_length, actual_context.c_str(),
            block_index, block ? GetBlockKindName(block->kind) : "end",
            source.material.c_str(), source.stage.c_str(), source.module.c_str(),
            source.path.c_str(), source.logical_name.c_str());
        return false;
    }

    bool ValidateSourceDocument(
        const char *fixture,
        const char *stage,
        const ShaderDocument &document)
    {
        int version_count = 0;
        int main_count = 0;
        bool valid = document.GetBlockCount() > 0;

        for (int i = 0; i < document.GetBlockCount(); ++i)
        {
            const ShaderDocumentBlock &block = document.GetBlock(i);
            if (block.kind == ShaderDocumentBlockKind::Version)
                ++version_count;
            if (block.kind == ShaderDocumentBlockKind::MainBody)
                ++main_count;
            if (block.source.stage != stage
             || block.source.logical_name.IsEmpty())
            {
                GLogError(
                    "[ShaderLegacyDocumentCompare] fixture=%s stage=%s source "
                    "document block=%d has invalid metadata "
                    "{stage=%s,module=%s,path=%s,name=%s}",
                    fixture, stage, i, block.source.stage.c_str(),
                    block.source.module.c_str(), block.source.path.c_str(),
                    block.source.logical_name.c_str());
                valid = false;
            }
        }

        if (version_count != 1 || main_count != 1
         || document.GetBlock(0).kind != ShaderDocumentBlockKind::Version
         || document.GetBlock(document.GetBlockCount() - 1).kind
                != ShaderDocumentBlockKind::MainBody)
        {
            GLogError(
                "[ShaderLegacyDocumentCompare] fixture=%s stage=%s source "
                "document order invalid: blocks=%d versions=%d mains=%d "
                "first=%s last=%s",
                fixture, stage, document.GetBlockCount(), version_count, main_count,
                document.GetBlockCount() > 0
                    ? GetBlockKindName(document.GetBlock(0).kind) : "none",
                document.GetBlockCount() > 0
                    ? GetBlockKindName(
                        document.GetBlock(document.GetBlockCount() - 1).kind)
                    : "none");
            valid = false;
        }
        return valid;
    }

    bool ValidateFinalDocument(
        const char *fixture,
        const char *stage,
        const ShaderDocument &document,
        AnsiString &out_serialized)
    {
        ShaderDocumentDiagnostics diagnostics;
        if (!document.Serialize(out_serialized, diagnostics))
        {
            GLogError(
                "[ShaderLegacyDocumentCompare] fixture=%s stage=%s final "
                "document serialization failed (diagnostics=%d)",
                fixture, stage, diagnostics.GetCount());
            return false;
        }

        const int version_count = CountText(out_serialized, "#version");
        const int main_count = CountText(out_serialized, "void main(");
        if (version_count != 1 || main_count != 1
         || out_serialized.Left(8).Comp("#version") != 0)
        {
            GLogError(
                "[ShaderLegacyDocumentCompare] fixture=%s stage=%s final "
                "document invariant failed: versions=%d mains=%d first=\"%s\"",
                fixture, stage, version_count, main_count,
                GetContext(out_serialized.c_str(), out_serialized.Length(), 0).c_str());
            return false;
        }
        return true;
    }

    bool VerifyStage(
        const char *fixture,
        const char *stage_name,
        const ShaderDocument &source_document,
        const ShaderDocument &final_document,
        const ShaderCreateInfo *actual_stage,
        const ShaderStageKey &stage_key)
    {
        if (!actual_stage)
        {
            GLogError(
                "[ShaderLegacyDocumentCompare] fixture=%s stage=%s is absent "
                "from its production ShaderBuildContext",
                fixture, stage_name);
            return false;
        }

        bool valid = ValidateSourceDocument(fixture, stage_name, source_document);
        ShaderDocumentDiagnostics diagnostics;
        AnsiString source_serialized;
        if (!source_document.Serialize(source_serialized, diagnostics))
        {
            GLogError(
                "[ShaderLegacyDocumentCompare] fixture=%s stage=%s source "
                "document serialization failed (diagnostics=%d)",
                fixture, stage_name, diagnostics.GetCount());
            return false;
        }
        const uint64 source_document_hash =
            source_document.GetSerializedHash(diagnostics);
        const uint64 source_hash = HashFinalShaderSource(
            source_serialized.c_str(), source_serialized.Length());
        const auto &actual = actual_stage->GetFinalGLSL();
        if (source_document_hash == 0 || source_hash == 0
         || stage_key.definition_hash != source_hash)
        {
            GLogError(
                "[ShaderLegacyDocumentCompare] fixture=%s stage=%s source hash "
                "mismatch: serialized=%llu document=%llu stage-key=%llu",
                fixture, stage_name,
                static_cast<unsigned long long>(source_hash),
                static_cast<unsigned long long>(source_document_hash),
                static_cast<unsigned long long>(stage_key.definition_hash));
            valid = false;
        }

        AnsiString serialized;
        if (!ValidateFinalDocument(
                fixture, stage_name, final_document, serialized))
            return false;

        const uint64 final_document_hash =
            final_document.GetSerializedHash(diagnostics);
        const uint64 final_hash = HashFinalShaderSource(
            serialized.c_str(), serialized.Length());
        const uint64 actual_hash = HashFinalShaderSource(
            actual.data(), actual.size());
        if (final_document_hash == 0 || final_hash == 0
         || final_hash != actual_hash)
        {
            GLogError(
                "[ShaderLegacyDocumentCompare] fixture=%s stage=%s final hash "
                "mismatch: serialized=%llu document=%llu actual=%llu",
                fixture, stage_name,
                static_cast<unsigned long long>(final_hash),
                static_cast<unsigned long long>(final_document_hash),
                static_cast<unsigned long long>(actual_hash));
            valid = false;
        }

        return ReportSerializationMismatch(
                   fixture, stage_name, final_document, serialized,
                   actual.data(), int(actual.size()))
            && valid;
    }

    bool RunFixture(const Fixture &fixture)
    {
        MaterialDefinition definition{};
        if (!TryGetMaterialDefinitionByID(fixture.material_id, definition))
        {
            GLogError(
                "[ShaderLegacyDocumentCompare] fixture=%s cannot resolve material=%s",
                fixture.name, fixture.material_id);
            return false;
        }

        GeometryVertexFormat geometry;
        if (fixture.requires_geometry)
        {
            if (!geometry.Add(VertexSemantic::Position, VF_V3F, 0, 0))
                return false;
            if (std::strcmp(fixture.material_id, "Lit") == 0
             && (!geometry.Add(VertexSemantic::TexCoord, VF_V2F, 0, 0)
              || !geometry.Add(VertexSemantic::Normal, VF_V3F, 0, 0)))
                return false;
        }

        MaterialShaderDocumentCapture capture{};
        MaterialDefinitionBuildRequest request{};
        request.recipe.mtl_def_id = definition.definition_id;
        request.geometry_vertex_format = fixture.requires_geometry ? &geometry : nullptr;
        request.defer_finalize = true;
        request.override_shader_program_purpose =
            fixture.purpose != ShaderProgramPurpose::ForwardColor;
        request.shader_program_purpose = fixture.purpose;

        AutoDelete<ShaderBuildContext> context(
            CreateMaterialFromDefinition(nullptr, definition, request, &capture));
        if (!context || !context->HasProgramLink()
         || !context->HasProgramArtifactMetadata())
        {
            GLogError(
                "[ShaderLegacyDocumentCompare] fixture=%s production material "
                "build failed or did not expose program identity",
                fixture.name);
            return false;
        }

        const ShaderLinkSpec &link = context->GetProgramLink();
        bool valid = VerifyStage(
            fixture.name, "mesh", capture.mesh_source_document,
            capture.mesh_final_document,
            context->GetStageShader(ShaderStage::Mesh), link.mesh_stage);
        valid = VerifyStage(
            fixture.name, "fragment", capture.fragment_source_document,
            capture.fragment_final_document,
            context->GetStageShader(ShaderStage::Fragment), link.fragment_stage)
            && valid;

        const ShaderProgramArtifactMetadata &metadata =
            context->GetProgramArtifactMetadata();
        const auto *mesh = context->GetStageShader(ShaderStage::Mesh);
        const auto *fragment = context->GetStageShader(ShaderStage::Fragment);
        hgl::hash::FNV1aHasher64 source_hasher;
        source_hasher << HashFinalShaderSource(
            mesh->GetFinalGLSL().data(), mesh->GetFinalGLSL().size())
                      << HashFinalShaderSource(
            fragment->GetFinalGLSL().data(), fragment->GetFinalGLSL().size());
        if (metadata.program_key_digest != link.BuildKey().GetDigest()
         || metadata.mesh_stage_digest != link.mesh_stage.GetDigest()
         || metadata.fragment_stage_digest != link.fragment_stage.GetDigest()
         || metadata.generated_source_digest != uint64(source_hasher))
        {
            GLogError(
                "[ShaderLegacyDocumentCompare] fixture=%s program/stage key "
                "mismatch: program=%llu/%llu mesh=%llu/%llu fragment=%llu/%llu "
                "source=%llu/%llu",
                fixture.name,
                static_cast<unsigned long long>(metadata.program_key_digest),
                static_cast<unsigned long long>(link.BuildKey().GetDigest()),
                static_cast<unsigned long long>(metadata.mesh_stage_digest),
                static_cast<unsigned long long>(link.mesh_stage.GetDigest()),
                static_cast<unsigned long long>(metadata.fragment_stage_digest),
                static_cast<unsigned long long>(link.fragment_stage.GetDigest()),
                static_cast<unsigned long long>(metadata.generated_source_digest),
                static_cast<unsigned long long>(uint64(source_hasher)));
            valid = false;
        }
        return valid;
    }

    bool ParseFull(const int argc, char **argv, bool &out_full)
    {
        out_full = false;
        if (argc == 1)
            return true;
        if (argc != 2)
            return false;
        if (std::strcmp(argv[1], "--smoke") == 0)
            return true;
        if (std::strcmp(argv[1], "--full") == 0)
        {
            out_full = true;
            return true;
        }
        return false;
    }
}

int main(const int argc, char **argv)
{
    if (!hgl::logger::InitLogger(OS_TEXT("ShaderLegacyDocumentCompare")))
        return 3;

    bool full = false;
    if (!ParseFull(argc, argv, full))
    {
        GLogError(
            "[ShaderLegacyDocumentCompare] usage: "
            "ShaderLegacyDocumentCompare [--smoke|--full]");
        return 2;
    }

    const hgl::filesystem::Path sampler_toml =
        hgl::filesystem::Path(ToOSString(GetShaderLibraryPath()))
        / OSString(OS_TEXT("sampler.toml"));
    if (!SamplerPresetLibrary::Instance().Load(sampler_toml.ToOSString()))
    {
        GLogError(
            "[ShaderLegacyDocumentCompare] cannot load sampler presets from %s",
            ToU8String(sampler_toml.ToOSString()).c_str());
        return 4;
    }

    static const Fixture smoke_fixtures[] =
    {
        { "pure-color-forward", BUILTIN_MTL_DEF_PURE_COLOR,
          ShaderProgramPurpose::ForwardColor, true }
    };
    static const Fixture full_fixtures[] =
    {
        { "pure-color-forward", BUILTIN_MTL_DEF_PURE_COLOR,
          ShaderProgramPurpose::ForwardColor, true },
        { "pure-color-depth", BUILTIN_MTL_DEF_PURE_COLOR,
          ShaderProgramPurpose::DepthOnly, true },
        { "pure-color-shadow", BUILTIN_MTL_DEF_PURE_COLOR,
          ShaderProgramPurpose::ShadowDepth, true },
        { "lit-forward", "Lit", ShaderProgramPurpose::ForwardColor, true },
        { "text-gpu-charquad", BUILTIN_MTL_DEF_TEXT,
          ShaderProgramPurpose::ForwardColor, false }
    };

    const Fixture *fixtures = full ? full_fixtures : smoke_fixtures;
    const int fixture_count = full
        ? int(sizeof(full_fixtures) / sizeof(full_fixtures[0]))
        : int(sizeof(smoke_fixtures) / sizeof(smoke_fixtures[0]));
    for (int i = 0; i < fixture_count; ++i)
    {
        if (!RunFixture(fixtures[i]))
            return 1;
    }

    GLogInfo(
        "[ShaderLegacyDocumentCompare] %s production fixtures passed",
        full ? "full" : "smoke");
    return 0;
}
