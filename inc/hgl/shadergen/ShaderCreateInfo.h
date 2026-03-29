#pragma once

#include <hgl/common/VertexAttribDef.h>
#include <hgl/common/ShaderStageDef.h>
#include <hgl/common/InterpolationDef.h>
#include<hgl/log/Log.h>
#include<string>

namespace hgl{namespace graph
{
struct SPVData;

class MaterialDescriptorInfo;
class ShaderDescriptorInfo;

struct UBODescriptor;
struct SSBODescriptor;
struct TextureDescriptor;
struct TextureSamplerDescriptor;

class ShaderCreateInfo
{
    OBJECT_LOGGER

protected:

    ShaderStage shader_stage;                      ///<着色器阶段

    ShaderDescriptorInfo *sdi;                      ///<着色器描述符信息(owned)
    MaterialDescriptorInfo *descriptor_db;

    std::string final_shader;

    SPVData *spv_data;

protected:

    bool CompileToSPV();

public:

    ShaderDescriptorInfo *GetShaderDescriptorInfo(){return sdi;}
    const ShaderDescriptorInfo *GetShaderDescriptorInfo()const{return sdi;}
    const ShaderStage GetShaderStage()const{return shader_stage;}

public:

    ShaderCreateInfo(ShaderDescriptorInfo *sdi,MaterialDescriptorInfo *m);
    virtual ~ShaderCreateInfo();

    bool AddUBO(const UBODescriptor *sd);
    bool AddSSBO(const SSBODescriptor *sd);
    bool AddTexture(const TextureDescriptor *sd);
    bool AddTextureSampler(const TextureSamplerDescriptor *sd);

    void SetMaterialInstance(UBODescriptor *);
    void SetMaterialInstance(SSBODescriptor *);

    const std::string &GetFinalGLSL()const{return final_shader;}

    void SetFinalGLSL(const std::string &glsl){final_shader=glsl;}
    void SetFinalGLSL(const char *glsl){final_shader=glsl?glsl:"";}

    bool CompileFinalGLSLToSPV();                ///< 直接编译 final_shader 到 SPV

    const uint32 *GetSPVData()const;
    const size_t GetSPVSize()const;
};//class ShaderCreateInfo
}}//namespace hgl::graph
