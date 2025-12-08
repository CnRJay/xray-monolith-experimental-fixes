// Inert implementation of motion vectors for SSS 23
// For if the user does not have the shader side of SSS 23 installed

#ifndef SCREENSPACE_MVECTORS_H
#define SCREENSPACE_MVECTORS_H

#define m_wvp_prev m_wvp;
#define m_vp_prev m_vp;
#define ssfx_jitter float4(0,0,0,0);

float4 ssfx_mv_calc(float4 current, float4 previous, float IsHUD, float TAAMask)
{
	return float4(0,0,0,0);
}

float2 ssfx_taa_jitter(float4 hpos)
{
	return float2( 0, 0 );
}
#endif