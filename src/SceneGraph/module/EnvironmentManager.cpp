#include<hgl/graph/module/EnvironmentManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/ShaderBufferSources.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/vk/StructuredBufferAccessor.h>
#include<hgl/log/Log.h>

namespace hgl::graph
{
    EnvironmentManager::Profile *EnvironmentManager::FindProfile(EnvProfileID id) const
    {
        for (auto *p : profiles)
            if (p && p->id == id)
                return p;
        return nullptr;
    }

    EnvironmentManager::Profile *EnvironmentManager::FindProfile(const AnsiString &name) const
    {
        for (auto *p : profiles)
            if (p && p->name == name)
                return p;
        return nullptr;
    }

    bool EnvironmentManager::MaterializeSkyUBO(Profile *profile)
    {
        if (!profile || profile->sky_ubo)
            return profile && profile->sky_ubo;

        auto *gc = GetGraphicsContext();
        if (!gc)
            return false;

        auto *buffer_manager = gc->GetBufferManager();
        if (!buffer_manager)
            return false;

        GLogInfo(u8"[EnvironmentManager] MaterializeSkyUBO: %s", profile->name.c_str());

        AnsiString buf_name = "SkyUBO:";
        buf_name += profile->name;

        auto *buf = buffer_manager->CreateUBO(buf_name,
                                              StructuredBufferAccessor<SkyInfo>::GetSize());
        if (!buf)
        {
            GLogError("[EnvironmentManager] create sky UBO failed: %s", profile->name.c_str());
            return false;
        }

        buf->SetUpdateClass(BufferUpdateClass::Deferred);
        profile->sky_ubo = StructuredBufferAccessor<SkyInfo>::Create(buf, &mtl::SBS_SkyInfo, false);
        if (!profile->sky_ubo)
        {
            buffer_manager->Release(buf);
            GLogError("[EnvironmentManager] create sky accessor failed: %s", profile->name.c_str());
            return false;
        }

        // 注意：此 UBO 不在设备级 dirty 扫描 registry 内（仅 StagedBuffer 注册），
        // MarkDirty 只打标记不写数据；实际落盘必须走 Update()（内部 DeviceBuffer::Write，
        // 自动路由 staged/直写，参考 CameraSystem 的 camera_ubo 路径）。
        profile->sky_ubo->Update(profile->cpu.sky);    // 拷贝数据 + 置脏
        profile->sky_ubo->Update();                    // CommitInternal → gpu Write
        return true;
    }

    void EnvironmentManager::EnsureDefault()
    {
        if (FindProfile(kEnvProfileDefault))
            return;

        auto *gc = GetGraphicsContext();
        GLogInfo(u8"[EnvironmentManager] EnsureDefault: gc=%p bm=%p",
                 (void *)gc,
                 (void *)(gc ? gc->GetBufferManager() : nullptr));
        if (!gc || !gc->GetBufferManager())
            return;    // BufferManager 未就绪，推迟到下次访问

        auto *p = new Profile();
        p->id = kEnvProfileDefault;
        p->name = "default";
        p->cpu.sky.SetTime(10, 0, 0);
        profiles.push_back(p);

        // default 立即物化并标脏：任何 world 第一帧的设备级上传扫描即可拿到有效数据。
        MaterializeSkyUBO(p);

        GLogInfo(u8"[EnvironmentManager] default profile ready (sky UBO=%p)",
                 (void *)(p->sky_ubo ? p->sky_ubo->GetGPUBuffer() : nullptr));
    }

    EnvironmentManager::EnvironmentManager(GraphicsContext *gc)
        : GraphModuleInherit<EnvironmentManager, GraphModule>(gc, "EnvironmentManager")
    {
    }

    EnvironmentManager::~EnvironmentManager()
    {
        for (auto *p : profiles)
            delete p;
        profiles.clear();
    }

    void EnvironmentManager::OnGraphicsContextChanged(GraphicsContext *)
    {
        EnsureDefault();
    }

    void EnvironmentManager::Release()
    {
        auto *gc = GetGraphicsContext();
        auto *buffer_manager = gc ? gc->GetBufferManager() : nullptr;

        for (auto *p : profiles)
        {
            if (!p)
                continue;

            if (p->sky_ubo)
            {
                auto *buf = p->sky_ubo->ubo();
                delete p->sky_ubo;
                p->sky_ubo = nullptr;

                if (buffer_manager && buf)
                    buffer_manager->Release(buf);
            }
        }
    }

    EnvProfileID EnvironmentManager::Create(const AnsiString &name, const EnvironmentInfo &init_info)
    {
        if (auto *exist = FindProfile(name))
            return exist->id;

        auto *p = new Profile();
        p->id = next_id++;
        p->name = name;
        p->cpu = init_info;
        profiles.push_back(p);
        return p->id;
    }

    EnvProfileID EnvironmentManager::Find(const AnsiString &name) const
    {
        auto *p = FindProfile(name);
        return p ? p->id : kEnvProfileInvalid;
    }

    EnvironmentInfo *EnvironmentManager::Edit(EnvProfileID id)
    {
        auto *p = FindProfile(id);
        return p ? &p->cpu : nullptr;
    }

    const EnvironmentInfo *EnvironmentManager::Get(EnvProfileID id) const
    {
        auto *p = FindProfile(id);
        return p ? &p->cpu : nullptr;
    }

    void EnvironmentManager::MarkDirty(EnvProfileID id)
    {
        auto *p = FindProfile(id);
        if (!p)
            return;

        if (p->sky_ubo)
        {
            p->sky_ubo->Update(p->cpu.sky);    // 拷贝数据 + 置脏
            p->sky_ubo->Update();              // 立即写入 GPU（该 UBO 不走设备级扫描上传）
        }
        // 未物化时无需标记：MaterializeSkyUBO 会用当前 cpu 数据初始化
    }

    void EnvironmentManager::CommitMaterialized()
    {
        // 视图三件套契约：pass 开始固定写入。sky UBO 是 host-visible 映射直写，
        // 每个已物化 profile 全量写一次（通常只有 default，几十字节，代价可忽略）
        for (auto *p : profiles)
        {
            if (!p || !p->sky_ubo)
                continue;

            p->sky_ubo->Update(p->cpu.sky);    // 拷贝数据 + 置脏
            p->sky_ubo->Update();              // 写入 GPU
        }
    }

    const IGPUBuffer *EnvironmentManager::GetSkyUBO(EnvProfileID id)
    {
        EnsureDefault();

        Profile *p = FindProfile(id);
        if (!p)
            p = FindProfile(kEnvProfileDefault);
        if (!p)
            return nullptr;

        if (!MaterializeSkyUBO(p))
            return nullptr;

        return p->sky_ubo->GetGPUBuffer();
    }
}//namespace hgl::graph
