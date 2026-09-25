#include<hgl/graph/module/EnvironmentManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/ShaderBufferSources.h>
#include<hgl/vk/buffer/DeviceBuffer.h>
#include<hgl/vk/buffer/StructView.h>
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
                                              StructView<SkyInfo>::GetSize());
        if (!buf)
        {
            GLogError("[EnvironmentManager] create sky UBO failed: %s", profile->name.c_str());
            return false;
        }

        buf->SetUpdateClass(BufferUpdateClass::Deferred);
        profile->sky_ubo = StructView<SkyInfo>::Create(buf, false);
        if (!profile->sky_ubo)
        {
            buffer_manager->Release(buf);
            GLogError("[EnvironmentManager] create sky accessor failed: %s", profile->name.c_str());
            return false;
        }

        // 注意：此 UBO 不在设备级 dirty 扫描 registry 内（仅 StagedBuffer 注册），
        // 数据写进映射窗口即可；Commit() 只把窗口范围标脏交 L2
        // （staged 由 RenderBufferUploadSystem 上传，直写缓冲立刻可见）。
        profile->sky_ubo->Update(profile->cpu.sky);    // 拷贝数据 + 置脏
        profile->sky_ubo->Commit();                    // 标脏交 L2
        return true;
    }

    void EnvironmentManager::ReleaseShadowRing(Profile *profile)
    {
        if (!profile)
            return;

        auto *gc = GetGraphicsContext();
        auto *buffer_manager = gc ? gc->GetBufferManager() : nullptr;

        for (uint32_t i = 0; i < kShadowUboRing; ++i)
        {
            if (!profile->shadow_ring[i])
                continue;

            auto *buf = profile->shadow_ring[i]->GetBuffer();
            delete profile->shadow_ring[i];
            profile->shadow_ring[i] = nullptr;

            if (buffer_manager && buf)
                buffer_manager->Release(buf);
        }
    }

    bool EnvironmentManager::MaterializeShadowUBO(Profile *profile)
    {
        if (!profile || profile->shadow_ring[0])
            return profile && profile->shadow_ring[0];

        auto *gc = GetGraphicsContext();
        if (!gc)
            return false;

        auto *buffer_manager = gc->GetBufferManager();
        if (!buffer_manager)
            return false;

        GLogInfo(u8"[EnvironmentManager] MaterializeShadowUBO: %s ring=%u",
                 profile->name.c_str(), kShadowUboRing);

        for (uint32_t i = 0; i < kShadowUboRing; ++i)
        {
            AnsiString buf_name = "ShadowUBO:";
            buf_name += profile->name;
            buf_name += ":";
            buf_name += AnsiString::numberOf(i);

            auto *buf = buffer_manager->CreateUBO(buf_name,
                                                  StructView<ShadowInfo>::GetSize());
            if (!buf)
            {
                GLogError("[EnvironmentManager] create shadow UBO failed: %s slot=%u",
                          profile->name.c_str(), i);
                ReleaseShadowRing(profile);
                return false;
            }

            buf->SetUpdateClass(BufferUpdateClass::Deferred);
            profile->shadow_ring[i] = StructView<ShadowInfo>::Create(buf, false);
            if (!profile->shadow_ring[i])
            {
                buffer_manager->Release(buf);
                GLogError("[EnvironmentManager] create shadow accessor failed: %s slot=%u",
                          profile->name.c_str(), i);
                ReleaseShadowRing(profile);
                return false;
            }

            // 每槽一份初始数据。之后只写 acquire 完成的那一槽，避免踩在途帧。
            profile->shadow_ring[i]->Update(profile->cpu.shadow);
            profile->shadow_ring[i]->Commit();
        }
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
        MaterializeShadowUBO(p);

        GLogInfo(u8"[EnvironmentManager] default profile ready (sky UBO=%p, shadow ring0=%p)",
                 (void *)(p->sky_ubo ? p->sky_ubo->GetGPUBuffer() : nullptr),
                 (void *)(p->shadow_ring[0] ? p->shadow_ring[0]->GetGPUBuffer() : nullptr));
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
                auto *buf = p->sky_ubo->GetBuffer();
                delete p->sky_ubo;
                p->sky_ubo = nullptr;

                if (buffer_manager && buf)
                    buffer_manager->Release(buf);
            }

            ReleaseShadowRing(p);
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
            p->sky_ubo->Commit();              // 标脏交 L2
        }
        // shadow 不在这里写 GPU。调用点在 Tick（阴影 pass 之前），交换链上仍有
        // 1~2 帧在飞读着同一块 ShadowInfo。写 CPU 即可，GPU 槽由 CommitMaterialized
        // 在 acquire 之后写入。
    }

    void EnvironmentManager::CommitMaterialized(uint32_t shadow_frame_index, bool commit_shadow)
    {
        if (shadow_frame_index >= kShadowUboRing)
            shadow_frame_index %= kShadowUboRing;

        for (auto *p : profiles)
        {
            if (!p)
                continue;

            if (p->sky_ubo)
            {
                p->sky_ubo->Update(p->cpu.sky);
                p->sky_ubo->Commit();
            }
            if (commit_shadow && p->shadow_ring[shadow_frame_index])
            {
                p->shadow_ring[shadow_frame_index]->Update(p->cpu.shadow);
                p->shadow_ring[shadow_frame_index]->Commit();
            }
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

    const IGPUBuffer *EnvironmentManager::GetShadowUBO(EnvProfileID id, uint32_t frame_index)
    {
        EnsureDefault();

        Profile *p = FindProfile(id);
        if (!p)
            p = FindProfile(kEnvProfileDefault);
        if (!p)
            return nullptr;

        if (!MaterializeShadowUBO(p))
            return nullptr;

        if (frame_index >= kShadowUboRing)
            frame_index %= kShadowUboRing;

        return p->shadow_ring[frame_index] ? p->shadow_ring[frame_index]->GetGPUBuffer() : nullptr;
    }
}//namespace hgl::graph
