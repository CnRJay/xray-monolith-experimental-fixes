/*
	=====================================================================
	Addon      : Shader 3D Scopes
	Link       : https://www.moddb.com/mods/stalker-anomaly/addons/shader-3d-scopes
	Authors    : LVutner, party_50

	All credit to original authors.
	=====================================================================
*/

#include "scope_3dss_common.h"

float4 scope_custom_reticle(Scope S) {
	float RETICLE_SIZE = s3ds_param_1.x;
	float EYE_RELIEF = s3ds_param_1.y;
	float EXIT_PUPIL = s3ds_param_1.z;
	int FFP = s3ds_param_1.w;
	
	float REFLECTION_HUE = s3ds_param_2.x;
	int MIN_ZOOM_1X = s3ds_param_2.z;
	
	float DIRT_INTENSITY = s3ds_param_3.z;
	float CHROMA_POWER = s3ds_param_3.w;
		
	float RETICLE_PROJECT = 10;
	float SHADOW_WIDTH = 0.15;
	
	if (RETICLE_TYPE == RT_FLAT_SCREEN)
		RETICLE_PROJECT = 0;

	float current_zoom = (curMag()-minMag()) * 0.4 + 1.0;
	float zoom_part = zoomPercent();

	
	if (!SETTING(SETTINGS, ST_CHROMATISM)) {
		CHROMA_POWER = 0;
	}
	
	float lum = current_lum();

	// Sight reticle
	float2 reticle_lens_tc = S.sfp;
	float2 reticle_tc = FFP ? S.ffp : S.sfp;  // FIXME: Scaling size here also dampens the movement

	if (RETICLE_TYPE == RT_SCREEN || RETICLE_TYPE == RT_FLAT_SCREEN) {
		reticle_lens_tc = S.tc0;
		reticle_tc = S.tc0;
	}

    float4 mark_texture = float4(0, 0, 0, 0);
	if (reticle_tc.x >= 0 && reticle_tc.x <= 1 && reticle_tc.y >= 0 && reticle_tc.y <= 1)
	{
		float2 tc = SCOPECOORD_TO_TEXCOORD(clamp(reticle_tc, 0, 1));
		mark_texture = s_reticle.Sample(smp_base, tc);

		// Try and determine whether the texture is transparent or not.
		//    Sample from two coordinates that are likely to be "transparent" in case mipmaps are missing
		bool opaque1 = s_reticle.SampleLevel(smp_base, float2(0.233, 0.1), 24).a > .99;
		bool opaque2 = s_reticle.SampleLevel(smp_base, float2(0.1, 0.233), 24).a > .99;
		if (opaque1 && opaque2)
			mark_texture.a = length(mark_texture.xyz);
	}
	
	float4 giperon_sfp;
	if (RETICLE_TYPE == RT_GIPERON)
	{
		mark_texture = mark_texture.r;
		float finder = s_reticle.Sample(smp_base, SCOPECOORD_TO_TEXCOORD(clamp(reticle_lens_tc, 0, 1))).g;
		float shift_3x = 0.053;
		float angle = -PI * (zoom_part + shift_3x) / (1 + shift_3x);
		float2 tc = reticle_lens_tc - 0.5;
		tc = float2(tc.x * cos(angle) - tc.y * sin(angle), tc.x * sin(angle) + tc.y * cos(angle));
		tc += 0.5;
		float numbers = s_reticle.Sample(smp_base, SCOPECOORD_TO_TEXCOORD(clamp(tc, 0, 1))).b;
		giperon_sfp = float4(0, 0, 0, max(finder, numbers));
	}

	if (RETICLE_TYPE == RT_ACOG)
	{
		float3 black = float3(0, 0, 0);
		float3 text = float3(0.3, 0.3, 0.3);
		float tritium_lum = 0.2;
		mark_texture = rgba_blend(rgba_blend(float4(black, mark_texture.r), float4(markswitch_color.rgb * max(tritium_lum, lum * 2), mark_texture.g)), float4(text, mark_texture.b * lum));
	}
	
	if (RETICLE_TYPE == RT_LED || RETICLE_TYPE == RT_GIPERON)
	{
		mark_texture = float4(markswitch_color.rgb, mark_texture.a);
		giperon_sfp = float4(markswitch_color.rgb, giperon_sfp.a);
	}
	
	if (RETICLE_TYPE == RT_SPECTER)
	{
		float3 black = float3(0, 0, 0);
		float4 light = float4(0, 0, 0, 0);
		if (markswitch_current.x == 1)
			light = float4(markswitch_color.rgb, mark_texture.g);
		if (markswitch_current.x == 2)
			light = float4(markswitch_color.rgb, mark_texture.b);
		
		mark_texture = rgba_blend(float4(black, mark_texture.r), light);
	}
	
	if (RETICLE_TYPE == RT_LED_MASKED)
	{
		float3 black = float3(0, 0, 0);
		mark_texture = rgba_blend(float4(black, mark_texture.r), float4(markswitch_color.rgb, mark_texture.g));
	}
	
	if (!SETTING(SETTINGS, ST_SEE_THROUGH))
	{
		mark_texture.rgb *= zoomRotateFactor();
		giperon_sfp.rgb *= zoomRotateFactor();
	}

	return RETICLE_TYPE == RT_GIPERON 
		? rgba_blend(mark_texture, giperon_sfp)
		: mark_texture;
}