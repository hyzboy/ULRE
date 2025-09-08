#include<hgl/graph/VertexDataManager.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKVertexInputFormat.h>
#include<hgl/graph/VKVertexInputLayout.h>
#include<hgl/graph/VKDevice.h>

DEFINE_LOGGER_MODULE(VertexDataManager)

namespace hgl::graph
{
    VertexDataManager::VertexDataManager(VulkanDevice *dev,const VIL *_vil)
    {
        device=dev;

        vil=_vil;
        vi_count=_vil->GetVertexAttribCount();
        vif_list=_vil->GetVIFList();     //来自于Material，不会被释放，所以指针有效

        vab_max_size=0;
        vab_cur_size=0;
        vab=hgl_zero_new<VAB *>(vi_count);

        ibo_cur_size=0;
        ibo=nullptr;

        MLogVerbose(VertexDataManager, OS_TEXT("VertexDataManager constructed: vi_count=") + OSString::numberOf(vi_count) + OS_TEXT(", vif_list=") + OSString::numberOf((uintptr_t)vif_list));
    }

    VertexDataManager::~VertexDataManager()
    {
        MLogVerbose(VertexDataManager, OS_TEXT("VertexDataManager destroying: vab_cur_size=") + OSString::numberOf(vab_cur_size) + OS_TEXT(", ibo_cur_size=") + OSString::numberOf(ibo_cur_size));

        SAFE_CLEAR_OBJECT_ARRAY_OBJECT(vab,vi_count);
        SAFE_CLEAR(ibo);

        MLogVerbose(VertexDataManager, OS_TEXT("VertexDataManager destroyed."));
    }

    /**
    * 初始化顶点数据管理器
    * @param vbo_size VAB大小
    * @param ibo_size IBO大小
    * @param index_type 索引类型
    */
    bool VertexDataManager::Init(const VkDeviceSize vbo_size,const VkDeviceSize ibo_size,const IndexType index_type)
    {
        MLogInfo(VertexDataManager, OS_TEXT("Init called: vbo_size=") + OSString::numberOf(vbo_size) + OS_TEXT(", ibo_size=") + OSString::numberOf(ibo_size) + OS_TEXT(", index_type=") + OSString::numberOf((int)index_type));

        if(vab[0]||ibo)     //已经初始化过了
        {
            MLogWarning(VertexDataManager, OS_TEXT("Init skipped: already initialized."));
            return(false);
        }

        if(vbo_size<=0)
        {
            MLogError(VertexDataManager, OS_TEXT("Init failed: invalid vbo_size=") + OSString::numberOf(vbo_size));
            return(false);
        }

        vab_max_size=vbo_size;
        ibo_cur_size=ibo_size;

        vab_cur_size=0;
        ibo_cur_size=0;

        for(uint i=0;i<vi_count;i++)
        {
            MLogVerbose(VertexDataManager, OS_TEXT("Creating VAB[") + OSString::numberOf(i) + OS_TEXT("] format=") + OSString::numberOf((int)vif_list[i].format) + OS_TEXT(", size=") + OSString::numberOf(vab_max_size));

            vab[i]=device->CreateVAB(vif_list[i].format,vab_max_size);
            if(!vab[i])
            {
                MLogError(VertexDataManager, OS_TEXT("CreateVAB failed for index=") + OSString::numberOf(i));

                // cleanup any created VABs
                for(uint j=0;j<i;j++)
                {
                    if(vab[j]){ SAFE_CLEAR(vab[j]); }
                }

                return(false);
            }

            MLogVerbose(VertexDataManager, OS_TEXT("VAB[") + OSString::numberOf(i) + OS_TEXT("] created."));
        }

        vbo_data_chain.Init(vab_max_size);
        MLogVerbose(VertexDataManager, OS_TEXT("vbo_data_chain initialized with size=") + OSString::numberOf(vab_max_size));

        if(ibo_size>0)
        {
            ibo=device->CreateIBO(index_type,ibo_size);
            if(!ibo)
            {
                MLogError(VertexDataManager, OS_TEXT("CreateIBO failed: size=") + OSString::numberOf(ibo_size));

                // cleanup VABs
                for(uint i=0;i<vi_count;i++)
                    SAFE_CLEAR(vab[i]);

                return(false);
            }

            ibo_data_chain.Init(ibo_size);
            MLogInfo(VertexDataManager, OS_TEXT("IBO created and ibo_data_chain initialized with size=") + OSString::numberOf(ibo_size));
        }

        MLogVerbose(VertexDataManager, OS_TEXT("VertexDataManager Init success."));

        return(true);
    }

