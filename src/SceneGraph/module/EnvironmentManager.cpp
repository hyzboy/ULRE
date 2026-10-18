#include<hgl/graph/module/EnvironmentManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/GlobalSSBOBufferRegistry.h>
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

    /// 物化 profile 的**一个参数段**到 SSBO。
    ///
    /// 必须用 CreateSSBO：只有它带 SHADER_DEVICE_ADDRESS usage（BDA 取址前提）；
    /// 数据经**世界表**解引用（addr_ambient_env），不吃 Scene 集绑定。
    /// 段不在设备级 dirty 扫描 registry 内（仅 StagedBuffer 注册）：数据写进映射窗口即可，
    /// Commit() 只把窗口范围标脏交 L2（staged 由 RenderBufferUploadSystem 上传）。
    ///
    /// 地址不在这里入表（世界私有字段，`ECSContext::SyncWorldAddresses` 写本世界槽），
    /// 但物化处做一次取址 fail-fast：取不到地址 = shader 解引用 0 基址（UB），校验层抓不到这类崩。
    template<typename T>
    static bool MaterializeProfileSegment(GraphicsContext *gc,
                                          EnvironmentManager::Profile *profile,
                                          StructView<T> *&segment,
                                          const T &init,
                                          const char *seg_name)
    {
        if (!profile)
            return false;

        if (segment)
            return true;

        if (!gc)
            return false;

        auto *buffer_manager = gc->GetBufferManager();
        if (!buffer_manager)
            return false;

        GLogInfo(u8"[EnvironmentManager] Materialize%s: %s", seg_name, profile->name.c_str());

        AnsiString buf_name = seg_name;
        buf_name += ":";
        buf_name += profile->name;

        auto *buf = buffer_manager->CreateSSBO(buf_name, StructView<T>::GetSize());
        if (!buf)
        {
            GLogError("[EnvironmentManager] create %s UBO failed: %s", seg_name, profile->name.c_str());
            return false;
        }

        buf->SetUpdateClass(BufferUpdateClass::Deferred);
        segment = StructView<T>::Create(buf, false);
        if (!segment)
        {
            buffer_manager->Release(buf);
            GLogError("[EnvironmentManager] create %s accessor failed: %s", seg_name, profile->name.c_str());
            return false;
        }

        segment->Update(init);      // 拷贝数据 + 置脏
        segment->Commit();          // 标脏交 L2

        if (gc->GetDevice()->GetBufferDeviceAddressAligned16(buf->GetBuffer()) == 0)
        {
            GLogError("[EnvironmentManager] %s 取不到设备地址（usage / 16B 对齐）：%s",
                      seg_name, profile->name.c_str());
            buffer_manager->Release(buf);
            delete segment;
            segment = nullptr;
            return false;
        }

        return true;
    }

    bool EnvironmentManager::MaterializeAmbientEnvUBO(Profile *profile)
    {
        return MaterializeProfileSegment(GetGraphicsContext(), profile,
                                         profile->ambient_env_ubo, profile->cpu.ambient_env, "AmbientEnvUBO");
    }

    /// 释放一个参数段（模块析构 / 归还 profile 共用；两个段同形）。
    template<typename T>
    static void ReleaseProfileSegment(StructView<T> *&segment, BufferManager *buffer_manager)
    {
        if (!segment)
            return;

        auto *buf = segment->GetBuffer();
        delete segment;
        segment = nullptr;

        if (buffer_manager && buf)
            buffer_manager->Release(buf);
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
        // T6b：这里不再调 `sky.SetTime(10,0,0)` —— 天空已不含光源参数（光晕色也不再是字段，
        // shader 直接读主光色）；默认 profile 的天空数据（底色 / 光晕强度 / IBL 句柄）与旧值逐字段相同，
        // 主光走世界私有光源行。
        profiles.push_back(p);

        // default 立即物化并标脏：任何 world 第一帧的设备级上传扫描即可拿到有效数据。
        MaterializeAmbientEnvUBO(p);

        GLogInfo(u8"[EnvironmentManager] default profile ready (ambient_env UBO=%p)",
                 (void *)(p->ambient_env_ubo ? p->ambient_env_ubo->GetGPUBuffer() : nullptr));
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

            ReleaseProfileSegment(p->ambient_env_ubo, buffer_manager);
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

        if (p->ambient_env_ubo)
        {
            p->ambient_env_ubo->Update(p->cpu.ambient_env);
            p->ambient_env_ubo->Commit();
        }
        // 影子数据不在本模块（T6b/P4：随世界，见 ECSContext::CommitShadowInfo）。
    }

    bool EnvironmentManager::SetAmbientProvider(EnvProfileID id, const AnsiString &module_name)
    {
        auto *p = FindProfile(id);
        if (!p)
        {
            GLogWarning("[EnvironmentManager] SetAmbientProvider: 无效 profile 句柄 %u", id);
            return false;
        }

        if (p->ambient_module_name == module_name)
            return true;

        GLogInfo(u8"[EnvironmentManager] ambient provider: profile=%s %s -> %s",
                 p->name.c_str(),
                 p->ambient_module_name.IsEmpty() ? "(template default)"
                                                  : p->ambient_module_name.c_str(),
                 module_name.IsEmpty() ? "(template default)" : module_name.c_str());

        p->ambient_module_name = module_name;
        return true;
    }

    const AnsiString &EnvironmentManager::GetAmbientProvider(EnvProfileID id) const
    {
        static const AnsiString empty;

        auto *p = FindProfile(id);
        if (!p)
            p = FindProfile(kEnvProfileDefault);      // 无效句柄回退 default（与 Get 同语义）

        return p ? p->ambient_module_name : empty;
    }

    uint64_t EnvironmentManager::GetAmbientEnvAddress(EnvProfileID id)
    {
        EnsureDefault();

        Profile *p = FindProfile(id);
        if (!p)
            p = FindProfile(kEnvProfileDefault);      // 无效句柄回退 default（与 Get 同语义）
        if (!p || !MaterializeAmbientEnvUBO(p))
            return 0;

        auto *gc = GetGraphicsContext();
        auto *device = gc ? gc->GetDevice() : nullptr;
        if (!device)
            return 0;

        const uint64_t addr = device->GetBufferDeviceAddressAligned16(p->ambient_env_ubo->GetBuffer()->GetBuffer());
        if (addr == 0)
        {
            // 0 地址 = 读该段的 shader 解引用 0 基址（UB）⇒ 调用方必须保证不写进表
            GLogError("[EnvironmentManager] GetAmbientEnvAddress 取不到设备地址: %s", p->name.c_str());
        }
        return addr;
    }

    void EnvironmentManager::Release(EnvProfileID id)
    {
        // 内置 default 可共享（任何世界都能用它），**不随任何世界销毁**；无效句柄 no-op。
        if (id == kEnvProfileInvalid || id == kEnvProfileDefault)
            return;

        for (auto it = profiles.begin(); it != profiles.end(); ++it)
        {
            Profile *p = *it;
            if (!p || p->id != id)
                continue;

            auto *gc = GetGraphicsContext();
            auto *buffer_manager = gc ? gc->GetBufferManager() : nullptr;

            ReleaseProfileSegment(p->ambient_env_ubo, buffer_manager);

            GLogInfo(u8"[EnvironmentManager] Release profile: %s (id=%u)",
                     p->name.c_str(), p->id);

            delete p;
            profiles.erase(it);
            return;
        }
    }
}//namespace hgl::graph
