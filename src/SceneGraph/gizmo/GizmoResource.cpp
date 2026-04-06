#include<hgl/vk/VKMaterialInstance.h>
#include<hgl/vk/pipeline/VKGraphicsPipeline.h>
#include<hgl/graph/geo/VKGeometry.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/pipeline/VKRenderTargetFormat.h>
#include<hgl/color/Color.h>
#include<hgl/graph/geo/InlineGeometry.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/MaterialManager.h>
#include<hgl/graph/module/PrimitiveManager.h>
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
        static GraphicsContext *graphics_context=nullptr;
        static RenderTargetFormat *gizmo_render_pass=nullptr;
        static MaterialManager *gizmo_mtl_manager=nullptr;

        struct GizmoResource
        {
            ShaderProgram *          mtl;
            MaterialInstance *  mi[size_t(GizmoColor::RANGE_SIZE)];
            VertexDataManager * vdm;

            GeometryCreater *  prim_creater;
        };

        static GizmoResource    gizmo_triangle{};

        struct GizmoMesh
        {
            Geometry *geometry;
            Primitive *primitive;

        public:

            void Create(Geometry *p)
            {
                geometry=p;
                if (graphics_context)
                {
                    auto *primitive_manager = graphics_context->GetPrimitiveManager();
                    primitive = primitive_manager ? primitive_manager->CreatePrimitive(geometry,gizmo_triangle.mi[0]) : nullptr;
                }
                else
                {
                    primitive = nullptr;
                }
            }

            void Clear()
            {
                primitive=nullptr;
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

        bool InitMI(GizmoResource *gr)
        {
            if(!gr||!gr->mtl)
                return(false);

            Color4f color;

            for(uint i=0;i<uint(GizmoColor::RANGE_SIZE);i++)
            {
                color=GetColor4f(gizmo_color[i],1.0);

                MaterialInstanceSpec mi_spec;
                mi_spec.material = gr->mtl;
                mi_spec.vil = nullptr;
                mi_spec.instance_data = &color;
                mi_spec.instance_data_size = sizeof(color);
                mi_spec.preset = GraphicsPipelinePreset::GizmoOverlay3D;

                gr->mi[i]=gizmo_mtl_manager->AcquireMaterialInstance(mi_spec);

                if(!gr->mi[i])
                    return(false);
            }

            return(true);
        }

        bool InitGizmoResource3D()
        {
            if(!graphics_context)
                return(false);

            VulkanDevice *device=graphics_context->GetDevice();
            auto *buffer_manager = graphics_context->GetBufferManager();
            RenderTargetFormat *render_pass=gizmo_render_pass;

            if(!device || !render_pass)
                return(false);

            gizmo_mtl_manager=graphics_context->GetMaterialManager();

            {
                mtl::Material3DCreateConfig cfg(PrimitiveType::Triangles);

                cfg.local_to_world=true;
                cfg.material_instance=true;

                gizmo_triangle.mtl=gizmo_mtl_manager->AcquireMaterial(mtl::MaterialPreset::PureColor3D,&cfg);
                if(!gizmo_triangle.mtl)
                    return(false);

                gizmo_triangle.mtl->Update();
            }

            (void)render_pass;

            if(!InitMI(&gizmo_triangle))
                return(false);

            {
                gizmo_triangle.vdm=new VertexDataManager(buffer_manager,gizmo_triangle.mtl->GetDefaultVIL());

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

    bool InitGizmoResource(GraphicsContext *gc, RenderTargetFormat *rp)
    {
        if(!gc)
            return(false);

        graphics_context=gc;
        gizmo_render_pass=rp;

        VulkanDevice *device=graphics_context->GetDevice();
        if(!device)
            return(false);

        if(!InitGizmoResource3D())
            return(false);

        return(true);
    }

    void FreeGizmoResource()
    {
        //ClearGizmoRotateMesh();
        //ClearGizmoScaleMesh();

        for(GizmoMesh &gr:gizmo_mesh)
            gr.Clear();

        SAFE_CLEAR(gizmo_triangle.prim_creater);
        SAFE_CLEAR(gizmo_triangle.vdm);

        gizmo_triangle.mtl = nullptr;
        for (size_t i = 0; i < size_t(GizmoColor::RANGE_SIZE); ++i)
            gizmo_triangle.mi[i] = nullptr;

        gizmo_mtl_manager = nullptr;
        gizmo_render_pass = nullptr;
        graphics_context = nullptr;
    }

    MaterialInstance *GetGizmoMI3D(const GizmoColor &color)
    {
        RANGE_CHECK_RETURN_NULLPTR(color)

        return gizmo_triangle.mi[size_t(color)];
    }

    Primitive *GetGizmoMeshPrimitive(const GizmoShape &shape)
    {
        if(!graphics_context)
            return nullptr;

        RANGE_CHECK_RETURN_NULLPTR(shape)

        return gizmo_mesh[size_t(shape)].primitive;
    }
}//namespace hgl::graph