    DataChain::UserNode *VertexDataManager::AcquireIB(const VkDeviceSize count)
    {
        MLogVerbose(VertexDataManager, OS_TEXT("AcquireIB requested: count=") + OSString::numberOf(count));

        if(count<=0)
        {
            MLogWarning(VertexDataManager, OS_TEXT("AcquireIB: invalid count=") + OSString::numberOf(count));
            return(nullptr);
        }

        DataChain::UserNode *un=ibo_data_chain.Acquire(hgl_align<int>(count,4));

        if(!un)
        {
            MLogError(VertexDataManager, OS_TEXT("AcquireIB failed: no space for count=") + OSString::numberOf(count));
            return(nullptr);
        }

        ibo_cur_size+=un->GetCount();

        MLogVerbose(VertexDataManager, OS_TEXT("AcquireIB success: allocated=") + OSString::numberOf(un->GetCount()) + OS_TEXT(", ibo_cur_size=") + OSString::numberOf(ibo_cur_size));

        return(un);
    }

    bool VertexDataManager::ReleaseIB(DataChain::UserNode *un)
    {
        if(!un)
        {
            MLogWarning(VertexDataManager, OS_TEXT("ReleaseIB called with null node."));
            return(false);
        }

        const auto count=un->GetCount();

        if(!ibo_data_chain.Release(un))
        {
            MLogError(VertexDataManager, OS_TEXT("ReleaseIB failed for count=") + OSString::numberOf(count));
            return(false);
        }

        ibo_cur_size-=count;
        MLogVerbose(VertexDataManager, OS_TEXT("ReleaseIB success: released=") + OSString::numberOf(count) + OS_TEXT(", ibo_cur_size=") + OSString::numberOf(ibo_cur_size));
        return(true);
    }

    DataChain::UserNode *VertexDataManager::AcquireVAB(const VkDeviceSize count)
    {
        MLogVerbose(VertexDataManager, OS_TEXT("AcquireVAB requested: count=") + OSString::numberOf(count));

        if(count<=0)
        {
            MLogWarning(VertexDataManager, OS_TEXT("AcquireVAB: invalid count=") + OSString::numberOf(count));
            return(nullptr);
        }

        DataChain::UserNode *un=vbo_data_chain.Acquire(hgl_align<int>(count,4));

        if(!un)
        {
            MLogError(VertexDataManager, OS_TEXT("AcquireVAB failed: no space for count=") + OSString::numberOf(count));
            return(nullptr);
        }

        vab_cur_size+=un->GetCount();

        MLogVerbose(VertexDataManager, OS_TEXT("AcquireVAB success: allocated=") + OSString::numberOf(un->GetCount()) + OS_TEXT(", vab_cur_size=") + OSString::numberOf(vab_cur_size));

        return(un);
    }

    bool VertexDataManager::ReleaseVAB(DataChain::UserNode *un)
    {
        if(!un)
        {
            MLogWarning(VertexDataManager, OS_TEXT("ReleaseVAB called with null node."));
            return(false);
        }

        const auto count=un->GetCount();

        if(!vbo_data_chain.Release(un))
        {
            MLogError(VertexDataManager, OS_TEXT("ReleaseVAB failed for count=") + OSString::numberOf(count));
            return(false);
        }

        vab_cur_size-=count;
        MLogVerbose(VertexDataManager, OS_TEXT("ReleaseVAB success: released=") + OSString::numberOf(count) + OS_TEXT(", vab_cur_size=") + OSString::numberOf(vab_cur_size));
        return(true);
    }
}//namespace hgl::graph
