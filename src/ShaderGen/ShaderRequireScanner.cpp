#include <hgl/shadergen/ShaderRequireScanner.h>
#include <hgl/mtl/DescriptorBindingContract.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace hgl::graph::mtl
{
namespace
{
    struct PreprocessFrame
    {
        bool parent_active = true;
        bool branch_taken = false;
        bool active = true;
    };

    static std::string Trim(const std::string &text)
    {
        size_t begin = 0;
        while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin])) != 0)
            ++begin;

        size_t end = text.size();
        while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
            --end;

        return text.substr(begin, end - begin);
    }

    static std::string ToUpperASCII(const std::string &text)
    {
        std::string out;
        out.reserve(text.size());

        for (const unsigned char c : text)
        {
            if (c >= 'a' && c <= 'z')
                out.push_back(char(c - ('a' - 'A')));
            else
                out.push_back(char(c));
        }

        return out;
    }

    static bool EqualsNoCase(const std::string &a, const std::string &b)
    {
        return ToUpperASCII(a) == ToUpperASCII(b);
    }

    static std::string JoinPath(const std::string &root, const std::string &relative)
    {
        if (root.empty())
            return relative;

        if (relative.empty())
            return root;

        if (root.back() == '/' || root.back() == '\\')
            return root + relative;

        return root + "/" + relative;
    }

    static bool ReadTextFile(const std::string &path, std::string &out)
    {
        std::ifstream ifs(path, std::ios::in);
        if (!ifs.is_open())
            return false;

        std::ostringstream ss;
        ss << ifs.rdbuf();
        out = ss.str();
        return true;
    }

    static bool ParseDirective(const std::string &line, std::string &directive, std::string &rest)
    {
        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] != '#')
            return false;

        size_t i = 1;
        while (i < trimmed.size() && std::isspace(static_cast<unsigned char>(trimmed[i])) != 0)
            ++i;

        const size_t begin = i;
        while (i < trimmed.size() && (std::isalnum(static_cast<unsigned char>(trimmed[i])) != 0 || trimmed[i] == '_'))
            ++i;

        if (i <= begin)
            return false;

        directive = ToUpperASCII(trimmed.substr(begin, i - begin));
        rest = Trim(trimmed.substr(i));
        return true;
    }

    static bool SplitTopLevel(const std::string &text, const std::string &token, std::vector<std::string> &parts)
    {
        parts.clear();
        if (text.empty())
            return false;

        size_t begin = 0;
        bool found = false;
        while (begin <= text.size())
        {
            const size_t pos = text.find(token, begin);
            if (pos == std::string::npos)
            {
                parts.emplace_back(Trim(text.substr(begin)));
                break;
            }

            found = true;
            parts.emplace_back(Trim(text.substr(begin, pos - begin)));
            begin = pos + token.size();
        }

        return found;
    }

    static bool IsMacroDefined(const std::unordered_set<std::string> &defines, const std::string &name)
    {
        return defines.find(ToUpperASCII(name)) != defines.end();
    }

    static bool EvalIfAtom(const std::string &expr, const std::unordered_set<std::string> &defines)
    {
        std::string atom = Trim(expr);
        if (atom.empty())
            return false;

        if (atom.front() == '(' && atom.back() == ')' && atom.size() >= 2)
            atom = Trim(atom.substr(1, atom.size() - 2));

        if (atom.empty())
            return false;

        if (atom[0] == '!')
            return !EvalIfAtom(atom.substr(1), defines);

        constexpr std::string_view kDefined = "defined";
        if (atom.size() > kDefined.size() && ToUpperASCII(atom.substr(0, kDefined.size())) == "DEFINED")
        {
            const size_t l = atom.find('(');
            const size_t r = atom.rfind(')');
            if (l == std::string::npos || r == std::string::npos || r <= l + 1)
                return false;

            return IsMacroDefined(defines, atom.substr(l + 1, r - l - 1));
        }

        if (atom == "1")
            return true;

        if (atom == "0")
            return false;

        return IsMacroDefined(defines, atom);
    }

    static bool EvalIfExpression(const std::string &expr, const std::unordered_set<std::string> &defines)
    {
        std::vector<std::string> or_parts;
        if (SplitTopLevel(expr, "||", or_parts))
        {
            for (const std::string &or_part : or_parts)
            {
                std::vector<std::string> and_parts;
                if (SplitTopLevel(or_part, "&&", and_parts))
                {
                    bool all_true = true;
                    for (const std::string &and_part : and_parts)
                    {
                        if (!EvalIfAtom(and_part, defines))
                        {
                            all_true = false;
                            break;
                        }
                    }

                    if (all_true)
                        return true;
                }
                else if (EvalIfAtom(or_part, defines))
                {
                    return true;
                }
            }

            return false;
        }

        std::vector<std::string> and_parts;
        if (SplitTopLevel(expr, "&&", and_parts))
        {
            for (const std::string &and_part : and_parts)
            {
                if (!EvalIfAtom(and_part, defines))
                    return false;
            }
            return true;
        }

        return EvalIfAtom(expr, defines);
    }

    static bool ParseIncludePath(const std::string &directive_rest, std::string &path)
    {
        const size_t q0 = directive_rest.find('"');
        if (q0 == std::string::npos)
            return false;

        const size_t q1 = directive_rest.find('"', q0 + 1);
        if (q1 == std::string::npos || q1 <= q0 + 1)
            return false;

        path = directive_rest.substr(q0 + 1, q1 - q0 - 1);
        return !path.empty();
    }

    static bool ParseCommaArgs(const std::string &args, std::vector<std::string> &out)
    {
        out.clear();

        size_t begin = 0;
        while (begin <= args.size())
        {
            const size_t comma = args.find(',', begin);
            if (comma == std::string::npos)
            {
                out.emplace_back(Trim(args.substr(begin)));
                break;
            }

            out.emplace_back(Trim(args.substr(begin, comma - begin)));
            begin = comma + 1;
        }

        return !out.empty();
    }

    static bool ParseUBOSemantic(const std::string &name, UBODescriptorSemantic &semantic)
    {
        for (size_t i = 0; i < UBODescriptorSemanticCount; ++i)
        {
            const UBODescriptorSemantic candidate = static_cast<UBODescriptorSemantic>(i);
            if (!IsBuiltinDescriptorSemantic(candidate))
                continue;

            if (EqualsNoCase(name, GetUBODescriptorSemanticName(candidate)))
            {
                semantic = candidate;
                return true;
            }
        }

        return false;
    }

    static bool ParseSSBOSemantic(const std::string &name, SSBODescriptorSemantic &semantic)
    {
        for (size_t i = 0; i < SSBODescriptorSemanticCount; ++i)
        {
            const SSBODescriptorSemantic candidate = static_cast<SSBODescriptorSemantic>(i);
            if (!IsBuiltinDescriptorSemantic(candidate))
                continue;

            if (EqualsNoCase(name, GetSSBODescriptorSemanticName(candidate)))
            {
                semantic = candidate;
                return true;
            }
        }

        return false;
    }

    static bool ParseSamplerSlot(const std::string &name, SamplerSlot &slot)
    {
        for (size_t i = 0; i < SamplerSlotCount; ++i)
        {
            if (EqualsNoCase(name, SamplerSlotNameList[i]))
            {
                slot = static_cast<SamplerSlot>(i);
                return true;
            }
        }

        return false;
    }

    static bool ParseTextureChannelHint(const std::string &name, TextureChannelHint &hint)
    {
        if (EqualsNoCase(name, "RGBA"))
        {
            hint = TextureChannelHint::RGBA;
            return true;
        }

        if (EqualsNoCase(name, "GRAYSCALE"))
        {
            hint = TextureChannelHint::Grayscale;
            return true;
        }

        return false;
    }

    static bool ParseRequire(const std::string &line,
                             const uint32_t stage_flags,
                             ShaderAutoRequirements &out_requirements,
                             std::string *diagnostics)
    {
        const size_t marker = line.find("@require");
        if (marker == std::string::npos)
            return false;

        std::string payload = Trim(line.substr(marker + 8));
        const size_t l = payload.find('(');
        const size_t r = payload.rfind(')');
        if (l == std::string::npos || r == std::string::npos || r <= l)
            return false;

        const std::string kind = ToUpperASCII(Trim(payload.substr(0, l)));
        const std::string args_text = payload.substr(l + 1, r - l - 1);

        std::vector<std::string> args;
        ParseCommaArgs(args_text, args);

        if (kind == "UBO")
        {
            if (args.empty())
                return false;

            UBODescriptorSemantic semantic = UBODescriptorSemantic::Unknown;
            if (!ParseUBOSemantic(args[0], semantic))
            {
                if (diagnostics)
                    *diagnostics += "Unknown UBO semantic in @require: " + args[0] + "\n";
                return false;
            }

            AddFixedUBODescriptor(out_requirements.ubos, semantic, stage_flags);
            return true;
        }

        if (kind == "SSBO")
        {
            if (args.empty())
                return false;

            SSBODescriptorSemantic semantic = SSBODescriptorSemantic::Unknown;
            if (!ParseSSBOSemantic(args[0], semantic))
            {
                if (diagnostics)
                    *diagnostics += "Unknown SSBO semantic in @require: " + args[0] + "\n";
                return false;
            }

            AddFixedSSBODescriptor(out_requirements.ssbos, semantic, stage_flags);
            return true;
        }

        if (kind == "TEX")
        {
            if (args.empty())
                return false;

            SamplerSlot slot = SamplerSlot::BaseColor;
            if (!ParseSamplerSlot(args[0], slot))
            {
                if (diagnostics)
                    *diagnostics += "Unknown SamplerSlot in @require TEX: " + args[0] + "\n";
                return false;
            }

            SamplerType sampler_type = SamplerType::Sampler2D;
            if (args.size() >= 2 && !args[1].empty())
            {
                sampler_type = ParseSamplerType(args[1].c_str(), int(args[1].size()));
                if (sampler_type == SamplerType::Error)
                {
                    if (diagnostics)
                        *diagnostics += "Unknown SamplerType in @require TEX: " + args[1] + "\n";
                    return false;
                }
            }

            TextureChannelHint channel_hint = TextureChannelHint::RGBA;
            if (args.size() >= 3 && !args[2].empty())
            {
                if (!ParseTextureChannelHint(args[2], channel_hint))
                {
                    if (diagnostics)
                        *diagnostics += "Unknown TextureChannelHint in @require TEX: " + args[2] + "\n";
                    return false;
                }
            }

            auto iter = out_requirements.samplers.find(slot);
            if (iter == out_requirements.samplers.end())
            {
                AddFixedTextureSampler(out_requirements.samplers,
                                       slot,
                                       stage_flags,
                                       sampler_type,
                                       SET_TYPE_MATERIAL,
                                       0,
                                       0,
                                       channel_hint);
            }
            else
            {
                iter->second.stage_flags |= stage_flags;
            }

            return true;
        }

        return false;
    }

    static bool ScanSourceRecursive(const std::string &source,
                                    const uint32_t stage_flags,
                                    const std::string &shader_library_path,
                                    ShaderAutoRequirements &out_requirements,
                                    std::unordered_set<std::string> &visited_rel_files,
                                    std::unordered_set<std::string> &defines,
                                    std::string *diagnostics);

    static bool ScanIncludeFile(const std::string &include_path,
                                const uint32_t stage_flags,
                                const std::string &shader_library_path,
                                ShaderAutoRequirements &out_requirements,
                                std::unordered_set<std::string> &visited_rel_files,
                                std::unordered_set<std::string> &defines,
                                std::string *diagnostics)
    {
        const std::string include_key = ToUpperASCII(include_path);
        if (visited_rel_files.find(include_key) != visited_rel_files.end())
            return true;

        visited_rel_files.insert(include_key);

        std::string content;
        if (!ReadTextFile(JoinPath(shader_library_path, include_path), content))
        {
            if (diagnostics)
                *diagnostics += "Failed to read include for @require scan: " + include_path + "\n";
            return false;
        }

        return ScanSourceRecursive(content,
                                   stage_flags,
                                   shader_library_path,
                                   out_requirements,
                                   visited_rel_files,
                                   defines,
                                   diagnostics);
    }

    static bool ScanSourceRecursive(const std::string &source,
                                    const uint32_t stage_flags,
                                    const std::string &shader_library_path,
                                    ShaderAutoRequirements &out_requirements,
                                    std::unordered_set<std::string> &visited_rel_files,
                                    std::unordered_set<std::string> &defines,
                                    std::string *diagnostics)
    {
        std::istringstream ss(source);
        std::string line;
        std::vector<PreprocessFrame> pp_stack;

        bool ok = true;

        auto current_active = [&pp_stack]() -> bool
        {
            if (pp_stack.empty())
                return true;

            return pp_stack.back().active;
        };

        while (std::getline(ss, line))
        {
            std::string directive;
            std::string rest;
            const bool has_directive = ParseDirective(line, directive, rest);

            if (has_directive)
            {
                if (directive == "IF" || directive == "IFDEF" || directive == "IFNDEF")
                {
                    const bool parent_active = current_active();
                    bool cond = false;

                    if (directive == "IFDEF")
                        cond = IsMacroDefined(defines, rest);
                    else if (directive == "IFNDEF")
                        cond = !IsMacroDefined(defines, rest);
                    else
                        cond = EvalIfExpression(rest, defines);

                    PreprocessFrame frame;
                    frame.parent_active = parent_active;
                    frame.branch_taken = cond;
                    frame.active = parent_active && cond;
                    pp_stack.push_back(frame);
                    continue;
                }

                if (directive == "ELIF")
                {
                    if (pp_stack.empty())
                        continue;

                    PreprocessFrame &frame = pp_stack.back();
                    const bool cond = EvalIfExpression(rest, defines);

                    if (!frame.parent_active || frame.branch_taken)
                    {
                        frame.active = false;
                    }
                    else
                    {
                        frame.active = cond;
                        if (cond)
                            frame.branch_taken = true;
                    }
                    continue;
                }

                if (directive == "ELSE")
                {
                    if (pp_stack.empty())
                        continue;

                    PreprocessFrame &frame = pp_stack.back();
                    frame.active = frame.parent_active && !frame.branch_taken;
                    frame.branch_taken = true;
                    continue;
                }

                if (directive == "ENDIF")
                {
                    if (!pp_stack.empty())
                        pp_stack.pop_back();
                    continue;
                }
            }

            if (!current_active())
                continue;

            if (has_directive && directive == "DEFINE")
            {
                const std::string macro = Trim(rest);
                if (!macro.empty())
                {
                    size_t end = 0;
                    while (end < macro.size() && (std::isalnum(static_cast<unsigned char>(macro[end])) != 0 || macro[end] == '_'))
                        ++end;
                    if (end > 0)
                        defines.insert(ToUpperASCII(macro.substr(0, end)));
                }
                continue;
            }

            ParseRequire(line, stage_flags, out_requirements, diagnostics);

            if (has_directive && directive == "INCLUDE")
            {
                std::string include_path;
                if (ParseIncludePath(rest, include_path))
                {
                    if (!ScanIncludeFile(include_path,
                                         stage_flags,
                                         shader_library_path,
                                         out_requirements,
                                         visited_rel_files,
                                         defines,
                                         diagnostics))
                    {
                        ok = false;
                    }
                }
            }
        }

        return ok;
    }

    static bool ScanShaderSource(const std::string &shader_source,
                                 const uint32_t stage_flags,
                                 const std::string &shader_library_path,
                                 ShaderAutoRequirements &out_requirements,
                                 std::string *diagnostics)
    {
        std::unordered_set<std::string> visited_rel_files;
        std::unordered_set<std::string> defines;

        return ScanSourceRecursive(shader_source,
                                   stage_flags,
                                   shader_library_path,
                                   out_requirements,
                                   visited_rel_files,
                                   defines,
                                   diagnostics);
    }

    static const char *ToTextureChannelHintName(const TextureChannelHint hint)
    {
        switch (hint)
        {
        case TextureChannelHint::RGBA: return "RGBA";
        case TextureChannelHint::Grayscale: return "Grayscale";
        default: return "Unknown";
        }
    }

    static void DumpCollectedRequirements(const ShaderAutoRequirements &requirements)
    {
        if (requirements.ubos.empty())
        {
            std::fprintf(stderr, "[ShaderRequireScanner]   UBO: (none)\n");
        }
        else
        {
            for (const auto &[semantic, stage_flags] : requirements.ubos)
            {
                std::fprintf(stderr,
                             "[ShaderRequireScanner]   UBO: semantic=%s, stage_flags=0x%08X\n",
                             GetUBODescriptorSemanticName(semantic),
                             stage_flags);
            }
        }

        if (requirements.ssbos.empty())
        {
            std::fprintf(stderr, "[ShaderRequireScanner]   SSBO: (none)\n");
        }
        else
        {
            for (const auto &[semantic, stage_flags] : requirements.ssbos)
            {
                std::fprintf(stderr,
                             "[ShaderRequireScanner]   SSBO: semantic=%s, stage_flags=0x%08X\n",
                             GetSSBODescriptorSemanticName(semantic),
                             stage_flags);
            }
        }

        if (requirements.samplers.empty())
        {
            std::fprintf(stderr, "[ShaderRequireScanner]   TEX: (none)\n");
        }
        else
        {
            for (const auto &[slot, sampler] : requirements.samplers)
            {
                std::fprintf(stderr,
                             "[ShaderRequireScanner]   TEX: slot=%s, type=%s, channel=%s, stage_flags=0x%08X\n",
                             SamplerSlotNameList[size_t(slot)],
                             GetSamplerTypeName(sampler.sampler_type),
                             ToTextureChannelHintName(sampler.channel_hint),
                             sampler.stage_flags);
            }
        }
    }
}

