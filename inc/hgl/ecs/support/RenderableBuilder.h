#pragma once

#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/components/MaterialData.h>
#include <hgl/ecs/support/TransformAccessor.h>
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

        RenderableBuilder &WithRecipe(
            const hgl::graph::mtl::MaterialRecipe &recipe);

        RenderableBuilder &WithMaterialDataResource(
            const hgl::graph::GlobalSSBOBinding &binding);

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
