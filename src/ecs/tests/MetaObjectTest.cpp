#include<hgl/log/Log.h>
#include<hgl/log/Logger.h>
#include<hgl/object/Object.h>
#include<hgl/object/RefObject.h>
#include<hgl/object/MetaObject.h>

DEFINE_LOGGER_MODULE_WITH_LEVEL(MetaObjectTest,LogLevel::Info)

namespace
{
    /**
    * 纯Object派生：验证唯一ID、Init/Release幂等、类型链
    */
    class MockDeviceObject:public hgl::Object
    {
    public:

        HGL_OBJECT(MockDeviceObject,hgl::Object)

        int init_count=0;
        int release_count=0;

    protected:

        bool OnInit() override
        {
            ++init_count;
            return(true);
        }

        void OnRelease() override
        {
            ++release_count;
        }
    };//class MockDeviceObject

    /**
    * RefObject派生：验证RefPtr引用计数与AutoDestroy
    */
    class MockSharedBuffer:public hgl::RefObject
    {
    public:

        HGL_OBJECT(MockSharedBuffer,hgl::RefObject)

        static int alive_count;

    public:

        MockSharedBuffer()
        {
            ++alive_count;
        }

        ~MockSharedBuffer() override
        {
            --alive_count;
        }

    protected:

        bool OnInit() override
        {
            return(true);
        }

        void OnRelease() override
        {
        }
    };//class MockSharedBuffer

    int MockSharedBuffer::alive_count=0;

    /**
    * MetaObject派生：验证meta与负载分离
    */
    class MockTextureAsset:public hgl::MetaObject
    {
    public:

        HGL_OBJECT(MockTextureAsset,hgl::MetaObject)

        bool        fail_next_load=false;               ///<下一次加载强制失败
        int         load_count=0;
        int         unload_count=0;
        bool        payload_resident=false;             ///<模拟显存是否占用

        int         event_loaded=0;
        int         event_unloaded=0;
        int         event_load_failed=0;

    protected:

        bool OnInitMeta() override
        {
            return(true);
        }

        void OnReleaseMeta() override
        {
        }

        bool OnLoadPayload() override
        {
            ++load_count;

            if(fail_next_load)
            {
                fail_next_load=false;
                return(false);
            }

            payload_resident=true;
            return(true);
        }

        void OnUnloadPayload() override
        {
            ++unload_count;
            payload_resident=false;
        }

        void OnLifecycleEvent(const hgl::ObjectLifecycleEvent event) override
        {
            using namespace hgl;

            switch(event)
            {
                case ObjectLifecycleEvent::PayloadLoaded:       ++event_loaded;         break;
                case ObjectLifecycleEvent::PayloadUnloaded:     ++event_unloaded;       break;
                case ObjectLifecycleEvent::PayloadLoadFailed:   ++event_load_failed;    break;
                default:                                                                break;
            }
        }
    };//class MockTextureAsset
}//namespace

using namespace hgl;
using namespace hgl::logger;

