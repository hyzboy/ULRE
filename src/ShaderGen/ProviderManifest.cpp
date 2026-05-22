/// ProviderManifest.cpp
///
/// @sfm header parser and ProviderManifestRegistry implementation.
///
/// @sfm header format (lines inside a GLSL block comment or as // comments):
///
///   @sfm version: 1
///   @sfm kind: vab | pcg
///   @sfm output_space: local | world | clip_ndc
///   @sfm consumes_vab: true | false          (optional; defaults by kind)
///   @sfm needs_ssbo: true | false            (optional; default false)
///   @sfm needs_ubo: true | false             (optional; default false)
///   @sfm needs_sampler: true | false         (optional; default false)
///   @sfm allow_dim_override: true | false    (optional; default: true for vab, false for pcg)
///   @sfm input: vab <attrib_name> <format>   (one per VAB attribute)
///   @sfm input: ssbo <name>                  (resource inputs)
///   @sfm input: ubo  <name>
///
/// The @sfm block must appear before the first #ifndef guard line.
/// Parsing stops at the first line that does NOT start with // or is empty.

#include <hgl/shadergen/ProviderManifest.h>
#include <hgl/shadergen/PositionProviderRegistry.h>
#include <hgl/shadergen/ShaderLibraryPath.h>
#include <hgl/common/PositionProvider.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cassert>
#include <cstdio>

namespace fs = std::filesystem;

namespace hgl::graph
{

// ── Local helpers ─────────────────────────────────────────────────────────────

static std::string_view Trim(std::string_view s) noexcept
{
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
        s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n'))
        s.remove_suffix(1);
    return s;
}

static bool BoolValue(std::string_view v) noexcept
{
    return v == "true" || v == "1" || v == "yes";
}

// ── @sfm parser ───────────────────────────────────────────────────────────────

/// Parse @sfm metadata from the header of a GLSL file.
/// Returns false if mandatory fields are missing or version != 1.
static bool ParseSfmHeader(const fs::path &file_path,
                            std::string_view rel_glsl_path,
                            ProviderManifest &out)
{
    std::ifstream f(file_path);
    if (!f.is_open())
    {
        std::fprintf(stderr, "[ProviderManifest] Cannot open: %s\n",
                     file_path.string().c_str());
        return false;
    }

    out = {};
    out.glsl_path      = std::string(rel_glsl_path);
    out.glsl_path_hash = Fnv1a32(rel_glsl_path);

    bool found_version      = false;
    bool found_kind         = false;
    bool found_output_space = false;
    bool kind_override_dim_set = false;

    std::string line_buf;
    while (std::getline(f, line_buf))
    {
        std::string_view raw = line_buf;
        std::string_view line = Trim(raw);

        // Skip empty lines and block-comment delimiters at the start
        if (line.empty() || line == "/*" || line == "*/")
            continue;

        // Strip leading // or * (inside block comment)
        if (line.substr(0, 2) == "//")
            line = Trim(line.substr(2));
        else if (line.front() == '*')
            line = Trim(line.substr(1));
        else
            break; // Non-comment content reached; stop scanning

        if (line.substr(0, 4) != "@sfm")
            continue;

        // @sfm <key>: <value>
        auto colon = line.find(':');
        if (colon == std::string_view::npos)
            continue;

        std::string_view key   = Trim(line.substr(4, colon - 4));
        std::string_view value = Trim(line.substr(colon + 1));

        if (key == "version")
        {
            out.sfm_version = static_cast<uint16_t>(std::stoi(std::string(value)));
            found_version   = true;
        }
        else if (key == "kind")
        {
            if (value == "vab") { out.kind = ProviderKind::VAB; found_kind = true; }
            else if (value == "pcg") { out.kind = ProviderKind::PCG; found_kind = true; }
            else
            {
                std::fprintf(stderr, "[ProviderManifest] Unknown kind '%.*s' in %s\n",
                             (int)value.size(), value.data(), file_path.string().c_str());
                return false;
            }
        }
        else if (key == "output_space")
        {
            if      (value == "local")    { out.output_space = OutputSpace::Local;   found_output_space = true; }
            else if (value == "world")    { out.output_space = OutputSpace::World;   found_output_space = true; }
            else if (value == "clip_ndc") { out.output_space = OutputSpace::ClipNDC; found_output_space = true; }
            else
            {
                std::fprintf(stderr, "[ProviderManifest] Unknown output_space '%.*s' in %s\n",
                             (int)value.size(), value.data(), file_path.string().c_str());
                return false;
            }
        }
        else if (key == "consumes_vab")       out.consumes_vab       = BoolValue(value);
        else if (key == "needs_ssbo")         out.needs_ssbo         = BoolValue(value);
        else if (key == "needs_ubo")          out.needs_ubo          = BoolValue(value);
        else if (key == "needs_sampler")      out.needs_sampler      = BoolValue(value);
        else if (key == "allow_dim_override") { out.allow_dim_override = BoolValue(value); kind_override_dim_set = true; }
        else if (key == "input")
        {
            ProviderManifest::InputSpec spec;
            // Format: <source> <attrib_name> [format]
            auto sp1 = value.find(' ');
            std::string_view src_tok = (sp1 == std::string_view::npos) ? value : value.substr(0, sp1);
            std::string_view rest    = (sp1 == std::string_view::npos) ? "" : Trim(value.substr(sp1 + 1));

            if      (src_tok == "vab")  spec.source = InputSource::VAB;
            else if (src_tok == "ssbo") spec.source = InputSource::SSBO;
            else if (src_tok == "ubo")  spec.source = InputSource::UBO;
            else if (src_tok == "push") spec.source = InputSource::Push;
            else                        spec.source = InputSource::None;

            auto sp2 = rest.find(' ');
            spec.attrib_name = std::string(sp2 == std::string_view::npos ? rest : rest.substr(0, sp2));
            if (sp2 != std::string_view::npos)
                spec.format = std::string(Trim(rest.substr(sp2 + 1)));

            out.inputs.push_back(std::move(spec));
        }
        // Unknown keys are silently ignored for forward-compatibility
    }

    if (!found_version)
    {
        std::fprintf(stderr, "[ProviderManifest] Missing @sfm version in %s\n",
                     file_path.string().c_str());
        return false;
    }
    if (out.sfm_version != 1)
    {
        std::fprintf(stderr, "[ProviderManifest] Unsupported @sfm version %u in %s\n",
                     out.sfm_version, file_path.string().c_str());
        return false;
    }
    if (!found_kind)
    {
        std::fprintf(stderr, "[ProviderManifest] Missing @sfm kind in %s\n",
                     file_path.string().c_str());
        return false;
    }
    if (!found_output_space)
    {
        std::fprintf(stderr, "[ProviderManifest] Missing @sfm output_space in %s\n",
                     file_path.string().c_str());
        return false;
    }

    // Apply kind defaults if not explicitly set
    if (!kind_override_dim_set)
        out.allow_dim_override = (out.kind == ProviderKind::VAB);

    if (out.kind == ProviderKind::VAB && !out.consumes_vab)
        out.consumes_vab = true; // VAB providers implicitly consume a VAB

    return true;
}

// ── Registry storage ──────────────────────────────────────────────────────────

namespace
{
    // All parsed manifests (builtin + UserPCG)
    std::vector<ProviderManifest> g_manifests;

