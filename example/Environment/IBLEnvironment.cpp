#include<hgl/framework/WorkManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include<hgl/graph/asset/GeometryPreset.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/graph/module/TextureManager.h>
#include<hgl/graph/module/GeometryManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/ssbo/GlobalSSBOTypes.h>
#include<hgl/graph/ssbo/MaterialDataRows.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/vk/VKBindlessTextureManager.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/graph/ssbo/LitMaterialData.h>

#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/ecs/components/GeometryData.h>
#include<hgl/ecs/components/CameraControlMode.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/ecs/systems/render/LightSystem.h>

#include<hgl/graph/gizmo/SunDirectionControlSystem.h>

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<glm/gtx/quaternion.hpp>
#include<hgl/vk/VKRenderTargetSwapchain.h>
#include "../Shared/FrameDigest.h"   // 帧摘要自检（T0 判据前置共用件）

#include<vector>
#include<memory>
#include<string>

using namespace hgl;
using namespace hgl::graph;
using namespace hgl::ecs;

namespace
{
    // 材质数据行的唯一来源：一个 URI + 一个行号，材质数值全部在块文件里
    constexpr char   MATERIAL_DATA_URI[] =
        "mtldata:file:ShaderLibrary/asset/material_data/ibl_environment.material_data.toml";
    constexpr uint32 kMeshRowCount = 5;   // floor / sphere / cube / torus / arrow
}

namespace
{
    GeometryVertexFormat CreateSkyCubeGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
        };
        return gvf;
    }

    GeometryVertexFormat CreateStandardGeometryVertexFormat()
    {
        GeometryVertexFormat gvf{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2F},
            {VertexSemantic::Normal,   VF_V3F},
        };
        return gvf;
    }

    /**
     * `IBLENV_OCT=1`：改用八面体展开（`.TexOct`）环境图 + `ambient_light_ibl_oct`
     * provider。
     *
     * 两条路径的 split-sum 公式、`lod = roughness × (mip-1)` 映射、LDR 曝光调制
     * **完全同一套**，唯一差别是纹理寻址（硬件立方体 vs 方向→八面体 uv）。
     * 因此这个开关同时是 `ambient_light_ibl_oct` 模块的现场判据：同一场景同一组
     * 材质参数，两条路径的亮度/颜色必须高度接近。
     */
    bool UseOctahedralEnv()
    {
        static const bool s_oct = (getenv("IBLENV_OCT") != nullptr);
        return s_oct;
    }

    /**
     * `IBLENV_SH=1`：改用 SH9 球谐天光 + `ambient_light_sh` provider。
     *
     * 第三条路径的数据源与前两条**完全不同**：不读任何贴图、不注册 bindless 句柄，
     * 只把 9 个 vec4 系数写进 `ambient_env.sh`。这是 `ambient_light_sh` 模块的现场判据
     * ——它证明「零烘焙 / 零贴图」的间接光路径能真的出图，且方向性（上亮下暗）生效。
     */
    bool UseSHAmbient()
    {
        static const bool s_sh = (getenv("IBLENV_SH") != nullptr);
        return s_sh;
    }

    /**
     * `IBLENV_SKY_OCT=1`：天空**背景**换用八面体展开图（`IBLEnvironmentSkyOct.Sky`）。
     *
     * 与 `IBLENV_OCT` 是两件独立的事：那个换的是环境 IBL（天光辐照/预过滤）的贴图
     * 形态，这个换的是背景天空球的贴图形态。两者都走 `texture_projection_*`，
     * 区别仅在消费端 —— 前者是 `ambient_light_ibl*`，后者是 `surface_sky_cube`。
     */
    bool UseSkyOct()
    {
        static const bool s_sky_oct = (getenv("IBLENV_SKY_OCT") != nullptr);
        return s_sky_oct;
    }

    /**
     * 天空梯度 L(ω) = c0 + c1·(ω·ẑ) → SH9 系数。
     *
     * 这样填充的意义：GPU 端 `EvalSHAnalyticIrradiance` 求出的正好是
     * `∫ L(ω)·max(n·ω,0) dω = π·c0 + (2π/3)·c1·(n·ẑ)`，即**解析真值**而非随手凑的数
     * ⇒ 可与 IBL 版的辐照度亮度直接对比（差太远就说明系数或下标约定错了）。
     * 系数写入位置严格按 `AmbientEnvInfo::sh` 的下标约定（Z-up：idx0 常量、idx2 ∝ z）。
     */
    void FillSkyGradientSH(AmbientEnvInfo &info)
    {
        constexpr float PI = 3.14159265358979323846f;
        constexpr float kConvolveL0 = 0.886227f;    // ambient_light_sh 中 l=0 的余弦卷积常数
        constexpr float kConvolveL1 = 1.023327f;    // ambient_light_sh 中 l=1 的余弦卷积常数

        const math::Vector3f c0(0.60f, 0.65f, 0.75f);   // 上下平均辐亮度（偏冷的天空色）
        const math::Vector3f c1(0.25f, 0.28f, 0.34f);   // 垂直梯度；c1 ≤ c0 才能保证地平线下非负

        const math::Vector3f A = c0 * (PI / kConvolveL0);
        const math::Vector3f B = c1 * ((2.0f * PI / 3.0f) / kConvolveL1);

        for (int i = 0; i < 9; ++i)
            info.sh[i] = math::Vector4f(0.0f);

        info.sh[0] = math::Vector4f(A.x, A.y, A.z, 0.0f);
        info.sh[2] = math::Vector4f(B.x, B.y, B.z, 0.0f);
    }
}

