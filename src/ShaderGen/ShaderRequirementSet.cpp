#include <hgl/shadergen/ShaderRequirementSet.h>
#include <hgl/shadergen/ShaderLibraryPath.h>
#include <hgl/mtl/DescriptorSemanticRegistry.h>
#include <algorithm>
#include <sstream>
#include <cstring>
#include <fstream>
#include <vulkan/vulkan.h>

namespace hgl::graph
{
    // ─────────────────────────────────────────────────────────────────────────
    // 语义名 → GLSL include 路径的静态映射表
    // key: "UBO:<sem_name>" 或 "SSBO:<sem_name>"
    // ─────────────────────────────────────────────────────────────────────────
    struct SemIncludePair { const char *sem_name; const char *glsl_include; };

    static constexpr SemIncludePair kUBOIncludes[] = {
        { "viewport",     "common/ubo_viewport.glsl"          },
        { "camera",       "common/ubo_camera.glsl"            },
        { "sky",          "common/ubo_sky.glsl"               },
        { "color_palette","common/ubo_color_palette.glsl"     },
    };

    static constexpr SemIncludePair kSSBOIncludes[] = {
        { "transform_id",   "common/ssbo_transform.glsl"           },
        { "transform_data", "common/ssbo_transform.glsl"           },  // 同文件，重复 include 由 guard 保护
        { "mbi_id",         "common/ssbo_material_instance.glsl"   },
        { "mbi_data",       "common/ssbo_material_instance.glsl"   },
        { "mbi_texture",    "common/ssbo_material_instance.glsl"   },
        { "joint",          "common/ssbo_bone.glsl"                },
        { "joint_weight",   "common/ssbo_bone.glsl"                },
    };

