#pragma once

#include<hgl/graph/VK.h>
#include<hgl/type/DataChain.h>

VK_NAMESPACE_BEGIN

class VDMAccess
{
protected:

    VertexDataManager *vdm;
    DataChain::UserNode *dc_node;
    const VIL *vil;

public:

    VDMAccess(VertexDataManager *_vdm,const VIL *_vil)
    {
        vdm=_vdm;
        vil=_vil;
        dc_node=nullptr;
    }

    virtual ~VDMAccess()=default;

    const VIL *                 GetVIL      ()const{ return vil; }
    const DataChain::UserNode * GetDCNode   ()const{ return dc_node; }
};//class VDMAccess

class VABAccessVDM:public VDMAccess
{
    VABAccess **vab;

public:

    ~VABAccessVDM() override
    {
        vdm->ReleaseVAB(dc_node);
    }

};//class VABAccessVDM:public VDMAccess

class IBAccessVDM:public VDMAccess
{
    IBAccess *iba;

public:

};//class IBAccessVDM:public VDMAccess
VK_NAMESPACE_END