#define DRAW_SKY_SPHERE
#define DRAW_GIZMO

/**
 * IBL 环境光照示例:
 *
 * 以 sky_34 全景环境(cubemap)为天空背景,对场景物体做基于图像的光照(split-sum)。
 * IBL 资源由 TexConv 转换产出:
 *   * <env>_irradiance.TexCube  漫反射辐照度(按世界法线采样)
 *   * <env>_prefilter.TexCube   镜面预滤波(按反射向量采样)
 *   * brdflut.Tex2D             BRDF LUT((NdotV,roughness) → scale/bias)
 *
 * 加 `IBLENV_OCT=1` 则前两张换成 TexConv `/octa /ibl` 产出的八面体展开版
 * (`<env>_irradiance.TexOct` / `<env>_prefilter.TexOct`),ambient provider 相应切到
 * `ambient_light_ibl_oct`——光照公式/粗糙度映射/LDR 曝光全不变,只换纹理寻址。
 *
 * 加 `IBLENV_SH=1` 则**一张贴图都不读**：ambient provider 切到 `ambient_light_sh`，
 * 只往 `ambient_env.sh[9]` 填一组解析天空梯度系数 ⇒ 用来验证「零烘焙、零贴图」的
 * 低频天光路径确实能出图（而不是退化成黑球）。
 *
 * 三张纹理以 bindless 句柄形式写入 ambient_env(ambient_light_provider 槽参数块,场景全局环境数据),
 * 并把该槽的 **ambient provider 模块**设为 `indirect_ibl`
 * (EnvironmentSystem::SetAmbientProvider —— 环境光算法由环境层决定,不再由材质声明)。
 * 场内物体是不同金属度/粗糙度的 PBR 材质("lit"),便于对比 IBL 在不同表面上的表现。
 */
class IBLEnvironmentApp : public WorkObject
{
private:

    struct MeshEntry
    {
        Geometry* geometry = nullptr;
        // Geometry 由 GeometryAssetRegistry 持有，本结构只做观察（勿在此 delete）
        PrimitiveAsset asset{};
    };

    ECSContext* ecs_context = nullptr;
    Entity* camera_entity = nullptr;

#ifdef DRAW_SKY_SPHERE
    Entity* sky_entity = nullptr;
    std::shared_ptr<EnvironmentSystem> environment_system;   // 天空外观（sky_provider）+ IBL 句柄（ambient_light_provider）
    /// T6b：太阳方向写进**世界私有光源行 0**（sky 已不含光源参数）。
    std::shared_ptr<LightSystem> light_system;
    Geometry* sky_geometry = nullptr;
    graph::mtl::MaterialRecipe sky_recipe{};
    PrimitiveAsset             sky_asset{};
#endif//DRAW_SKY_SPHERE

#ifdef DRAW_GIZMO
    std::shared_ptr<SunDirectionControlSystem> sun_gizmo_system;
#endif//DRAW_GIZMO

