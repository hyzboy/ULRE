#include"MaterialFileData.h"
#include"Std3DMaterial.h"
#include<hgl/shadergen/MaterialCreateInfo.h>

STD_MTL_NAMESPACE_BEGIN

const char *GetUBOCodes(const AccumMemoryManager::Block *block);

namespace
{
    class Std3DMaterialLoader: public Std3DMaterial
    {
    protected:

        material_file::MaterialFileData *mfd;

    public:

        Std3DMaterialLoader(material_file::MaterialFileData *data,const Material3DCreateConfig *c)
            : Std3DMaterial(c)
        {
            mfd=data;
        }

        ~Std3DMaterialLoader()
        {
            delete mfd;
        }

        bool BeginCustomShader() override
        {
            if(!Std3DMaterial::BeginCustomShader())
                return (false);

            for(const auto ubo:mfd->ubo_list)
            {
                const char *ubo_codes=GetUBOCodes(ubo.block);

                mci->AddStruct(ubo.struct_name,ubo_codes);

                mci->AddUBO(ubo.shader_stage_flag_bits,
                            ubo.set,
                            ubo.struct_name,
                            ubo.name);
            }

            if(mfd->mi.mi_bytes>0)
            {
                mci->SetMaterialInstance(mfd->mi.code,
                                         mfd->mi.mi_bytes,
                                         mfd->mi.shader_stage_flag_bits);
            }

            return true;
        }

        material_file::ShaderData *CommonProc(VkShaderStageFlagBits ss,ShaderCreateInfo *sc)
        {
            material_file::ShaderData *sd=mfd->shader[ss];

            if(!sd)
                return (nullptr);

            for(auto &ua:sd->output)
                sc->AddOutput(ua.vat,ua.name);

            for(auto &s:sd->sampler)
                mci->AddSampler(ss,DescriptorSetType::PerMaterial,s.type,s.name);

            sc->SetMain(sd->code,sd->code_length); // 这里会产生复制这个string，但我不希望产生这个。未来想办法改进

            return sd;
        }

        bool CustomVertexShader(ShaderCreateInfoVertex *vsc) override
        {
            for(auto &ua:mfd->vi)
                vsc->AddInput(ua.vat,ua.name);

            if(!Std3DMaterial::CustomVertexShader(vsc))
                return (false);

            return CommonProc(VK_SHADER_STAGE_VERTEX_BIT,vsc);
        }

        bool CustomGeometryShader(ShaderCreateInfoGeometry *gsc) override
        {
            material_file::GeometryShaderData *sd=(material_file::GeometryShaderData *)CommonProc(VK_SHADER_STAGE_GEOMETRY_BIT,gsc);

            if(!sd)
                return (false);

            gsc->SetGeom(sd->input_prim,sd->output_prim,sd->max_vertices);

            return true;
        }

        bool CustomFragmentShader(ShaderCreateInfoFragment *fsc) override
        {
            return CommonProc(VK_SHADER_STAGE_FRAGMENT_BIT,fsc);
        }
    }; // class Std3DMaterialLoader:public Std3DMaterial
} // namespace

material_file::MaterialFileData *LoadMaterialDataFromFile(const AnsiString &mtl_filename);

MaterialCreateInfo *LoadMaterialFromFile(const AnsiString &name,Material3DCreateConfig *cfg)
{
    if(name.IsEmpty()||!cfg)
        return (nullptr);

    material_file::MaterialFileData *mfd=LoadMaterialDataFromFile(name);

    if(!mfd)
        return nullptr;

    cfg->shader_stage_flag_bit=mfd->shader_stage_flag_bit;

    Std3DMaterialLoader mtl(mfd,cfg);

    return mtl.Create();
}
STD_MTL_NAMESPACE_END
