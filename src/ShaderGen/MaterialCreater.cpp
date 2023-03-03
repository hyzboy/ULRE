#include<hgl/graph/VK.h>
#include<hgl/type/Map.h>
#include<hgl/graph/VKShaderStage.h>
#include<hgl/graph/VKShaderDescriptor.h>
#include<hgl/graph/VKDescriptorSetType.h>
#include<hgl/shadergen/MaterialDescriptorManager.h>
#include<hgl/shadergen/ShaderDescriptorManager.h>
#include<hgl/graph/VKSamplerType.h>

using namespace hgl;
using namespace hgl::graph;

SHADERGEN_NAMESPACE_BEGIN
class ShaderCreater
{
    VkShaderStageFlagBits shader_stage;                      ///<着色器阶段

public:

    ShaderDescriptorManager sdm;

    VkShaderStageFlagBits GetShaderStage()const{return shader_stage;}

public:

    ShaderCreater(VkShaderStageFlagBits ss):sdm(ss)
    {
        shader_stage=ss;
    }

    ~ShaderCreater()
    {
    }
};

class ShaderCreaterMap:public ObjectMap<VkShaderStageFlagBits,ShaderCreater>
{
public:

    using ObjectMap<VkShaderStageFlagBits,ShaderCreater>::ObjectMap;

    bool Add(ShaderCreater *sc)
    {
        if(!sc)return(false);

        VkShaderStageFlagBits flag=sc->GetShaderStage();

        if(KeyExist(flag))
            return(false);

        ObjectMap<VkShaderStageFlagBits,ShaderCreater>::Add(flag,sc);
        return(true);
    }
};

class VertexShaderCreater:public ShaderCreater
{
    List<ShaderStage> input;

public:

    VertexShaderCreater():ShaderCreater(VK_SHADER_STAGE_VERTEX_BIT){}
    ~VertexShaderCreater()=default;

    int AddInput(const AnsiString &name,const VertexAttribType &type)
    {
        ShaderStage ss;

        hgl::strcpy(ss.name,sizeof(ss.name),name.c_str());

        ss.location=input.GetCount();
        ss.basetype=(uint8) type.basetype;
        ss.vec_size=        type.vec_size;

        return input.Add(ss);
    }
};

class GeometryShaderCreater:public ShaderCreater
{
public:

    GeometryShaderCreater():ShaderCreater(VK_SHADER_STAGE_GEOMETRY_BIT){}
    ~GeometryShaderCreater()=default;
};

class FragmentShaderCreater:public ShaderCreater
{
public:

    FragmentShaderCreater():ShaderCreater(VK_SHADER_STAGE_FRAGMENT_BIT){}
    ~FragmentShaderCreater()=default;
};

class MaterialCreater
{
    uint rt_count;                                          ///<输出的RT数量

    uint32_t shader_stage;                                  ///<着色器阶段

    MaterialDescriptorManager MDM;                          ///<材质描述符管理器

    ShaderCreaterMap shader_map;                            ///<着色器列表

    VertexShaderCreater *vert;
    GeometryShaderCreater *geom;
    FragmentShaderCreater *frag;

public:

    bool hasShader(const VkShaderStageFlagBits ss)const{return shader_stage&ss;}

    bool hasVertex  ()const{return hasShader(VK_SHADER_STAGE_VERTEX_BIT);}
//    bool hasTessCtrl()const{return hasShader(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);}
//    bool hasTessEval()const{return hasShader(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);}
    bool hasGeometry()const{return hasShader(VK_SHADER_STAGE_GEOMETRY_BIT);}
    bool hasFragment()const{return hasShader(VK_SHADER_STAGE_FRAGMENT_BIT);}
//    bool hasCompute ()const{return hasShader(VK_SHADER_STAGE_COMPUTE_BIT);}

public:

    MaterialCreater(const uint rc,const uint32 ss)
    {
        rt_count=rc;
        shader_stage=ss;

        if(hasVertex    ())shader_map.Add(vert=new VertexShaderCreater  );else vert=nullptr;
        if(hasGeometry  ())shader_map.Add(geom=new GeometryShaderCreater);else geom=nullptr;
        if(hasFragment  ())shader_map.Add(frag=new FragmentShaderCreater);else frag=nullptr;
    }

    ~MaterialCreater()
    {
    }

    int AddVertexInput(const AnsiString &name,const VertexAttribType &type)
    {
        if(!vert)return(-1);

        return vert->AddInput(name,type);
    }

    bool AddUBOCode(const AnsiString &ubo_typename,const AnsiString &codes)
    {
        if(ubo_typename.IsEmpty()||codes.IsEmpty())
            return(false);

        return MDM.AddUBOCode(ubo_typename,codes);
    }

    bool AddUBO(const VkShaderStageFlagBits flag_bits,const DescriptorSetType set_type,const AnsiString &type_name,const AnsiString &name)
    {
        if(!shader_map.KeyExist(flag_bits))
            return(false);

        if(!MDM.hasUBOCode(type_name))
            return(false);

        ShaderCreater *sc=shader_map[flag_bits];

        if(!sc)
            return(false);

        UBODescriptor *ubo=new UBODescriptor();

        ubo->type=type_name;
        ubo->name=name;

        return sc->sdm.AddUBO(set_type,MDM.AddUBO(flag_bits,set_type,ubo));
    }

    bool AddSampler(const VkShaderStageFlagBits flag_bits,const DescriptorSetType set_type,const SamplerType &st,const AnsiString &name)
    {
        if(!shader_map.KeyExist(flag_bits))
            return(false);

        RANGE_CHECK_RETURN_FALSE(st);

        ShaderCreater *sc=shader_map[flag_bits];

        if(!sc)
            return(false);

        SamplerDescriptor *sampler=new SamplerDescriptor();

        sampler->type=GetSamplerTypeName(st);
        sampler->name=name;

        return sc->sdm.AddSampler(set_type,MDM.AddSampler(flag_bits,set_type,sampler));
    }
};//class MaterialCreater

int main()
{
    MaterialCreater mc(1,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT);

    return 0;
}
SHADERGEN_NAMESPACE_END
