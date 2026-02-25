#include"Std3DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/graph/mtl/ShaderComposition.h>
#include<hgl/graph/mtl/MaterialCompiler.h>
#include"S_PureColor3D.h"
#include"S_PureColor3D_Logic.h"
#include<cstdio>

namespace hgl::graph::mtl{
namespace
{
    static const char *VATypeToGLSL(const VAType &type)
    {
        switch (type.basetype)
        {
            case VertexAttribBaseType::Float:
                switch (type.vec_size)
                {
                    case 1: return "float";
                    case 2: return "vec2";
                    case 3: return "vec3";
                    case 4: return "vec4";
                    default: return "vec4";
                }
            case VertexAttribBaseType::UInt:
                switch (type.vec_size)
                {
                    case 1: return "uint";
                    case 2: return "uvec2";
                    case 3: return "uvec3";
                    case 4: return "uvec4";
                    default: return "uint";
                }
            case VertexAttribBaseType::Int:
                switch (type.vec_size)
                {
                    case 1: return "int";
                    case 2: return "ivec2";
                    case 3: return "ivec3";
                    case 4: return "ivec4";
                    default: return "int";
                }
            case VertexAttribBaseType::Bool:
                switch (type.vec_size)
                {
                    case 1: return "bool";
                    case 2: return "bvec2";
                    case 3: return "bvec3";
                    case 4: return "bvec4";
                    default: return "bool";
                }
            default:
                return "vec4";
        }
    }

    static AnsiString BuildVertexGLSLFromBusiness(const ComposedMaterialDef &def)
    {
        AnsiString glsl;

        glsl += "struct VertexInput\n{\n";
        for (uint32_t i = 0; i < def.vertex_entry_count; ++i)
        {
            const auto &entry = def.vertex_entries[i];
            glsl += "    ";
            glsl += VATypeToGLSL(entry.type);
            glsl += " ";
            glsl += entry.name;
            glsl += ";\n";
        }
        glsl += "};\n\n";

        if (def.vertex_business && def.vertex_business->code)
        {
            glsl += def.vertex_business->code;
            glsl += "\n\n";
        }

        glsl += R"(
void main()
{
    VertexInput vi;
)";

        for (uint32_t i = 0; i < def.vertex_entry_count; ++i)
        {
            const auto &entry = def.vertex_entries[i];
            glsl += "    vi.";
            glsl += entry.name;
            glsl += "=";
            glsl += entry.name;
            glsl += ";\n";
        }

        glsl += R"(
    HandoverMI();
    vec4 local_pos = VertexShaderBusiness(vi);
    gl_Position = camera.vp * GetLocalToWorld() * local_pos;
}
)";

        return glsl;
    }

    static AnsiString BuildFragmentGLSLFromBusiness(const ComposedMaterialDef &def)
    {
        AnsiString glsl;

        glsl += R"(
struct VS_Output
{
    uint MaterialInstanceID;
};

)";

        if (def.fragment_business && def.fragment_business->code)
        {
            glsl += def.fragment_business->code;
            glsl += "\n\n";
        }

        glsl += R"(
void main()
{
    VS_Output vso;
    vso.MaterialInstanceID = Input.MaterialInstanceID;
    FragColor = FragmentShaderBusiness(vso);
}
)";

        return glsl;
    }

    static bool ProbePureColor3DComposedPath(const ShaderPermutationKey &key)
    {
        ComposedMaterialBuildFromLogicResult bridge_result;

        const bool bridge_ok = BuildComposedMaterialDefFromLogic(
            PURE_COLOR_3D_COMPOSED_DEF,
            PURE_COLOR_3D_LOGIC,
            bridge_result);

        if (!bridge_ok)
        {
            std::fprintf(stderr, "[PureColor3D][Composed] bridge failed, missing resources: ");
            for (size_t i = 0; i < bridge_result.diagnostics.missing_resources.size(); ++i)
            {
                std::fprintf(stderr, "%s%s",
                             i == 0 ? "" : ", ",
                             bridge_result.diagnostics.missing_resources[i].c_str());
            }
            std::fprintf(stderr, "\n");
            return false;
        }

        const AnsiString vs_code = ComposedShaderGenerator::ComposeVertexShader(bridge_result.def, key);
        const AnsiString fs_code = ComposedShaderGenerator::ComposeFragmentShader(bridge_result.def, key);

        if (vs_code.IsEmpty() || fs_code.IsEmpty())
        {
            std::fprintf(stderr, "[PureColor3D][Composed] shader compose failed: VS or FS is empty\n");
            return false;
        }

        std::fprintf(stderr,
            "[PureColor3D][Composed] bridge precheck ok, VS=%u bytes, FS=%u bytes, helpers=%u\n",
            vs_code.Length(),
            fs_code.Length(),
            uint32_t(bridge_result.def.logic_required_helpers.size()));

        return true;
    }

    constexpr const char mi_codes[]="vec4 Color;";          //材质实例代码
    constexpr const uint32_t mi_bytes=sizeof(Vector4f);     //材质实例数据大小

    constexpr const char vs_main[]=R"(