    // ID → index (for builtin lookups, populated during Initialize)
    std::unordered_map<uint32_t, size_t> g_by_pos_id;

    // glsl_path → index
    std::unordered_map<std::string, size_t> g_by_path;

    // hash → index
    std::unordered_map<uint32_t, size_t> g_by_hash;

    std::string g_lib_root; // stored for AcquireUserProvider
}

static void RegisterManifest(ProviderManifest m)
{
    // Deduplicate by glsl_path
    auto it = g_by_path.find(m.glsl_path);
    if (it != g_by_path.end())
        return; // already present

    size_t idx = g_manifests.size();
    g_by_path[m.glsl_path]     = idx;
    g_by_hash[m.glsl_path_hash] = idx;
    if (m.pos_id != PositionProviderId::Unknown &&
        m.pos_id != PositionProviderId::Invalid &&
        m.pos_id != PositionProviderId::UserPCG)
    {
        g_by_pos_id[static_cast<uint32_t>(m.pos_id)] = idx;
    }
    g_manifests.push_back(std::move(m));
}

// ── ProviderManifestRegistry ──────────────────────────────────────────────────

void ProviderManifestRegistry::Initialize(std::string_view lib_root)
{
    g_manifests.clear();
    g_by_pos_id.clear();
    g_by_path.clear();
    g_by_hash.clear();
    g_lib_root = std::string(lib_root);

    const fs::path dir = fs::path(g_lib_root) / "position_provider";
    if (!fs::exists(dir))
    {
        std::fprintf(stderr, "[ProviderManifest] Directory not found: %s\n",
                     dir.string().c_str());
        return;
    }

    // Import static ID↔path mapping from the builtin registry
    // We use FindBuiltinProvider to get the glsl_path for each known ID so
    // that the manifest registry can attach the correct PositionProviderId
    // to each parsed manifest.

    size_t id_count = 0;
    const PositionProviderId* ids = GetAllBuiltinProviderIds(&id_count);

    // Build a path→id table so we can attach IDs after parsing.
    // PositionProviderRegistry stores paths as "ShaderLibrary/position_provider/xxx.glsl"
    // but the manifest scanner builds relative paths as "position_provider/xxx.glsl"
    // (relative to lib_root, which is already the ShaderLibrary directory).
    // Strip the "ShaderLibrary/" prefix so both sides use the same key format.
    static constexpr std::string_view kShaderLibPrefix = "ShaderLibrary/";
    std::unordered_map<std::string, PositionProviderId> path_to_id;
    for (size_t i = 0; i < id_count; ++i)
    {
        const PositionProvider* p = FindBuiltinProvider(ids[i]);
        if (!p || p->glsl_path.empty()) continue;
        std::string_view sv = p->glsl_path;
        if (sv.substr(0, kShaderLibPrefix.size()) == kShaderLibPrefix)
            sv = sv.substr(kShaderLibPrefix.size());
        path_to_id[std::string(sv)] = ids[i];
    }

    // Scan directory
    for (const auto& entry : fs::directory_iterator(dir))
    {
        if (!entry.is_regular_file()) continue;
        const auto& path = entry.path();
        if (path.extension() != ".glsl") continue;

        // Build ShaderLibrary-relative path (forward slashes)
        std::string rel = "position_provider/" + path.filename().string();

        ProviderManifest m;
        if (!ParseSfmHeader(path, rel, m))
            continue;

        // Attach PositionProviderId if this is a known builtin
        auto id_it = path_to_id.find(rel);
        if (id_it != path_to_id.end())
            m.pos_id = id_it->second;
        else
            m.pos_id = PositionProviderId::Unknown; // UserPCG or unknown builtin

        RegisterManifest(std::move(m));
    }
}

void ProviderManifestRegistry::Shutdown()
{
    g_manifests.clear();
    g_by_pos_id.clear();
    g_by_path.clear();
    g_by_hash.clear();
    g_lib_root.clear();
}

const ProviderManifest* ProviderManifestRegistry::FindByPosId(PositionProviderId id) noexcept
{
    if (id == PositionProviderId::Unknown ||
        id == PositionProviderId::Invalid ||
        id == PositionProviderId::UserPCG)
        return nullptr;

    auto it = g_by_pos_id.find(static_cast<uint32_t>(id));
    if (it == g_by_pos_id.end()) return nullptr;
    return &g_manifests[it->second];
}

const ProviderManifest* ProviderManifestRegistry::FindByGlslPath(std::string_view path) noexcept
{
    auto it = g_by_path.find(std::string(path));
    if (it == g_by_path.end()) return nullptr;
    return &g_manifests[it->second];
}

const ProviderManifest* ProviderManifestRegistry::FindByPathHash(uint32_t hash) noexcept
{
    auto it = g_by_hash.find(hash);
    if (it == g_by_hash.end()) return nullptr;
    return &g_manifests[it->second];
}

const ProviderManifest* ProviderManifestRegistry::AcquireUserProvider(std::string_view glsl_path)
{
    // Check if already registered
    const ProviderManifest* existing = FindByGlslPath(glsl_path);
    if (existing) return existing;

    // Parse on demand
    std::string rel = std::string(glsl_path);
    fs::path full   = fs::path(g_lib_root) / rel;
    ProviderManifest m;
    if (!ParseSfmHeader(full, rel, m))
    {
        std::fprintf(stderr, "[ProviderManifest] AcquireUserProvider failed for: %.*s\n",
                     (int)glsl_path.size(), glsl_path.data());
        return nullptr;
    }
    m.pos_id = PositionProviderId::UserPCG;
    RegisterManifest(std::move(m));
    return FindByGlslPath(glsl_path);
}

bool ProviderManifestRegistry::RunSelfCheck()
{
    bool ok = true;

    // Check for hash collisions
    if (g_by_hash.size() != g_manifests.size())
    {
        std::fprintf(stderr, "[ProviderManifest::SelfCheck] Hash collision detected! "
                     "manifests=%zu, hash_entries=%zu\n",
                     g_manifests.size(), g_by_hash.size());
        ok = false;
    }

    for (const ProviderManifest& m : g_manifests)
    {
        // Version check
        if (m.sfm_version != 1)
        {
            std::fprintf(stderr, "[ProviderManifest::SelfCheck] Bad version %u: %s\n",
                         m.sfm_version, m.glsl_path.c_str());
            ok = false;
        }

        // File must exist
        if (!m.glsl_path.empty())
        {
            fs::path full = fs::path(g_lib_root) / m.glsl_path;
            if (!fs::exists(full))
            {
                std::fprintf(stderr, "[ProviderManifest::SelfCheck] File missing: %s\n",
                             full.string().c_str());
                ok = false;
            }
        }

        // VAB providers must have at least one VAB input spec
        if (m.kind == ProviderKind::VAB && m.consumes_vab)
        {
            bool has_vab_input = false;
            for (const auto& inp : m.inputs)
                if (inp.source == InputSource::VAB) { has_vab_input = true; break; }
            if (!has_vab_input)
            {
                std::fprintf(stderr, "[ProviderManifest::SelfCheck] VAB provider missing input spec: %s\n",
                             m.glsl_path.c_str());
                ok = false;
            }
        }
    }

    assert(ok && "[ProviderManifest] SelfCheck failed — see stderr for details");
    return ok;
}

size_t ProviderManifestRegistry::Count() noexcept
{
    return g_manifests.size();
}

} // namespace hgl::graph