int main()
{
    if(!InitLogger(OS_TEXT("MetaObjectTest")))
        return 1;

    // ---- Object基础语义 ----
    {
        MockDeviceObject a;
        MockDeviceObject b;

        if(a.GetObjectId()==InvalidObjectId||
           b.GetObjectId()==InvalidObjectId||
           a.GetObjectId()==b.GetObjectId())
            return 2;

        a.SetInstanceName(OS_TEXT("device-a"));

        if(!a.Init()||!a.IsInitialized()||a.init_count!=1||
           !a.Init()||a.init_count!=1)                          // 幂等
            return 3;

        if(a.GetTypeId()!=MockDeviceObject::StaticTypeInfo()->type_id||
           !a.IsA<MockDeviceObject>()||
           !a.IsA<hgl::Object>()||
           a.Cast<MockDeviceObject>()!=&a||
           a.GetInstanceName()!=OS_TEXT("device-a")||
           a.GetLogger().GetBoundObjectId()!=a.GetObjectId())
            return 4;

        a.Release();
        a.Release();

        if(!a.IsReleased()||a.release_count!=1)
            return 5;

        MLogInfo(MetaObjectTest,U8_TEXT("object semantics ok"));
    }

    // ---- RefObject引用计数 ----
    {
        const int base_alive=MockSharedBuffer::alive_count;

        {
            RefPtr<MockSharedBuffer> buf(new MockSharedBuffer());

            if(!buf||buf->GetRefCount()!=1)
                return 6;

            {
                RefPtr<MockSharedBuffer> copy=buf;

                if(buf->GetRefCount()!=2||copy.Get()!=buf.Get())
                    return 7;
            }

            if(buf->GetRefCount()!=1)
                return 8;

            MockSharedBuffer *raw=buf.Detach();

            if(buf||raw->GetRefCount()!=1)
                return 9;

            buf.Reset(raw);                                     // 重新接管：RefInc

            if(raw->GetRefCount()!=2)
                return 10;

            buf.Reset();                                        // 放弃引用，raw仍持有1

            if(buf||raw->GetRefCount()!=1)
                return 11;

            raw->RefDec();                                      // 归零 → AutoDestroy
        }

        // RefPtr析构 → 归零 → AutoDestroy
        if(MockSharedBuffer::alive_count!=base_alive)
            return 12;

        // 显式RefInc/RefDec
        MockSharedBuffer *manual=new MockSharedBuffer();

        manual->RefInc();

        if(manual->RefDec()!=0||MockSharedBuffer::alive_count!=base_alive)
            return 13;

        MLogInfo(MetaObjectTest,U8_TEXT("ref object ok"));
    }

    // ---- MetaObject：meta与负载两层分离 ----
    {
        MockTextureAsset *tex=new MockTextureAsset();

        if(tex->GetPayloadState()!=PayloadState::MetaOnly||
           tex->IsPayloadLoaded()||
           tex->GetRefZeroPolicy()!=RefZeroPolicy::KeepMetaUnloadPayload||
           tex->GetMeta().IsValid())
            return 13;

        tex->GetMeta().source_path=OS_TEXT("res/texture/a.png");
        tex->GetMeta().asset_type=OS_TEXT("texture");
        tex->GetMeta().source_size=4096;
        tex->GetMeta().source_hash=0x0123456789ABCDEFull;

        if(!tex->GetMeta().IsValid())
            return 14;

        if(!tex->Init()||!tex->IsInitialized())
            return 15;

        // MetaOnly → Resident
        if(!tex->LoadPayload()||
           tex->GetPayloadState()!=PayloadState::Resident||
           !tex->IsPayloadResident()||
           !tex->payload_resident||
           tex->load_count!=1||
           tex->event_loaded!=1)
            return 16;

        // 幂等
        if(!tex->LoadPayload()||tex->load_count!=1)
            return 17;

        // Resident → MetaOnly，meta保留
        tex->UnloadPayload();

        if(tex->GetPayloadState()!=PayloadState::MetaOnly||
           tex->payload_resident||
           tex->unload_count!=1||
           tex->event_unloaded!=1||
           !tex->GetMeta().IsValid()||
           tex->GetMeta().source_path!=OS_TEXT("res/texture/a.png"))
            return 18;

        // 再次加载：无需重新解析资源头
        if(!tex->LoadPayload()||tex->load_count!=2||tex->event_loaded!=2)
            return 19;

        tex->UnloadPayload();
        tex->UnloadPayload();                                   // 幂等

        if(tex->unload_count!=2)
            return 20;

        // 加载失败可重试
        tex->fail_next_load=true;

        if(tex->LoadPayload()||
           tex->GetPayloadState()!=PayloadState::LoadFailed||
           tex->event_load_failed!=1)
            return 21;

        if(!tex->LoadPayload()||tex->GetPayloadState()!=PayloadState::Resident)
            return 22;

        // ---- 引用归零：只卸载负载，保留meta壳 ----
        {
            RefPtr<MockTextureAsset> holder(tex);

            if(tex->GetRefCount()!=1)
                return 23;
        }

        if(tex->GetRefCount()!=0||
           tex->GetObjectState()!=ObjectState::Released||
           tex->GetPayloadState()!=PayloadState::Disposed||
           !tex->GetMeta().IsValid()||
           tex->payload_resident)
            return 24;

        // 壳仍在：可再次Init并使用meta
        if(!tex->Init()||!tex->LoadPayload()||!tex->IsPayloadResident())
            return 25;

        tex->Release();

        if(tex->payload_resident||tex->GetPayloadState()!=PayloadState::Disposed)
            return 26;

        delete tex;

        MLogInfo(MetaObjectTest,U8_TEXT("meta object ok"));
    }

    return 0;
}
