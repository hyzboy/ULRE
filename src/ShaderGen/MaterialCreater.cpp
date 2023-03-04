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

    int AddInput(const VAT &type,const AnsiString &name)
    {
        ShaderStage *ss=new ShaderStage;

        hgl::strcpy(ss->name,sizeof(ss->name),name.c_str());

        ss->basetype=(uint8) type.basetype;
        ss->vec_size=        type.vec_size;

        return sdm.AddInput(ss);
    }

    int AddInput(const AnsiString &type,const AnsiString &name)
    {
        VAT vat;

        if(!ParseVertexAttribType(&vat,type))
            return(-2);

        return AddInput(vat,name);
    }

    int AddOutput(const VAT &type,const AnsiString &name)
    {
        ShaderStage *ss=new ShaderStage;

        hgl::strcpy(ss->name,sizeof(ss->name),name.c_str());

        ss->basetype=(uint8) type.basetype;
        ss->vec_size=        type.vec_size;

        return sdm.AddOutput(ss);
    }

    int AddOutput(const AnsiString &type,const AnsiString &name)
    {
        VAT vat;

        if(!ParseVertexAttribType(&vat,type))
            return(-2);

        return AddOutput(vat,name);
    }

    bool AddFunction(const AnsiString &return_type,const AnsiString &func_name,const AnsiString &param_list,const AnsiString &codes)
    {
        //return sdm.AddFunction(return_type,func_name,param_list,code);
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
public:

    VertexShaderCreater():ShaderCreater(VK_SHADER_STAGE_VERTEX_BIT){}
    ~VertexShaderCreater()=default;
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

    VertexShaderCreater *   GetVS(){return vert;}
    GeometryShaderCreater * GetGS(){return geom;}
    FragmentShaderCreater * GetFS(){return frag;}

public:

    MaterialCreater(const uint rc,const uint32 ss=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT)
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

    bool AddUBOStruct(const AnsiString &ubo_typename,const AnsiString &codes)
    {
        if(ubo_typename.IsEmpty()||codes.IsEmpty())
            return(false);

        return MDM.AddUBOStruct(ubo_typename,codes);
    }

    bool AddUBO(const VkShaderStageFlagBits flag_bits,const DescriptorSetType set_type,const AnsiString &type_name,const AnsiString &name)
    {
        if(!shader_map.KeyExist(flag_bits))
            return(false);

        if(!MDM.hasUBOStruct(type_name))
            return(false);

        ShaderCreater *sc=shader_map[flag_bits];

        if(!sc)
            return(false);

        UBODescriptor *ubo=MDM.GetUBO(name);

        if(ubo)
        {
            if(ubo->type!=type_name)
                return(false);

            ubo->stage_flag|=flag_bits;
            
            return sc->sdm.AddUBO(set_type,ubo);
        }
        else
        {
            ubo=new UBODescriptor();

            ubo->type=type_name;
            ubo->name=name;

            return sc->sdm.AddUBO(set_type,MDM.AddUBO(flag_bits,set_type,ubo));
        }
    }

    bool AddSampler(const VkShaderStageFlagBits flag_bits,const DescriptorSetType set_type,const SamplerType &st,const AnsiString &name)
    {
        if(!shader_map.KeyExist(flag_bits))
            return(false);

        RANGE_CHECK_RETURN_FALSE(st);

        ShaderCreater *sc=shader_map[flag_bits];

        if(!sc)
            return(false);

        SamplerDescriptor *sampler=MDM.GetSampler(name);

        AnsiString st_name=GetSamplerTypeName(st);

        if(sampler)
        {
            if(sampler->type!=st_name)
                return(false);

            sampler->stage_flag|=flag_bits;

            return sc->sdm.AddSampler(set_type,sampler);
        }
        else
        {
            sampler=new SamplerDescriptor();

            sampler->type=st_name;
            sampler->name=name;

            return sc->sdm.AddSampler(set_type,MDM.AddSampler(flag_bits,set_type,sampler));
        }
    }
};//class MaterialCreater

#ifdef _DEBUG

bool PureColorMaterial()
{
    MaterialCreater mc(1);                                  //一个新材质，1个RT输出，默认使用Vertex/Fragment shader

    //vertex部分
    {
        VertexShaderCreater *vsc=mc.GetVS();                //获取vertex shader creater

        vsc->AddInput("vec3","Position");                   //添加一个vec3类型的position属性输入

        //以下代码会被合并成 vec3 GetPosition(/**/){return Position;}
        vsc->AddFunction("vec3","GetPosition","/**/","return Position;");
    }

    //添加一个名称为ColorMaterial的UBO定义,其内部有一个vec4 color的属性
    //该代码会被展开为
    /*
        struct ColorMaterial
        {
            vec4 color;
        };
     */
    mc.AddUBOStruct("ColorMaterial","vec4 color;");

    //添加一个UBO，该代码会被展开为
    /*
        layout(set=SET_PerMI,binding=?) uniform ColorMaterial mtl;
    */
    mc.AddUBO(  VK_SHADER_STAGE_FRAGMENT_BIT,               //这个UBO出现在fragment shader
                DescriptorSetType::PerMaterialInstance,     //它属于材质实例合集
                "ColorMaterial",                            //UBO名称为ColorMaterial
                "mtl");                                     //UBO变量名称为mtl

    //fragment部分
    {
        FragmentShaderCreater *fsc=mc.GetFS();              //获取fragment shader creater

        //以下代码会被合并成 vec4 GetColor(/**/){return mtl.color;}
        fsc->AddFunction("vec4","GetColor","/**/","return mtl.color;");

        fsc->AddOutput("vec4","Color");                     //添加一个vec4类型的color属性输出
        
    }

    return(false);
}

int MaterialCreaterTest()
{
        

    return 0;
}
#endif//_DEBUG
SHADERGEN_NAMESPACE_END