    graph::mtl::MaterialRecipe mesh_recipe{};
    /// 每个物体一行 PBR 参数；行数固定，内容在块文件里
    graph::GlobalSSBOBinding mesh_rows[kMeshRowCount]{};
    VertexDataManager* mesh_vdm = nullptr;

    MeshEntry* floor_mesh = nullptr;

    Texture*     env_irradiance  = nullptr;                 ///<IBL 漫反射辐照度（CubeMap 或八面体展开）
    Texture*     env_prefilter   = nullptr;                 ///<IBL 镜面预滤波（同上）
    Texture2D*   env_brdf_lut    = nullptr;                 ///<BRDF LUT

    std::vector<std::unique_ptr<MeshEntry>> meshes;

private:

    bool InitEnvironmentControl()
    {
        if (!ecs_context)
            return false;

    #ifdef DRAW_SKY_SPHERE
        environment_system = ecs_context->GetSystem<EnvironmentSystem>();
        if (!environment_system)
            environment_system = ecs_context->RegisterSystem<EnvironmentSystem>();

        if (!environment_system)
            return false;

        light_system = ecs_context->GetSystem<LightSystem>();
        if (!light_system)
            light_system = ecs_context->RegisterSystem<LightSystem>();
        if (!light_system)
            return false;

        light_system->SetMainLightDirection(math::Vector3f(0.2f, 0.7f, 0.68f));
            #endif//DRAW_SKY_SPHERE

    #ifdef DRAW_GIZMO
        sun_gizmo_system = ecs_context->GetSystem<SunDirectionControlSystem>();
        if (!sun_gizmo_system)
            sun_gizmo_system = ecs_context->RegisterSystem<SunDirectionControlSystem>();

        if (!sun_gizmo_system)
            return false;

        sun_gizmo_system->SetLightSystem(light_system.get());
        sun_gizmo_system->SetGizmoPosition(math::Vector3f(0.0f, 0.0f, 0.0f));
    #endif//DRAW_GIZMO

        return true;
    }

#ifdef DRAW_SKY_SPHERE
    bool InitSkySphereResource()
    {
        if (!ecs_context)
            return false;

        // 天空球的立方体贴图由配方文件的 `[[textures]] asset = "cubefile:..."` 声明；
        // 下面的 IBL 三张图是 bindless 环境贴图（不是材质纹理），仍由示例加载。
        const GeometryVertexFormat sky_format = CreateSkyCubeGeometryVertexFormat();
        sky_geometry = GetGeometry("geo:builtin:hex_sphere_sky", &sky_format);
        if (!sky_geometry)
            return false;

        // 天空贴图有两种投影配方，**只差 `[[textures]]` 的输入文件**：
        //   · `IBLEnvironment.Sky`      → `.TexCube`（立方体贴图）
        //   · `IBLEnvironmentSkyOct.Sky`→ `.TexOct`（八面体展开图，2D）
        // 领域、表面模块、槽名、主着色器全都相同：投影实现由 C++ 侧按资源后缀选，
        // 天空程序自己不认识格式。`IBLENV_SKY_OCT=1` 只是切到八面体那一份。
        const char *sky_recipe_name = UseSkyOct()
            ? "IBLEnvironmentSkyOct.Sky"
            : "IBLEnvironment.Sky";

        const mtl::MaterialRecipe *loaded_sky_recipe = GetRecipe(sky_recipe_name);
        if (!loaded_sky_recipe)
            return false;
        sky_recipe = *loaded_sky_recipe;

        sky_asset = PrimitiveAsset(sky_geometry, &sky_recipe, PrimitiveType::Triangles);

        return true;
    }
#endif//DRAW_SKY_SPHERE

