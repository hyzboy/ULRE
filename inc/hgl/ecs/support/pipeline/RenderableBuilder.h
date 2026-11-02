#pragma once

#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/components/MaterialData.h>
#include <hgl/ecs/support/transform/TransformAccessor.h>
#include <hgl/graph/asset/PrimitiveAsset.h>
#include <hgl/graph/ssbo/GlobalSSBOTypes.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace hgl::ecs
{
    /**
     * Small authoring helper for an entity with ECS transform and render data.
     * The PrimitiveAsset remains externally owned and must outlive the entity.
     */
    class RenderableBuilder
    {
        ECSContext *world = nullptr;
        Entity *entity = nullptr;
        bool failed = false;
        bool built = false;
        bool has_transform = false;

        void Fail(const char *operation);

    public:
        RenderableBuilder(ECSContext *context, const char *name);
        ~RenderableBuilder();

        RenderableBuilder(const RenderableBuilder &) = delete;
        RenderableBuilder &operator=(const RenderableBuilder &) = delete;
        RenderableBuilder(RenderableBuilder &&) = delete;
        RenderableBuilder &operator=(RenderableBuilder &&) = delete;

        RenderableBuilder &WithTransform(
            const glm::vec3 &position,
            const glm::quat &rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
            const glm::vec3 &scale = glm::vec3(1.0f),
            Mobility mobility = Mobility::Movable);

        RenderableBuilder &WithPrimitiveAsset(
            const hgl::graph::PrimitiveAsset *asset,
            uint32_t variant_index = 0);

        /**
         * @brief 按 `*.primitive.toml` 路径装配（几何 + 配方一步到位）。
         *
         * 走 `GraphicsContext` 的图元资产注册器：同一个文件只装配一次，之后共享。
         * 装配出的几何如果是**文件来源**，此时还在 `MetaOnly`/`Loading`，
         * `PrimitiveState::CanRender` 的几何门禁会拦住该实体，直到流送就绪。
         *
         * 失败（文件/配方/几何任一解析失败）⇒ builder 进入失败态，`Build()` 返回 nullptr。
         */
        RenderableBuilder &WithPrimitiveAssetPath(
            const hgl::OSString &primitive_path,
            uint32_t variant_index = 0);

        /**
         * @brief 用「几何 `AssetURI` + 材质配方」一步装配，几何由注册器持有。
         *
         * 几何 URI 的全部写法见 `GeometryAssetRegistry::GetOrLoadURI`：
         * `geo:builtin:cube` / `builtin:cube` / `geo:file:res/geo/x.geo.toml` / 裸路径。
         *
         * 装配出的 `PrimitiveAsset` 由 `PrimitiveAssetRegistry` 持有并去重，实体只拿裸指针
         * —— 与 `WithPrimitiveAssetPath` 同一所有权模型，调用方**不要**自己 delete。
         *
         * 失败（URI 解析失败 / 几何加载失败 / 配方不在注册器里）⇒ builder 进入失败态。
         */
        RenderableBuilder &WithGeometry(
            const hgl::OSString &geometry_uri,
            const hgl::graph::mtl::MaterialRecipe &recipe,
            hgl::graph::PrimitiveType primitive_type = hgl::graph::PrimitiveType::Triangles);

        RenderableBuilder &WithRecipe(
            const hgl::graph::mtl::MaterialRecipe &recipe);

        RenderableBuilder &WithMaterialDataResource(
            const hgl::graph::GlobalSSBOBinding &binding);

        /**
         * @brief 按 `mtldata` URI 引用**数据块内的一行**（批量材质示例的行绑定入口）。
         *
         * 这是把「几百行 SSBO 数据」从 C++ 双循环里搬出来之后的落点：
         *
         * @code
         * builder.WithGeometry(u8"geo:builtin:sphere", recipe)
         *        .WithMaterialDataRow(u8"mtldata:file:res/material/pbr_grid.material_data.toml", i);
         * @endcode
         *
         * 语义与 `WithMaterialRow(accessor, data)` 的区别：那条是**自己造一行**（数据在
         * 调用方内存里，行号由调用方租约管理），这条是**引用别人造好的一行**（数据在
         * 数据块文件里，块由注册表拥有并整块上传）。同一块的同一行可以被任意多个实体
         * 共享，而块本身只上传一次。
         *
         * 配方与数据行互不干扰：`WithRecipe` 决定用哪个 shader 变体与配方覆盖，
         * `WithMaterialDataRow` 只提供 SSBO 行内容。
         *
         * 失败（URI/类型不符 / 无数据块注册表 / 行号越界 / 未上传）⇒ builder 进入失败态。
         */
        RenderableBuilder &WithMaterialDataRow(
            const hgl::OSString &block_uri,
            uint32_t row_index = 0);

        template<typename T>
        RenderableBuilder &WithMaterialRow(
            hgl::graph::GlobalSSBODataAccessor &accessor,
            const T &data)
        {
            if (failed || !entity)
                return *this;

            MaterialData *material_data = world->GetOrCreateMaterialData(entity);
            if (!material_data || !material_data->SetMaterialDataRow(accessor, data))
                Fail("WithMaterialRow");
            return *this;
        }

        Entity *Build();
    };
}
