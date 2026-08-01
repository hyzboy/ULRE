#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/color/Color.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<hgl/graph/module/ResourceDomainManager.h>
#include<hgl/graph/asset/PrimitiveAsset.h>
#include"GizmoResource.h"

namespace hgl::graph
{
    //bool InitGizmoScaleMesh();
    //void ClearGizmoScaleMesh();
    //
    //bool InitGizmoRotateMesh();
    //void ClearGizmoRotateMesh();

    namespace
    {
        GeometryVertexFormat CreateGizmoGeometryVertexFormat()
        {
            GeometryVertexFormat gvf;
            gvf.Add(VertexSemantic::Position, VK_FORMAT_R32G32B32_SFLOAT, 3, sizeof(float) * 3);
            return gvf;
        }

        static GraphicsContext *graphics_context=nullptr;

        struct GizmoResource
        {
            ShaderProgram *          mtl;
            DeviceBuffer *      mi_ssbo;
            VertexDataManager * vdm;
            mtl::MaterialRecipe color_recipe[size_t(GizmoColor::RANGE_SIZE)]{};

            GeometryCreater *  prim_creater;
        };

        static GizmoResource    gizmo_triangle{};

        struct GizmoMesh
        {
            Geometry *geometry;
            PrimitiveAsset asset;

        public:

            void Create(Geometry *p)
            {
                geometry=p;
                asset = PrimitiveAsset(geometry, static_cast<const mtl::MaterialRecipe *>(nullptr), PrimitiveType::Triangles);
            }

            void Clear()
            {
                asset = PrimitiveAsset();
                geometry=nullptr;
            }
        };//class GizmoMesh

        GizmoMesh         gizmo_mesh[size_t(GizmoShape::RANGE_SIZE)]{};

        void InitGizmoMesh(const GizmoShape &gs,Geometry *geometry)
        {
            if(!geometry)
                return;

            gizmo_mesh[size_t(gs)].Create(geometry);
        }

        // Create the SSBO holding one Color4f per GizmoColor slot.
        // PureColor3D MI data is exactly one Color4f (16 bytes, std430 vec4).
        bool InitColorSSBO(GizmoResource *gr)
        {
            if (!gr || !gr->mtl)
                return false;

            if (!graphics_context)
                return false;

            auto *buffer_manager = graphics_context->GetBufferManager();
            auto *domain_manager = graphics_context->GetResourceDomainManager();
            if (!buffer_manager || !domain_manager)
                return false;

            bool has_struct_binding = false;
            for (const auto &req : gr->mtl->GetMaterialResourceLayout().requirements)
            {
                if (req.semantic == mtl::DescriptorSemantic::MaterialSSBOSlotData)
                {
                    has_struct_binding = true;
                    break;
                }
            }
            if (!has_struct_binding)
                return true;

            const uint32_t color_count = uint32_t(GizmoColor::RANGE_SIZE);

            gr->mi_ssbo = buffer_manager->CreateSSBO(
                "GizmoResource:PureColor3D:MIData",
                VkDeviceSize(sizeof(Color4f)) * color_count,
                nullptr,
                SharingMode::Exclusive);
            if (!gr->mi_ssbo)
                return false;

            {
                // 临时 accessor 用于写入颜色数据 / Temporary accessor for color write
                auto *acc = SSBOArrayAccessor<Color4f>::Create(gr->mi_ssbo, color_count);
                if (!acc)
                    return false;

                for (uint32_t i = 0; i < color_count; ++i)
                    (*acc)[i] = GetColor4f(gizmo_color[i], 1.0f);
                acc->Commit();
                delete acc;
            }

            for (const auto &req : gr->mtl->GetMaterialResourceLayout().requirements)
            {
                if (req.semantic != mtl::DescriptorSemantic::MaterialSSBOSlotData)
                    continue;

                has_struct_binding = true;
                const mtl::SSBOAddress addr{req.ssbo_type, req.ssbo_id, 0};
                if (!domain_manager->RegisterBuffer(addr, gr->mi_ssbo, color_count))
                    return false;

                for (uint32_t c = 0; c < color_count; ++c)
                {
                    auto &recipe = gr->color_recipe[c];
                    recipe = mtl::MaterialRecipe{};
                    recipe.recipe_name = "GizmoColor_" + std::to_string(c);
                    recipe.mtl_def_id = "PureColor3D";
                    recipe.structs.clear();
                    recipe.textures.clear();
                    recipe.ssbo_assets.clear();

                    mtl::RecipeStructBinding struct_binding{};
                    struct_binding.ssbo_slot = req.ssbo_slot;
                    struct_binding.ssbo_type = req.ssbo_type;
                    struct_binding.ssbo_id = req.ssbo_id;
                    struct_binding.ssbo_element_index = c;
                    struct_binding.use_ssbo_element_index = true;
                    struct_binding.shared_across_instances = true;
                    recipe.structs.emplace_back(std::move(struct_binding));
                }
            }

            return has_struct_binding;
        }

