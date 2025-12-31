#include<hgl/type/String.h>
#include<hgl/graph/VKPipelineData.h>
#include<hgl/graph/VKInlinePipeline.h>
#include<hgl/filesystem/FileSystem.h>
#include<iostream>

VK_NAMESPACE_BEGIN

    bool LoadFromFile(const OSString &filename,PipelineData *pd);
    bool SaveToFile(const OSString &filename,PipelineData *pd);

    std::string SavePipelineToToml(const PipelineData *data);
    bool LoadPipelineFromTomlFile(PipelineData *pd,const OSString &filename);

    bool Compare(const PipelineData *pd1,const PipelineData *pd2);

    void SaveToToml(const OSString &filename,const PipelineData &pd)
    {
        std::string str=SavePipelineToToml(&pd);

        if(hgl::filesystem::SaveMemoryToFile(filename,str.c_str(),str.length())>0)
        {
            os_out<<OS_TEXT("Save pipeline file: ")<<filename.c_str()<<OS_TEXT(" success!")<<std::endl;
        }
        else
        {
            os_err<<OS_TEXT("Save pipeline file: ")<<filename.c_str()<<OS_TEXT(" failed!")<<std::endl;
        }
    }

    void SaveInlinePipeline2Toml(const OSString &pathname)
    {
        if(!filesystem::IsDirectory(pathname))
        {
            os_err<<OS_TEXT("SaveInlinePipeline2Toml(")<<pathname.c_str()<<OS_TEXT(") failed, not a directory.")<<std::endl;
            return;
        }

    #define SAVE_PIPELINE_TO_FILE(name) SaveToToml(filesystem::JoinPathWithFilename(pathname,OS_TEXT(#name) OS_TEXT(".pipeline.toml")),GetPipelineData(InlinePipeline::name));

        SAVE_PIPELINE_TO_FILE(Solid3D)
        SAVE_PIPELINE_TO_FILE(Alpha3D)
        SAVE_PIPELINE_TO_FILE(Solid2D)
        SAVE_PIPELINE_TO_FILE(Alpha2D)
        SAVE_PIPELINE_TO_FILE(Sky)

    #undef SAVE_PIPELINE_TO_FILE
    }
VK_NAMESPACE_END

int os_main(int argc,os_char **argv)
{
    os_out<<OS_TEXT("PipelineCompiler v1.01 (offical web: ")<<HGL_OFFICAL_WEB_OS<<OS_TEXT(")")<<std::endl<<std::endl;

    if(argc<2)
    {
        std::cout<<"example: PipelineCompiler init"<<std::endl
                 <<"         the method should save all inline PipelineData to .toml files."<<std::endl<<std::endl;
            
        std::cout<<"example: PipelineCompiler [pipeline filename]"<<std::endl
                 <<"         the method should load the pipeline toml file, and to save a binary pipeline file."<<std::endl<<std::endl;

        return 0;
    }

    if(hgl::stricmp(argv[1],OS_TEXT("init"))==0)
    {
        hgl::OSString cur_path;

        hgl::filesystem::GetCurrentPath(cur_path);

        os_out<<OS_TEXT("Create Inline pipeline file at")<<cur_path.c_str()<<std::endl;

        VK_NAMESPACE::SaveInlinePipeline2Toml(cur_path);

        return 0;
    }

    const hgl::OSString toml_filename=argv[1];

    os_out<<OS_TEXT("pipeline filename: ")<<toml_filename.c_str()<<std::endl;

    VK_NAMESPACE::PipelineData pd;

    if(!LoadPipelineFromTomlFile(&pd,toml_filename))
    {
        os_err<<OS_TEXT("Load .toml pipeline file failed!")<<std::endl;
        return(-1);
    }

    const hgl::OSString ext_pipeline=OS_TEXT("pipeline");
    const hgl::OSString bin_filename=hgl::filesystem::ReplaceExtension(toml_filename,ext_pipeline,OS_TEXT('.'),false);

    os_out<<OS_TEXT("save pipeline file: ")<<bin_filename.c_str()<<std::endl;

    if(!VK_NAMESPACE::SaveToFile(bin_filename,&pd))
    {
        std::cerr<<"save failed!"<<std::endl;
        return -2;
    }

    std::cout<<"save ok!"<<std::endl;

    VK_NAMESPACE::PipelineData pd2;

    if(!VK_NAMESPACE::LoadFromFile(bin_filename,&pd2))
    {
        std::cerr<<"load failed!"<<std::endl;
        return -3;
    }

    if(VK_NAMESPACE::Compare(&pd,&pd2))
    {
        std::cout<<"compare ok!"<<std::endl;
    }
    else
    {
        std::cerr<<"compare failed!"<<std::endl;
        return -4;
    }

    return 0;
}