    bool InitMaterial()
    {
        auto* texture_manager = GetManager<TextureManager>();
        if (!texture_manager)
            return false;

        // ---- IBL 资源(TexConv /cube 或 /octa /ibl 产物 + BRDF LUT) ----
        // `.TexCube` 与 `.TexOct` 是同一批烘焙数据的两种投影；八面体版是 2D 容器
        // （单层、带完整 mip 链），因此走 LoadTexture2D 而不是 LoadTextureCube。
        const bool octahedral = UseOctahedralEnv();

        env_irradiance = octahedral
            ? static_cast<Texture *>(texture_manager->LoadTexture2D(
                  OS_TEXT("res/image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k_irradiance.TexOct"),
                  false))
            : static_cast<Texture *>(texture_manager->LoadTextureCube(
                  OS_TEXT("res/image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k_irradiance.TexCube"),
                  false));
        if (!env_irradiance)
            return false;

        env_prefilter = octahedral
            ? static_cast<Texture *>(texture_manager->LoadTexture2D(
                  OS_TEXT("res/image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k_prefilter.TexOct"),
                  false))
            : static_cast<Texture *>(texture_manager->LoadTextureCube(
                  OS_TEXT("res/image/Environments/sky_34_2k/sky_34_cubemap_2k/sky_34_cubemap_2k_prefilter.TexCube"),
                  false));
        if (!env_prefilter)
            return false;

        env_brdf_lut = texture_manager->LoadTexture2D(OS_TEXT("res/image/brdflut.Tex2D"), false);
        if (!env_brdf_lut)
            return false;

        // 与默认 Lit 材质同一套 PBR 表面：环境光**算法**由环境层决定（见 InitIBLBinding）
        const mtl::MaterialRecipe *loaded_mesh_recipe = GetRecipe("IBLEnvironment.Lit");
        if (!loaded_mesh_recipe)
            return false;
        mesh_recipe = *loaded_mesh_recipe;

        return true;
    }

    /// 把 IBL 纹理注册进 bindless,句柄写入 AmbientEnvInfo(ambient_light_provider 槽参数块)
    bool InitIBLBinding()
    {
        if (!environment_system)
            return false;

        // SH 版：不注册任何贴图 / 不碰 bindless，只填 sh[9]。
        // 这条早退本身就是判据的一半——若 shader 偷偷采样了 env_tex，句柄全 0 会直接发黑。
        if (UseSHAmbient())
        {
            auto *ambient_env = environment_system->EditAmbientEnvInfo();
            if (!ambient_env)
                return false;

            FillSkyGradientSH(*ambient_env);
            environment_system->MarkAmbientEnvDirty();

            return environment_system->SetAmbientProvider("ambient_light_sh");
        }

        auto* gc  = GetGraphicsContext();
        auto* btm = gc ? gc->GetBindlessTextureManager() : nullptr;

        if (!btm)
            return false;

        const uint32_t irr_handle = btm->RegisterTexture(env_irradiance);
        const uint32_t pre_handle = btm->RegisterTexture(env_prefilter);
        const uint32_t lut_handle = btm->RegisterTexture(env_brdf_lut);

        if (!irr_handle || !pre_handle || !lut_handle)
            return false;

        auto* ambient_env = environment_system->EditAmbientEnvInfo();
        if (!ambient_env)
            return false;

        // w = prefilter 贴图的 mip 级数：shader 用 lod = roughness*(w-1) 取级，
        // 与 TexConv 的「第 i 级 = 粗糙度 i/(级数-1)」互逆。级数取自贴图自身头部，
        // 重烘出不同级数无需改这里；单级贴图(w<=1)退化为固定粗糙度采样。
        // CubeMap 与八面体版级数一致（同一批烘焙数据），故这里不必区分格式。
        const uint32_t prefilter_mips = env_prefilter->GetMipLevel();

        ambient_env->env_tex = math::Vector4u(irr_handle, pre_handle, lut_handle, prefilter_mips);
        // 配置 LDR 曝光度与太阳光强度动态调制（仅对 LDR 生效）
        environment_system->SetLDRExposure(true, 2.5f, 1.0f, 0.05f);
        environment_system->MarkAmbientEnvDirty();

        // 环境光**算法**（ambient_light_provider 槽的 GLSL 模块）同样由环境层决定：
        // 与上面写入的句柄配对 —— 选 IBL 模块就必须给句柄，否则采样 0 句柄静默发黑。
        // 句柄槽位(x=irradiance / y=prefilter / z=BRDF LUT / w=mip 数)两版完全一致，
        // 所以这里只换模块名，材质与光照模型一行都不用改。
        if (!environment_system->SetAmbientProvider(UseOctahedralEnv()
                ? "ambient_light_ibl_oct"
                : "ambient_light_ibl"))
            return false;

        return true;
    }