bool CollectShaderAutoRequirements(const std::string &shader_library_path,
                                   const std::string &vertex_glsl,
                                   const std::string &fragment_glsl,
                                   ShaderAutoRequirements &out_requirements,
                                   std::string *diagnostics)
{
    std::fprintf(stderr,
                 "[ShaderRequireScanner] Collect begin: shader_lib='%s', vs_bytes=%zu, fs_bytes=%zu\n",
                 shader_library_path.c_str(),
                 vertex_glsl.size(),
                 fragment_glsl.size());

    out_requirements.ubos.clear();
    out_requirements.ssbos.clear();
    out_requirements.samplers.clear();

    bool vs_ok = true;
    bool fs_ok = true;

    if (!vertex_glsl.empty())
    {
        vs_ok = ScanShaderSource(vertex_glsl,
                                 uint32_t(VK_SHADER_STAGE_VERTEX_BIT),
                                 shader_library_path,
                                 out_requirements,
                                 diagnostics);

        std::fprintf(stderr,
                     "[ShaderRequireScanner] VS scan: ok=%d, ubos=%zu, ssbos=%zu, samplers=%zu\n",
                     vs_ok ? 1 : 0,
                     out_requirements.ubos.size(),
                     out_requirements.ssbos.size(),
                     out_requirements.samplers.size());
    }

    if (!fragment_glsl.empty())
    {
        fs_ok = ScanShaderSource(fragment_glsl,
                                 uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT),
                                 shader_library_path,
                                 out_requirements,
                                 diagnostics);

        std::fprintf(stderr,
                     "[ShaderRequireScanner] FS scan: ok=%d, ubos=%zu, ssbos=%zu, samplers=%zu\n",
                     fs_ok ? 1 : 0,
                     out_requirements.ubos.size(),
                     out_requirements.ssbos.size(),
                     out_requirements.samplers.size());
    }

    if (diagnostics && !diagnostics->empty())
    {
        std::fprintf(stderr,
                     "[ShaderRequireScanner] diagnostics(%zu):\n%s",
                     diagnostics->size(),
                     diagnostics->c_str());
    }

    std::fprintf(stderr,
                 "[ShaderRequireScanner] Collect end: ok=%d, ubos=%zu, ssbos=%zu, samplers=%zu\n",
                 (vs_ok && fs_ok) ? 1 : 0,
                 out_requirements.ubos.size(),
                 out_requirements.ssbos.size(),
                 out_requirements.samplers.size());

    DumpCollectedRequirements(out_requirements);

    return vs_ok && fs_ok;
}