void main()
{
    MaterialInstance mi=GetMI();

    Output.Color=mi.Color;

    gl_Position=GetPosition3D();
})";

    //一个shader中输出的所有数据，会被定义在一个名为Output的结构中。所以编写时要用Output.XXXX来使用。
    //而同时，这个结构在下一个Shader中以Input名称出现，使用时以Input.XXX的形式使用。

    constexpr const char fs_main[]=R"(
void main()
{
    FragColor=Input.Color;
})";// ^       ^
    // |       |
    // |       +--ps:这里的Input.Color就是上一个Shader中的Output.Color
    // +--ps:这里的Color就是最终的RT

    class MaterialPureColor3D:public Std3DMaterial
    {
    public:

        using Std3DMaterial::Std3DMaterial;
        ~MaterialPureColor3D()=default;

        bool CustomVertexShader(ShaderCreateInfoVertex *vsc) override
        {
            if(!Std3DMaterial::CustomVertexShader(vsc))
                return(false);

            vsc->AddOutput(SVT_VEC4,"Color");

            vsc->SetMain(vs_main);
            return(true);
        }

        bool CustomFragmentShader(ShaderCreateInfoFragment *fsc) override
        {
            fsc->AddOutput(VAT_VEC4,"FragColor");       //Fragment shader的输出等于最终的RT了，所以这个名称其实随便起。

            fsc->SetMain(fs_main);
            return(true);
        }

        bool EndCustomShader() override
        {
            mci->SetMaterialInstance(   mi_codes,                       //材质实例glsl代码
                                        mi_bytes,                       //材质实例数据大小
                                        VK_SHADER_STAGE_VERTEX_BIT);    //只在Vertex Shader中使用材质实例最终数据

            return(true);
        }
    };//class MaterialPureColor3D:public Std3DMaterial
}//namespace

MaterialCreateInfo *CreatePureColor3D(const VulkanDevAttr *dev_attr,Material3DCreateConfig *cfg)
{
    ShaderPermutationKey key;
    const bool composed_precheck_ok = ProbePureColor3DComposedPath(key);

    if (composed_precheck_ok)
    {
        ComposedMaterialBuildFromLogicResult bridge_result;
        const bool bridge_ok = BuildComposedMaterialDefFromLogic(
            PURE_COLOR_3D_COMPOSED_DEF,
            PURE_COLOR_3D_LOGIC,
            bridge_result);

        if (!bridge_ok)
        {
            std::fprintf(stderr,
                "[PureColor3D] bridge failed after precheck, fallback to legacy path\n");

            MaterialPureColor3D mvc3d(cfg);
            return mvc3d.Create(dev_attr);
        }

        AnsiString generated_vs = BuildVertexGLSLFromBusiness(bridge_result.def);
        AnsiString generated_fs = BuildFragmentGLSLFromBusiness(bridge_result.def);

        FixedMaterialDef runtime_def = PURE_COLOR_3D_DEF;
        runtime_def.vert_glsl = generated_vs.c_str();
        runtime_def.frag_glsl = generated_fs.c_str();

        MaterialCreateInfo *mci_new = CompileFixedMaterial(dev_attr, runtime_def, key);

        if (mci_new)
        {
            std::fprintf(stderr,
                "[PureColor3D] using new composed-business compile path\n");
            return mci_new;
        }

        std::fprintf(stderr,
            "[PureColor3D] composed-business compile failed, fallback to legacy Std3DMaterial path\n");
    }
    else
    {
        std::fprintf(stderr,
            "[PureColor3D] fallback to legacy Std3DMaterial path (reason: composed precheck failed)\n");
    }

    MaterialPureColor3D mvc3d(cfg);

    return mvc3d.Create(dev_attr);
}
}//namespace hgl::graph::mtl
