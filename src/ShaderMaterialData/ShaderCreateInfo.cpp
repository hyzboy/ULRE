#include<hgl/mtl/ShaderCreateInfo.h>
#include<hgl/mtl/ShaderArtifactContract.h>
#include<hgl/mtl/ShaderCompilerSPVData.h>
#include<cstring>
namespace hgl{namespace graph::mtl{
ShaderCreateInfo::SPVReleaser ShaderCreateInfo::spv_releaser=nullptr;

ShaderCreateInfo::ShaderCreateInfo(const ShaderStage stage)
{
    shader_stage=stage;
    spv_data=nullptr;
}

ShaderCreateInfo::~ShaderCreateInfo()
{
    // 走回调而不是直连 FreeSPVData：SPV 内存归编译器插件，发行形态没有插件
    // 也就没有可释放的对象（此路径下 spv_data 恒为 null）。
    if(spv_data&&spv_releaser)
        spv_releaser(spv_data);
}

bool ShaderCreateInfo::SetCachedSPVData(const void *data,const size_t byte_size)
{
    uint32 magic=0;
    if(data&&byte_size>=sizeof(uint32))
        std::memcpy(&magic,data,sizeof(magic));
    if(!data
     ||byte_size<sizeof(uint32)
     ||byte_size%sizeof(uint32)!=0
     ||magic!=mtl::ShaderArtifactSPVMagic)
        return(false);

    if(spv_data&&spv_releaser)
    {
        spv_releaser(spv_data);
        spv_data=nullptr;
    }

    cached_spv_data.Resize(
        static_cast<int>(byte_size/sizeof(uint32)));
    std::memcpy(cached_spv_data.GetData(),data,byte_size);
    return(true);
}

bool ShaderCreateInfo::HasSPVData() const
{
    return spv_data || !cached_spv_data.IsEmpty();
}

const uint32 *ShaderCreateInfo::GetSPVData()const
{
    if(spv_data)
        return(spv_data->spv_data);

    return cached_spv_data.IsEmpty()
        ?nullptr
        :cached_spv_data.GetData();
}

const size_t ShaderCreateInfo::GetSPVSize()const
{
    return spv_data
        ?spv_data->spv_length
        :static_cast<size_t>(cached_spv_data.GetCount())*sizeof(uint32);
}
}}//namespace hgl::graph::mtl