    bool InitVDM()
    {
        auto* buffer_manager = GetManager<BufferManager>();
        if (!buffer_manager)
            return false;

        mesh_vdm = new VertexDataManager(
            buffer_manager,
            CreateStandardGeometryVertexFormat());
        if (!mesh_vdm)
            return false;

        if (!mesh_vdm->Init(HGL_SIZE_1MB, HGL_SIZE_1MB, IndexType::U32))
            return false;

        return true;
    }

    MeshEntry* CreateMeshEntry(Geometry* geometry)
    {
        if (!geometry)
            return nullptr;

        auto mesh = std::make_unique<MeshEntry>();
        mesh->geometry = geometry;
        mesh->asset = PrimitiveAsset(geometry, &mesh_recipe, PrimitiveType::Triangles);

        MeshEntry* result = mesh.get();
        meshes.push_back(std::move(mesh));

        return result;
    }

    bool CreateGeometryMesh()
    {
        using namespace inline_geometry;

        // 共享顶点池：一批几何写进同一个 VertexDataManager
        GeometryCreater pc(mesh_vdm);
        SharedCreaterScope scope(GetGeometryAssetRegistry(), &pc);

        {
            auto geom = GetGeometry("geo:builtin:plane_square");
            if (!geom)
                return false;

            floor_mesh = CreateMeshEntry(geom);
            if (!floor_mesh)
                return false;
        }

        {
            auto geom = GetGeometry("geo:builtin:sphere_64");
            if (!geom || !CreateMeshEntry(geom))
                return false;
        }

        {

            auto geom = GetGeometry("geo:builtin:cube_2x2x2");
            if (!geom || !CreateMeshEntry(geom))
                return false;
        }

        {

            auto geom = GetGeometry("geo:builtin:torus_ring");
            if (!geom || !CreateMeshEntry(geom))
                return false;
        }

#if 0    // Arrow is not part of the *.geo.toml shape schema yet
        {

            auto geom = create_geometry([&](GeometryCreater* pc)
            {
                return CreateArrow(pc, &aci);
            });
            if (!geom || !CreateMeshEntry(geom))
                return false;
        }
#endif

        return true;
    }

    /// 每个物体一行独立的 PBR 参数,便于对比 IBL 在不同材质上的表现:
    ///   地板:白色粗糙电介质(以纯漫反射为主,看 irradiance)
    ///   球体:抛光金属(看 prefilter 反射)
    ///   立方体:半粗糙金属
    ///   圆环:红色光滑电介质
    ///   箭头:蓝色光滑电介质
    bool CreateMeshMaterialRows()
    {
        if (!GetMaterialDataBinding(MATERIAL_DATA_URI, 0, mesh_rows[0]))
            return false;

        for (uint32_t i = 1; i < kMeshRowCount; ++i)
            mesh_rows[i] = mesh_rows[0].RowOffset(i);

        return meshes.size() <= kMeshRowCount;
    }

