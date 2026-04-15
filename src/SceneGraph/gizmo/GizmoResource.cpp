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
        const VertexFormatMap kGizmoVertexFormats = {
            {VAN::Position, PF_RGB32F},
            {VAN::Normal,   PF_RGB32F},
            {VAN::Tangent,  PF_RGB32F},
            {VAN::TexCoord, PF_RG32F},
        };

        static GraphicsContext *graphics_context=nullptr;
        static RenderTargetFormat *gizmo_render_pass=nullptr;
        static MaterialManager *gizmo_mtl_manager=nullptr;

        struct GizmoResource
        {
            MaterialTemplate *          mtl;
            InstanceDataDomain *    domain = nullptr;
            int                         slot_id[size_t(GizmoColor::RANGE_SIZE)] = {};
            VertexDataManager * vdm;

            GeometryCreater *  prim_creater;
        };

        static GizmoResource    gizmo_triangle{};

        struct GizmoMesh
        {
            Geometry *geometry;
            Primitive *primitive[size_t(GizmoColor::RANGE_SIZE)];

        public:

            void Create(Geometry *p)
            {
                geometry=p;
                if (graphics_context && gizmo_triangle.domain && gizmo_triangle.mtl)
                {
                    const VIL *vil = gizmo_triangle.mtl->GetDefaultVIL();
                    for (size_t c = 0; c < size_t(GizmoColor::RANGE_SIZE); ++c)
                    {
                        if (gizmo_triangle.slot_id[c] >= 0)
                        {
                            const IDDHandle gizmo_idd_handle = (gizmo_mtl_manager && gizmo_triangle.domain)
                                                              ? gizmo_mtl_manager->GetIDDManager()->GetHandle(gizmo_triangle.domain)
                                                              : IDDHandle{};
                            PrimitiveMaterialSlot slot{gizmo_triangle.mtl, gizmo_triangle.domain,
                                                       gizmo_idd_handle,
                                                       gizmo_mtl_manager ? gizmo_mtl_manager->GetIDDManager() : nullptr,
                                                       gizmo_triangle.slot_id[c], vil,
                                                       GraphicsPipelinePreset::GizmoOverlay3D};
                            primitive[c] = DirectCreatePrimitive(geometry, slot);
                        }
                        else
                            primitive[c] = nullptr;
                    }
                }
                else
                {
                    for (size_t c = 0; c < size_t(GizmoColor::RANGE_SIZE); ++c)
                        primitive[c] = nullptr;
                }
            }

            void Clear()
            {
                for (size_t c = 0; c < size_t(GizmoColor::RANGE_SIZE); ++c)
                    primitive[c] = nullptr;
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
            if(!gr||!gr->mtl||!gizmo_mtl_manager)
                return(false);

            gr->domain = gizmo_mtl_manager->GetOrCreateDefaultDomain(gr->mtl);
            if(!gr->domain)
                return(false);

            Color4f color;

            for(uint i=0;i<uint(GizmoColor::RANGE_SIZE);i++)
            {
                gr->slot_id[i] = gr->domain->AllocSlot();
                if(gr->slot_id[i] < 0)
                    return(false);

                color=GetColor4f(gizmo_color[i],1.0);
                memcpy(gr->domain->GetSlotData(gr->slot_id[i]), &color, sizeof(color));
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

                gizmo_triangle.mtl=gizmo_mtl_manager->AcquireMaterialInternal(mtl::MaterialPreset::PureColor3D,&cfg);
                if(!gizmo_triangle.mtl)
                    return(false);

                gizmo_triangle.mtl->Update();
            }

            (void)render_pass;

            if(!InitMI(&gizmo_triangle))
                return(false);

            {
                gizmo_triangle.vdm=new VertexDataManager(buffer_manager,kGizmoVertexFormats);

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

        if(gizmo_triangle.domain)
        {
            for (size_t i = 0; i < size_t(GizmoColor::RANGE_SIZE); ++i)
                if(gizmo_triangle.slot_id[i] >= 0)
                    gizmo_triangle.domain->FreeSlot(gizmo_triangle.slot_id[i]);
        }

        SAFE_CLEAR(gizmo_triangle.prim_creater);
        SAFE_CLEAR(gizmo_triangle.vdm);

        gizmo_triangle.mtl = nullptr;
        gizmo_triangle.domain = nullptr;
        for (size_t i = 0; i < size_t(GizmoColor::RANGE_SIZE); ++i)
            gizmo_triangle.slot_id[i] = -1;

        gizmo_mtl_manager = nullptr;
        gizmo_render_pass = nullptr;
        graphics_context = nullptr;
    }

    int GetGizmoMIID3D(const GizmoColor &color)
    {
        if(size_t(color) >= size_t(GizmoColor::RANGE_SIZE))
            return -1;

        return gizmo_triangle.slot_id[size_t(color)];
    }

    Primitive *GetGizmoMeshPrimitive(const GizmoShape &shape, const GizmoColor &color)
    {
        if(!graphics_context)
            return nullptr;

        RANGE_CHECK_RETURN_NULLPTR(shape)
        RANGE_CHECK_RETURN_NULLPTR(color)

        return gizmo_mesh[size_t(shape)].primitive[size_t(color)];
    }
}//namespace hgl::graph
