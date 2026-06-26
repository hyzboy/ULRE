#include <hgl/shadergen/Matcher.h>
#include <hgl/shadergen/ShaderResourceScanner.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace hgl::graph::mtl
{
namespace
{
    static std::string ToLowerASCII(std::string text)
    {
        std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c)
        {
            return static_cast<char>(std::tolower(c));
        });
        return text;
    }

    static bool ContainsToken(const std::set<std::string> &tokens, const std::string &token)
    {
        return tokens.find(ToLowerASCII(token)) != tokens.end();
    }

    static std::string RenderPhaseToken(const RenderPhase phase)
    {
        switch (phase)
        {
        case RenderPhase::Shadow: return "shadow";
        case RenderPhase::EarlyZ: return "earlyz";
        default: return "forward";
        }
    }

    static bool LoadTextFile(const std::string &path, std::string &out)
    {
        std::ifstream file(path, std::ios::in | std::ios::binary);
        if (!file)
            return false;

        std::ostringstream ss;
        ss << file.rdbuf();
        out = ss.str();
        return true;
    }

    static bool CheckPhaseSupport(const SFMAnnotationScanReport &report,
                                  const RenderPhase phase)
    {
        const std::string wanted = RenderPhaseToken(phase);

        bool has_explicit = false;
        for (const auto &record : report.records)
        {
            if (record.key != "supports_phase")
                continue;

            has_explicit = true;
            for (const auto &arg : record.args)
            {
                if (ToLowerASCII(arg) == wanted)
                    return true;
            }
        }

        // Phase2 contract: if supports_phase is absent, default to forward-only.
        return !has_explicit && phase == RenderPhase::Forward;
    }

    static bool CheckSurfaceType(const SFMAnnotationScanReport &report,
                                 const SurfaceType surface_type)
    {
        std::string wanted = ToLowerASCII(GetSurfaceTypeName(surface_type));

        for (const auto &record : report.records)
        {
            if (record.key != "surface_type")
                continue;

            if (record.args.empty())
                return false;

            return ToLowerASCII(record.args.front()) == wanted;
        }

        // Backward compatibility: no surface_type annotation means do not reject.
        return true;
    }

    static bool CheckRequirements(const SFMAnnotationScanReport &report,
                                  const MatcherCapabilities &caps,
                                  std::string &missing_detail)
    {
        for (const auto &record : report.records)
        {
            if (record.key != "require")
                continue;

            if (record.args.size() < 2)
                continue;

            const std::string domain = ToLowerASCII(record.args[0]);
            for (size_t i = 1; i < record.args.size(); ++i)
            {
                const std::string wanted = ToLowerASCII(record.args[i]);
                bool ok = false;

                if (domain == "va")
                    ok = ContainsToken(caps.vertex_attribs, wanted);
                else if (domain == "tex")
                    ok = ContainsToken(caps.textures, wanted);
                else if (domain == "ubo")
                    ok = ContainsToken(caps.ubos, wanted);
                else if (domain == "ssbo")
                    ok = ContainsToken(caps.ssbos, wanted);

                if (!ok)
                {
                    missing_detail = domain + ":" + wanted;
                    return false;
                }
            }
        }

        return true;
    }
}

MatchedShaderSet Matcher::Resolve(const MatcherResolveRequest &request)
{
    MatchedShaderSet out{};
    out.preset = request.preset;
    out.quality_level = request.requested_quality;
    out.render_phase = request.phase;

    if (!request.preset_table)
    {
        out.used_fallback = true;
        out.failure_reason = "MT-FALLBACK-NO-PRESET-TABLE";
        std::fprintf(stderr, "[MT-FALLBACK-NO-PRESET-TABLE] preset=%u\n", static_cast<unsigned>(request.preset));
        return out;
    }

    if (!request.shader_library_path || !request.shader_library_path[0])
    {
        out.used_fallback = true;
        out.failure_reason = "MT-FALLBACK-NO-SHADER-LIB";
        std::fprintf(stderr, "[MT-FALLBACK-NO-SHADER-LIB] preset=%u\n", static_cast<unsigned>(request.preset));
        return out;
    }

    const auto candidates = request.preset_table->Query(request.preset,
                                                        request.requested_quality,
                                                        request.phase);
    if (candidates.empty())
    {
        out.used_fallback = true;
        out.failure_reason = "MT-FALLBACK-NO-CANDIDATE";
        std::fprintf(stderr,
            "[MT-FALLBACK-NO-CANDIDATE] preset=%u phase=%u quality=%u\n",
            static_cast<unsigned>(request.preset),
            static_cast<unsigned>(request.phase),
            static_cast<unsigned>(request.requested_quality));
        return out;
    }

    for (const auto &candidate : candidates)
    {
        if (!candidate.surface_path || !candidate.surface_path[0])
            continue;

        std::string full_path = request.shader_library_path;
        if (!full_path.empty() && full_path.back() != '/' && full_path.back() != '\\')
            full_path.push_back('/');
        full_path += candidate.surface_path;

        std::string source;
        if (!LoadTextFile(full_path, source))
        {
            std::fprintf(stderr,
                "[MT-MATCH-CANDIDATE-LOAD-FAIL] preset=%u path=%s\n",
                static_cast<unsigned>(request.preset),
                candidate.surface_path);
            continue;
        }

        SFMAnnotationScanReport report;
        std::string diagnostics;
        if (!ParseSFMAnnotationsFromGLSL(source, report, &diagnostics) || report.HasErrors())
        {
            std::fprintf(stderr,
                "[MT-MATCH-CANDIDATE-PARSE-FAIL] preset=%u path=%s detail=%s\n",
                static_cast<unsigned>(request.preset),
                candidate.surface_path,
                diagnostics.c_str());
            continue;
        }

        if (!CheckPhaseSupport(report, request.phase))
        {
            std::fprintf(stderr,
                "[MT-MATCH-PHASE-UNSUPPORTED] preset=%u path=%s phase=%s\n",
                static_cast<unsigned>(request.preset),
                candidate.surface_path,
                RenderPhaseToken(request.phase).c_str());
            continue;
        }

        if (!CheckSurfaceType(report, request.surface_type))
        {
            std::fprintf(stderr,
                "[MT-MATCH-SURFACE-MISMATCH] preset=%u path=%s\n",
                static_cast<unsigned>(request.preset),
                candidate.surface_path);
            continue;
        }

        std::string missing;
        if (!CheckRequirements(report, request.capabilities, missing))
        {
            std::fprintf(stderr,
                "[MT-MATCH-REQUIRE-MISS] preset=%u path=%s miss=%s\n",
                static_cast<unsigned>(request.preset),
                candidate.surface_path,
                missing.c_str());
            continue;
        }

        out.matched = true;
        out.surface_path = candidate.surface_path;
        out.quality_level = candidate.quality_level;
        out.has_pass_override = candidate.has_pass_override;
        out.pass_override = candidate.pass_override;
        std::fprintf(stderr,
            "[MT-MATCH-HIT] preset=%u path=%s phase=%u quality=%u\n",
            static_cast<unsigned>(request.preset),
            candidate.surface_path,
            static_cast<unsigned>(request.phase),
            static_cast<unsigned>(candidate.quality_level));
        return out;
    }

    out.used_fallback = true;
    out.failure_reason = "MT-FALLBACK-NO-MATCH";
    std::fprintf(stderr,
        "[MT-FALLBACK-NO-MATCH] preset=%u phase=%u quality=%u\n",
        static_cast<unsigned>(request.preset),
        static_cast<unsigned>(request.phase),
        static_cast<unsigned>(request.requested_quality));
    return out;
}
}