    bool InitSceneEntities()
    {
        if (!ecs_context || !floor_mesh || !sky_geometry)
            return false;

        if (!CreateMeshMaterialRows())
            return false;

    #ifdef DRAW_SKY_SPHERE
        {
            sky_entity = ecs_context->CreateEntity<Entity>("SkySphere");
            auto transform = ecs_context->GetTransform(ecs_context->CreateTransform(sky_entity->GetEntityID(), Mobility::Movable));
            auto primitive_comp = sky_entity->GetContext()->GetOrCreateGeometryData(sky_entity);
            hgl::ecs::MaterialData *material_data_comp = sky_entity->GetContext()->GetOrCreateMaterialData(sky_entity);

            transform.SetLocalPosition(glm::vec3(0.0f));
            transform.SetLocalRotation(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            transform.SetLocalScale(glm::vec3(1.0f));
            transform.SetMobility(Mobility::Static);

            primitive_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(primitive_comp->GetOwnerID())->SetPrimitiveAsset(&sky_asset);

            // 立方体天空纹理写在配方文件里（[[textures]] sky_cube），实体端零纹理调用

            // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删
        }
    #endif//DRAW_SKY_SPHERE

        {
            auto* entity = ecs_context->CreateEntity<Entity>("Floor");
            auto transform = ecs_context->GetTransform(ecs_context->CreateTransform(entity->GetEntityID(), Mobility::Static));
            auto primitive_comp = entity->GetContext()->GetOrCreateGeometryData(entity);
            hgl::ecs::MaterialData *material_data_comp = entity->GetContext()->GetOrCreateMaterialData(entity);

            transform.SetLocalPosition(glm::vec3(0.0f, 0.0f, 0.0f));
            transform.SetLocalRotation(glm::angleAxis(glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
            transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform.SetMobility(Mobility::Static);

            primitive_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(primitive_comp->GetOwnerID())->SetPrimitiveAsset(&floor_mesh->asset);
            // 纹理已由配方文件声明（[[textures]]），实体端不再重复绑定
            hgl::ecs::MaterialData::MaterialDataAuthoringResource floor_struct{};
            floor_struct = mesh_rows[0];
            material_data_comp->SetDataResource(floor_struct);
            // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删
        }

        const size_t total = meshes.size();
        const size_t mesh_count = total > 1 ? (total - 1) : 1;
        size_t index = 0;

        for (auto& mesh_ptr : meshes)
        {
            auto* rm = mesh_ptr.get();
            if (!rm || rm == floor_mesh)
                continue;

            const size_t row_index = index + 1;
            if (row_index >= kMeshRowCount)
                return false;

            auto* entity = ecs_context->CreateEntity<Entity>("Mesh_" + std::to_string(index));
            auto transform = ecs_context->GetTransform(ecs_context->CreateTransform(entity->GetEntityID(), Mobility::Static));
            auto primitive_comp = entity->GetContext()->GetOrCreateGeometryData(entity);
            hgl::ecs::MaterialData *material_data_comp = entity->GetContext()->GetOrCreateMaterialData(entity);

            float angle = glm::radians(360.0f * static_cast<float>(index) / static_cast<float>(mesh_count));
            glm::quat rotation = glm::angleAxis(angle, glm::vec3(0.0f, 0.0f, 1.0f));
            glm::vec3 pos = glm::rotate(rotation, glm::vec3(6.5f, 0.0f, 0.0f));

            transform.SetLocalPosition(pos);
            transform.SetLocalRotation(rotation);
            transform.SetLocalScale(glm::vec3(1.0f, 1.0f, 1.0f));
            transform.SetMobility(Mobility::Static);

            primitive_comp->GetOwner()->GetContext()->GetOrCreateGeometryData(primitive_comp->GetOwnerID())->SetPrimitiveAsset(&rm->asset);
            hgl::ecs::MaterialData::MaterialDataAuthoringResource mesh_struct{};
            mesh_struct = mesh_rows[row_index];
            material_data_comp->SetDataResource(mesh_struct);
            // [A5a] 可见性真值已收敛到实体级（默认即可见）：原组件级 SetVisible(true) 等义调用已删

            ++index;
        }

        return true;
    }

    bool InitScene()
    {
        ecs_context = GetECSContext();
        if (!ecs_context)
            return false;

        if (!InitEnvironmentControl())
            return false;

    #ifdef DRAW_SKY_SPHERE
        if (!InitSkySphereResource())
            return false;
    #endif//DRAW_SKY_SPHERE

        if (!InitIBLBinding())
            return false;

        if (!InitVDM())
            return false;

        if (!CreateGeometryMesh())
            return false;

        return InitSceneEntities();
    }

    bool InitCamera()
    {
        if (!ecs_context || !ecs_context->EnsureCameraSystem())
            return false;

        camera_entity = ecs_context->CreateEntity<Entity>("MainCamera");
        // 相机 = **世界级资源**：经世界访问器创建/取回（实体只是宿主），不直取组件
        auto *camera = ecs_context->GetOrCreateCamera(camera_entity);

        camera->control_mode = CameraControlMode::ViewModel;
        camera->target = math::Vector3f(0.0f, 0.0f, 0.0f);
        camera->distance = 16.0f;
        camera->yaw = 45.0f;
        camera->pitch = -20.0f;
        camera->is_main_camera = true;
        camera->matrix_dirty = true;

        camera->viewport_info = GetViewportInfo();

        return true;
    }

public:
    void Tick(double delta_time) override
    {
        WorkObject::Tick(delta_time);

        // IBLENV_SELFCHECK=1：**帧摘要自检**（T0 判据前置；共用件 example/Shared/FrameDigest.h）——
        // 第 8 帧帧内回读颜色靶 ⇒ {不同字节取值数, 平均亮度, FNV-1a 64} 比对下方内联常量：
        // PASS 退出 0 / FAIL 退出 1。
        // 为什么本例必须有它：IBL 句柄（env_tex）是八台架摘要的**盲区**（只有本示例把 indirect_ibl
        // 选为 ambient_light_provider）⇒ 句柄错位这种静默发黑事故只有这条判据有分辨力。
        // 判据只建在**冻结相机 + 定帧**上；常量由实跑记录，改渲染结果（少画/错画/挪相机）必须复核重录。
        static const bool s_selfcheck = (getenv("IBLENV_SELFCHECK") != nullptr);
        static uint32_t   s_self_frame = 0;

        static constexpr uint32_t kSelfW           = 1280;
        static constexpr uint32_t kSelfH           = 720;
        static constexpr uint32_t kSelfMinDistinct = 256;   // 不空转守卫（恒定图会 FAIL）

        // 三条寻址/求值路径各自的基线（同一场景 / 同一组材质参数 / 同一套 LDR 曝光调制）。
        // 八面体版是另一张图、另一套采样 ⇒ 摘要必然不同，亮度只应差投影重采样那几个百分点
        // （实测 mean_luma 0.6934 Cube vs 0.7085 Oct，差 2.2%）；SH 版则换了整套算法（低频解析），
        // 亮度量级应接近但绝不会相同。三侧都钉住，
        // 「provider 接错 / 句柄错位 / 采样规则坏掉」在任一模式下都会翻红。
        static constexpr uint64_t kSelfFNV_Cube      = 0x54295E44E3C1677Cull;  // CubeMap（Debug）
        static constexpr uint64_t kSelfFNV_Octahedral = 0x942421F7437CC76Full; // 八面体（Debug）
        static constexpr uint64_t kSelfFNV_SH         = 0x079D318B8728A100ull; // SH9 解析梯度（Debug；mean_luma 0.6917）
        // 天空背景八面体（`IBLENV_SKY_OCT`）：环境 IBL 仍是 cube，**只有背景天空球**换投影。
        // 亮度应与 Cube 基线几乎一致（同一张 enviroment 数据的两种展开，mean_luma 实测同为 0.6934），
        // 差异只在重采样细节 ⇒ 这条基线同时是「天空背景投影没接错」的判据。
        // 三个开关混用（如 SKY_OCT+OCT）未钉基线：一次只开一个，各自对照。
        static constexpr uint64_t kSelfFNV_SkyOct    = 0xFF878ABE60D2BACBull;  // 天空背景八面体（Debug）

        if (s_selfcheck && ++s_self_frame == 8)
        {
            auto * rt = dynamic_cast<graph::SwapchainRenderTarget *>( ecs_context ? ecs_context->GetRenderTarget() : nullptr );
            if (!rt)
            {
                printf("[IBLSELF] no swapchain RT\n");
                std::exit(2);
            }

            ulre_example::FrameDigest expect;
            expect.width                = kSelfW;
            expect.height               = kSelfH;
            expect.distinct_byte_values = kSelfMinDistinct;
            expect.fnv1a64              = UseSkyOct()           ? kSelfFNV_SkyOct
                                        : UseSHAmbient()          ? kSelfFNV_SH
                                        : UseOctahedralEnv()      ? kSelfFNV_Octahedral
                                                                  : kSelfFNV_Cube;

            ulre_example::InstallFrameDigestHook(rt, expect, "IBLSELF");
        }
    }

    ~IBLEnvironmentApp()
    {
        // 共享顶点池的几何由 GeometryAssetRegistry 持有：先销毁几何，再销毁顶点池
        if (auto *registry = GetGeometryAssetRegistry())
            registry->Clear();

        SAFE_CLEAR(mesh_vdm)
    }

    bool Init() override
    {
        SetClearColor(Color4f(0.10f, 0.12f, 0.16f, 1.0f));

        if (!InitMaterial())
            return false;

        if (!InitScene())
            return false;

        if (!InitCamera())
            return false;

        return true;
    }
};

int os_main(int argc, os_char** argv)
{
    return RunFramework<IBLEnvironmentApp>(OS_TEXT("IBL Environment"), argc, argv, 1280, 720);
}
