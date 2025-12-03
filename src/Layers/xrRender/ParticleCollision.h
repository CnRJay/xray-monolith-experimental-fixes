#ifndef ParticleCollisionH
#define ParticleCollisionH

#include "../../xrCDB/xrXRC.h"
#include "../../xrCDB/ispatial.h"

struct CollisionContext
{
    xrXRC xrc;
    collide::rq_results r_temp;
    xr_vector<ISpatial*> r_spatial;
};

#endif