    static const char *FindIncludePath(mtl::DescriptorKind kind, std::string_view sem_name)
    {
        if (kind == mtl::DescriptorKind::UBO)
        {
            for (const auto &p : kUBOIncludes)
                if (sem_name == p.sem_name) return p.glsl_include;
        }
        else if (kind == mtl::DescriptorKind::SSBO)
        {
            for (const auto &p : kSSBOIncludes)
                if (sem_name == p.sem_name) return p.glsl_include;
        }
        return nullptr;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // LookupSemantic: 从 DescriptorSemanticRegistry 查找语义名 → set_type
    // ─────────────────────────────────────────────────────────────────────────
    /*static*/ bool ShaderRequirementSet::LookupSemantic(
        mtl::DescriptorKind kind,
        std::string_view sem_name,
        ShaderRequirement &out)
    {
        using namespace mtl;

        if (kind == DescriptorKind::UBO)
        {
            for (size_t i = 1; i < UBODescriptorSemanticCount; ++i)
            {
                const auto &meta = UBODescriptorSemanticMetaList[i];
                if (meta.name && sem_name == meta.name)
                {
                    out.kind        = DescriptorKind::UBO;
                    out.set_type    = meta.set_type;
                    out.sem_name    = meta.name;
                    out.glsl_include = FindIncludePath(kind, sem_name);
                    return true;
                }
            }
        }
        else if (kind == DescriptorKind::SSBO)
        {
            for (size_t i = 1; i < SSBODescriptorSemanticCount; ++i)
            {
                const auto &meta = SSBODescriptorSemanticMetaList[i];
                if (meta.name && sem_name == meta.name)
                {
                    out.kind        = DescriptorKind::SSBO;
                    out.set_type    = meta.set_type;
                    out.sem_name    = meta.name;
                    out.glsl_include = FindIncludePath(kind, sem_name);
                    return true;
                }
            }
        }
        return false;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // MakeKey
    // ─────────────────────────────────────────────────────────────────────────
    /*static*/ std::string ShaderRequirementSet::MakeKey(
        mtl::DescriptorKind kind, std::string_view sem_name)
    {
        std::string key;
        key.reserve(16 + sem_name.size());
        key += (kind == mtl::DescriptorKind::UBO) ? "UBO:" : "SSBO:";
        key += sem_name;
        return key;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Add
    // ─────────────────────────────────────────────────────────────────────────
    void ShaderRequirementSet::Add(const ShaderRequirement &req)
    {
        if (!req.sem_name) return;

        const std::string key = MakeKey(req.kind, req.sem_name);
        if (std::find(seen_keys_.begin(), seen_keys_.end(), key) != seen_keys_.end())
            return; // 已存在，去重

        seen_keys_.push_back(key);
        buckets_[req.set_type].push_back(req);
        ++total_count_;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // ParseFromGLSLSource
    //
    // 解析格式：
    //   // @sfm:require  UBO camera
    //   // @sfm:require  SSBO transform_id
    //   // @sfm:no-require
    //
    // 遇第一个非注释/非空行停止。
    // ─────────────────────────────────────────────────────────────────────────
    void ShaderRequirementSet::ParseFromGLSLSource(std::string_view src)
    {
        std::string_view rest = src;

        while (!rest.empty())
        {
            // 找行尾
            const size_t lf = rest.find('\n');
            std::string_view line = (lf == std::string_view::npos) ? rest : rest.substr(0, lf);
            rest = (lf == std::string_view::npos) ? std::string_view{} : rest.substr(lf + 1);

            // 去前导空白
            while (!line.empty() && (line.front() == ' ' || line.front() == '\t'))
                line.remove_prefix(1);

            if (line.empty()) continue; // 空行跳过

            if (line.substr(0, 2) != "//")
                break; // 遇到非注释行，停止解析

            // 取 // 后内容，再去前导空白
            std::string_view body = line.substr(2);
            while (!body.empty() && (body.front() == ' ' || body.front() == '\t'))
                body.remove_prefix(1);

            // 匹配 @sfm:require
            constexpr std::string_view kRequire = "@sfm:require";
            if (body.substr(0, kRequire.size()) != kRequire)
                continue;

            body.remove_prefix(kRequire.size());
            while (!body.empty() && (body.front() == ' ' || body.front() == '\t'))
                body.remove_prefix(1);

            // 解析 KIND semname
            // 提取 kind token
            const size_t sp = body.find_first_of(" \t");
            if (sp == std::string_view::npos) continue;

            const std::string_view kind_token = body.substr(0, sp);
            body.remove_prefix(sp);
            while (!body.empty() && (body.front() == ' ' || body.front() == '\t'))
                body.remove_prefix(1);

            // 提取 sem_name token（到空白或行尾）
            const size_t sem_end = body.find_first_of(" \t\r\n");
            const std::string_view sem_token = (sem_end == std::string_view::npos)
                                               ? body
                                               : body.substr(0, sem_end);
            if (sem_token.empty()) continue;

            mtl::DescriptorKind kind;
            if (kind_token == "UBO")       kind = mtl::DescriptorKind::UBO;
            else if (kind_token == "SSBO") kind = mtl::DescriptorKind::SSBO;
            else continue; // 未知类型，跳过

            ShaderRequirement req;
            if (LookupSemantic(kind, sem_token, req))
                Add(req);
            // 未识别的语义名直接跳过（不打断解析）
        }
    }

    // ─────────────────────────────────────────────────────────────────────────
    // ParseFromGLSLFile
    // ─────────────────────────────────────────────────────────────────────────
    void ShaderRequirementSet::ParseFromGLSLFile(std::string_view rel_path)
    {
        if (rel_path.empty()) return;

        std::string full_path = GetShaderLibraryPath();
        full_path += '/';
        full_path.append(rel_path.data(), rel_path.size());

        std::ifstream f(full_path);
        if (!f.is_open()) return;

        // 只读顶部注释块，无需加载整个文件
        std::string src;
        src.reserve(2048);
        std::string line;
        while (std::getline(f, line))
        {
            // 去前导空白后判断是否还是注释行
            const char *p = line.c_str();
            while (*p == ' ' || *p == '\t') ++p;
            const bool is_comment = (p[0] == '/' && p[1] == '/');
            const bool is_empty   = (*p == '\0' || *p == '\r');

            src += line;
            src += '\n';

            // 遇到第一个非注释/非空行就停止读取
            if (!is_comment && !is_empty)
                break;
        }

        ParseFromGLSLSource(src);
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Requires
    // ─────────────────────────────────────────────────────────────────────────
    bool ShaderRequirementSet::Requires(std::string_view sem_name) const
    {
        for (const auto &k : seen_keys_)
        {
            // key 格式是 "UBO:xxx" / "SSBO:xxx"，只比较冒号后的部分
            const size_t colon = k.find(':');
            if (colon != std::string::npos && std::string_view(k).substr(colon + 1) == sem_name)
                return true;
        }
        return false;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // GetRequirements
    // ─────────────────────────────────────────────────────────────────────────
    const std::vector<ShaderRequirement> &ShaderRequirementSet::GetRequirements(
        DescriptorSetType set_type) const
    {
        const auto it = buckets_.find(set_type);
        if (it == buckets_.end()) return s_empty_bucket_;
        return it->second;
    }

    const std::vector<ShaderRequirement> ShaderRequirementSet::s_empty_bucket_ = {};

    // ─────────────────────────────────────────────────────────────────────────
    // EmitIncludes
    // ─────────────────────────────────────────────────────────────────────────
    std::string ShaderRequirementSet::EmitIncludes() const
    {
        // 去重：同一 include 路径只输出一次（transform_id / transform_data 同文件）
        std::vector<const char *> emitted;
        std::string out;

        // 按 set_type 顺序遍历（Static < PerFrame < PerObject < PerMaterial）
        for (int st = 0; st < static_cast<int>(DescriptorSetType::RANGE_SIZE); ++st)
        {
            const auto it = buckets_.find(static_cast<DescriptorSetType>(st));
            if (it == buckets_.end()) continue;

            for (const auto &req : it->second)
            {
                if (!req.glsl_include) continue;
                // 路径去重
                const bool already = std::any_of(emitted.begin(), emitted.end(),
                    [&](const char *p){ return std::strcmp(p, req.glsl_include) == 0; });
                if (already) continue;

                emitted.push_back(req.glsl_include);
                out += "#include \"";
                out += req.glsl_include;
                out += "\"\n";
            }
        }
        return out;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // GetVkBindings
    // ─────────────────────────────────────────────────────────────────────────
    std::vector<VkDescriptorSetLayoutBinding>
    ShaderRequirementSet::GetVkBindings(DescriptorSetType set_type) const
    {
        std::vector<VkDescriptorSetLayoutBinding> result;

        const auto it = buckets_.find(set_type);
        if (it == buckets_.end()) return result;

        uint32_t binding_idx = 0;
        for (const auto &req : it->second)
        {
            VkDescriptorSetLayoutBinding b{};
            b.binding         = binding_idx++;
            b.descriptorCount = 1;
            b.stageFlags      = VK_SHADER_STAGE_ALL_GRAPHICS;
            b.pImmutableSamplers = nullptr;

            switch (req.kind)
            {
            case mtl::DescriptorKind::UBO:
                b.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                break;
            case mtl::DescriptorKind::SSBO:
                b.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                break;
            case mtl::DescriptorKind::Texture:
            case mtl::DescriptorKind::TextureSampler:
                b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                break;
            }
            result.push_back(b);
        }
        return result;
    }

} // namespace hgl::graph
