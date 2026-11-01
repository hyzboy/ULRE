#pragma once

/**
 * 环境配置**资产句柄**（`hgl::asset::AssetObject` 派生）。
 *
 * 解决的问题：一份环境（画什么天 + 用什么天光 + 曝光策略）原本只能写成示例里的
 * `SkyEnvironmentConfig` 局部变量 —— 数据只活在代码里，换一份环境就得改代码重编。
 * 环境资产把这段数据变成**有身份、可加载、可 diff** 的一份文件：
 *
 * ```
 * env:file:ShaderLibrary/asset/environment/sky34_cube_ibl.env   ← 自有二进制资产形态（FlatBuffers）
 * env:@Sky34.CubeMapIBL                                         ← 表内名字引用（资产里的 name =）
 * ```
 *
 * | 层 | 类型 | 回答的问题 |
 * |----|------|-----------|
 * | 授权层 | `EnvironmentSpec` | 文件里写了什么键（检查器 / diff / 文档） |
 * | 句柄层 | `EnvironmentAsset`（本类） | 这份环境叫什么、从哪来、合不合法 |
 *
 * GPU 侧没有对应物：环境资产只是**参数表的来源**，消费它的是
 * `ecs::EnvironmentSystem::SetupSkyEnvironment(uri)`（贴图装载 / provider 选择 / SH
 * 写入都在那里发生）。所以本类可以在没有图形设备的进程里安全存在（离线校验、工具）。
 *
 * 生命周期：句柄由 `EnvironmentAssetRegistry` 的表拥有并销毁。线程约定：主线程。
 *
 * 内置预设（`ShaderLibrary/asset/environment/*.env`）由**工具工程**
 * `src/Tools/BuiltinAssets`（可执行 `BuiltinAssetExport`）生成 —— 生成器不在运行时库里，
 * 本类只负责「字段表 + 序列化通路」，不定义任何内置数据。
 */

#include <hgl/asset/AssetObject.h>
#include <hgl/graph/asset/EnvironmentAssetSpec.h>
#include <hgl/type/String.h>

namespace hgl::graph
{
    /**
     * 一份环境配置的资产句柄。
     *
     * 解析失败**不会**产生句柄：环境的字段少一个就是「这份环境不能用」，不像数据块
     * 那样诊断信息本身有价值。失败原因经 `EnvironmentAssetRegistry` 的
     * `GetLastError()` 返回给调用方（`SetupSkyEnvironment(uri)` 会原样转进日志）。
     */
    class EnvironmentAsset final : public hgl::asset::AssetObject
    {
    private:

        EnvironmentSpec     spec;                   ///<授权形态（字段契约真源）

        OSString            source_path;            ///<来源文件（空 = 内存注册）
        U8String            resource_id;            ///<资产身份（`env:@<name>` / `env:file:<path>`）
        AnsiString          asset_name;             ///<`AssetObject` 契约要求的稳定名字

    public:

        EnvironmentAsset() = default;
        explicit EnvironmentAsset(const EnvironmentSpec &in_spec);
        explicit EnvironmentAsset(EnvironmentSpec &&in_spec);

        ~EnvironmentAsset() override = default;

        NO_COPY(EnvironmentAsset)
        NO_MOVE(EnvironmentAsset)

    public:     // AssetObject 契约

        const hgl::asset::ClassMetaInfo *GetClassMetaInfo() const override;

        const AnsiString &GetAssetName() const override { return asset_name; }

        /**
         * 字段遍历（读写同源）。实现只有一行 —— 真正的字段表由
         * `EnvironmentSerialize.gen.h` 里的 `StructSerializer<EnvironmentSpec>::Do()`
         * 提供，物理格式由 `ArchiveFile.h` 按文件名后缀在基类之外分派。
         */
        bool Serialize(hgl::serialize::Archive &ar) override;

    public:     // 只读观察

        const EnvironmentSpec &GetSpec() const { return spec; }
        EnvironmentSpec &GetSpec() { return spec; }
        void SetSpec(const EnvironmentSpec &in_spec);
        void SetSpec(EnvironmentSpec &&in_spec);

        const OSString &GetSourcePath() const { return source_path; }

        const U8String &GetResourceId() const { return resource_id; }

        /// `spec.name`（可能为空：只按路径引用的匿名环境）
        const AnsiString &GetEnvironmentName() const { return spec.name; }

    public:     // 装配

        /// 记录来源并生成身份（`label` 为路径或空）
        void SetSource(const os_char *label);
    };//class EnvironmentAsset
}//namespace hgl::graph
