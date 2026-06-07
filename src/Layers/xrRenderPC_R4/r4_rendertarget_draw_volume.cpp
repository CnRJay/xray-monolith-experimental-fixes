#include "stdafx.h"
#include "../xrRender/du_sphere_part.h"
#include "../xrRender/du_cone.h"
#include "../xrRender/du_sphere.h"

void CRenderTarget::draw_volume(light* L)
{
    switch (L->flags.type)
    {
    case IRender_Light::REFLECTED:
    case IRender_Light::POINT:
        RCache.set_Geometry(g_accum_point);
		RCache.Render(D3DPT_TRIANGLELIST, 0, 0,DU_SPHERE_NUMVERTEX, 0,DU_SPHERE_NUMFACES);
        break;
    case IRender_Light::SPOT:
        RCache.set_Geometry(g_accum_spot);
		RCache.Render(D3DPT_TRIANGLELIST, 0, 0,DU_CONE_NUMVERTEX, 0,DU_CONE_NUMFACES);
        break;
    case IRender_Light::OMNIPART:
        RCache.set_Geometry(g_accum_omnipart);
		RCache.Render(D3DPT_TRIANGLELIST, 0, 0,DU_SPHERE_PART_NUMVERTEX, 0,DU_SPHERE_PART_NUMFACES);
        break;
    default:
        break;
    }
}

void CRenderTarget::draw_occq_volume(light* L)
{
    // Use the spatial bounding sphere for all light types in occlusion queries.
    // Cone and sphere-part geometry give false negatives when viewed along the
    // light axis or from inside the bounding volume; a sphere is conservative
    // and avoids both problems.
    Fmatrix xform;
    xform.scale(L->spatial.sphere.R, L->spatial.sphere.R, L->spatial.sphere.R);
    xform.c = L->spatial.sphere.P;
    RCache.set_xform_world(xform);
    RCache.set_Geometry(g_accum_point);
    RCache.Render(D3DPT_TRIANGLELIST, 0, 0, DU_SPHERE_NUMVERTEX, 0, DU_SPHERE_NUMFACES);
}