void MergeShaderAutoRequirements(const FixedMaterialDef &base_def,
                                 const ShaderAutoRequirements &auto_requirements,
                                 FixedMaterialDef &out_def,
                                 FixedUBODescriptors &ubo_storage,
                                 FixedSSBODescriptors &ssbo_storage,
                                 FixedTextureSamplerDescriptors &sampler_storage)
{
    ubo_storage.clear();
    ssbo_storage.clear();
    sampler_storage.clear();

    if (base_def.ubo_descriptors)
        ubo_storage = *base_def.ubo_descriptors;

    if (base_def.ssbo_descriptors)
        ssbo_storage = *base_def.ssbo_descriptors;

    if (base_def.texture_samplers)
        sampler_storage = *base_def.texture_samplers;

    for (const auto &[semantic, stage_flags] : auto_requirements.ubos)
        AddFixedUBODescriptor(ubo_storage, semantic, stage_flags);

    for (const auto &[semantic, stage_flags] : auto_requirements.ssbos)
        AddFixedSSBODescriptor(ssbo_storage, semantic, stage_flags);

    for (const auto &[slot, sampler] : auto_requirements.samplers)
    {
        auto iter = sampler_storage.find(slot);
        if (iter == sampler_storage.end())
        {
            sampler_storage.emplace(slot, sampler);
        }
        else
        {
            iter->second.stage_flags |= sampler.stage_flags;
        }
    }

    out_def = base_def;
    out_def.ubo_descriptors = ubo_storage.empty() ? nullptr : &ubo_storage;
    out_def.ssbo_descriptors = ssbo_storage.empty() ? nullptr : &ssbo_storage;
    out_def.texture_samplers = sampler_storage.empty() ? nullptr : &sampler_storage;
}
}