        bool InitGizmoResource3D()
        {
            if(!graphics_context)
                return(false);

            VulkanDevice *device=graphics_context->GetDevice();
            auto *buffer_manager = graphics_context->GetBufferManager();

            if(!device)
                return(false);

            auto *gizmo_mtl_manager = graphics_context->GetMaterialManager();
            if(!gizmo_mtl_manager)
                return(false);

            {
                mtl::MaterialRecipe recipe{};
                recipe.mtl_def_id = "PureColor3D";

                gizmo_triangle.mtl=gizmo_mtl_manager->AcquireMaterialProgram(recipe.mtl_def_id,recipe,PrimitiveType::Triangles);
                if(!gizmo_triangle.mtl)
                    return(false);

                gizmo_triangle.mtl->Update();
            }

            {
                const GeometryVertexFormat gizmo_gvf = CreateGizmoGeometryVertexFormat();

                if(!InitColorSSBO(&gizmo_triangle))
                    return(false);

                gizmo_triangle.vdm=new VertexDataManager(
                    buffer_manager,
                    gizmo_gvf);

                if(!gizmo_triangle.vdm)
                    return(false);

                if(!gizmo_triangle.vdm->Init(   HGL_SIZE_1MB,       //最大顶点数量
                                                HGL_SIZE_1MB,       //最大索引数量
                                                IndexType::U16))    //索引类型
                    return(false);
            }

            {
                gizmo_triangle.prim_creater=new GeometryCreater(gizmo_triangle.vdm);

                if(!gizmo_triangle.prim_creater)
                    return(false);
            }

            {
                using namespace inline_geometry;

                {
                    InitGizmoMesh(GizmoShape::Square,CreatePlaneSqaure(gizmo_triangle.prim_creater));
                }

                {
                    CircleCreateInfo cci;

                    cci.center=math::Vector2f(0,0);
                    cci.radius=math::Vector2f(0.5,0.5);
                    cci.field_count=16;
                    cci.has_center=false;

                    InitGizmoMesh(GizmoShape::Circle,CreateCircle3DByIndexTriangles(gizmo_triangle.prim_creater,&cci));
                }

                {
                    CubeCreateInfo cci;

                    cci.normal=true;
                    cci.tangent=false;
                    cci.tex_coord=false;

                    InitGizmoMesh(GizmoShape::Cube,CreateCube(gizmo_triangle.prim_creater,&cci));
                }

                {
                    InitGizmoMesh(GizmoShape::Sphere,CreateSphere(gizmo_triangle.prim_creater,16));
                }

                {
                    ConeCreateInfo cci;

                    cci.radius      =GIZMO_CONE_RADIUS;         //圆锥半径
                    cci.halfExtend  =1;                 //圆锤一半高度
                    cci.numberSlices=16;        //圆锥底部分割数
                    cci.numberStacks=3;         //圆锥高度分割数

                    InitGizmoMesh(GizmoShape::Cone,CreateCone(gizmo_triangle.prim_creater,&cci));
                }

                {
                    struct CylinderCreateInfo cci;

                    cci.halfExtend  =1;         //圆柱一半高度
                    cci.numberSlices=16;        //圆柱底部分割数
                    cci.radius      =1;         //圆柱半径

                    InitGizmoMesh(GizmoShape::Cylinder,CreateCylinder(gizmo_triangle.prim_creater,&cci));
                }

                {
                    struct TorusCreateInfo tci;

                    tci.innerRadius=0.975;
                    tci.outerRadius=1.0;
                    tci.numberSlices=64;
                    tci.numberStacks=8;

                    InitGizmoMesh(GizmoShape::Torus,CreateTorus(gizmo_triangle.prim_creater,&tci));
                }

                ENUM_CLASS_FOR(GizmoShape,int,i)
                {
                    if(!gizmo_mesh[i].geometry)
                        return(false);
                }
            }

            return(true);
        }
    }//namespace

    bool InitGizmoResource(GraphicsContext *gc, RenderPass *rp)
    {
        if(!gc)
            return(false);

        (void)rp;
        graphics_context=gc;

        VulkanDevice *device=graphics_context->GetDevice();
        if(!device)
            return(false);

        if(!InitGizmoResource3D())
            return(false);

        return(true);
    }

    void FreeGizmoResource()
    {
        for(GizmoMesh &gr:gizmo_mesh)
            gr.Clear();

        SAFE_CLEAR(gizmo_triangle.prim_creater);
        SAFE_CLEAR(gizmo_triangle.vdm);
        SAFE_CLEAR(gizmo_triangle.mi_ssbo);

        gizmo_triangle.mtl = nullptr;

        graphics_context = nullptr;
    }

    const mtl::MaterialRecipe *GetGizmoRecipe3D(const GizmoColor &color)
    {
        RANGE_CHECK_RETURN_NULLPTR(color)
        return gizmo_triangle.color_recipe + size_t(color);
    }

    const PrimitiveAsset *GetGizmoMeshAsset(const GizmoShape &shape)
    {
        if(!graphics_context)
            return nullptr;

        RANGE_CHECK_RETURN_NULLPTR(shape)

        return &(gizmo_mesh[size_t(shape)].asset);
    }
}//namespace hgl::graph
