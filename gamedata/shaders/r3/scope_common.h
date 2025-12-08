// DO NOT EDIT, OR INCLUDE IN MODS

#ifndef SCOPE_COMMON_H
#define SCOPE_COMMON_H

#include "common.h"
#include "scope_defines.h"

#define ASPECT_CORRECT_TC(tc) (tc - 0.5) * float2(screen_res.x/screen_res.y, 1.0) + 0.5;
#define ASPECT_UNCORRECT_TC(tc) (tc - 0.5) * float2(screen_res.y/screen_res.x, 1.0) + 0.5;

int scope_phase;

struct Scope
{
	// SCOPECOORDS ------------
	// Range of -radius to +radius, around center.
	// To get a valid texture sample coordinate call: SCOPECOORD_TO_TEXCOORD
	float2 ffp;
	float2 sfp;

	float2 exit_pupil;
	float2 center;
	float radius;

	// TEXCOORDS --------------
	float4 hpos;
    float2 tc0;	

	float2 ssp_jitter;  // screenspace jitter offset

	float4 dbg;
};
static Scope scope;


struct v_out {
    float4 hpos : SV_Position;
    float2 tc0 : TEXCOORD0;	
	float3 w_P : POSITION0;
	float3 w_T : TANGENT0;
	float3 w_B : BINORMAL0;
	float3 w_N : NORMAL0;
	float4 v_P : POSITION1;
	float3 v_T : TANGENT1;
	float3 v_B : BINORMAL1;
	float3 v_N : NORMAL1;

	float2 ssp_jitter : TEXCOORD1;
};

Texture2D s_reticle;

float4 m_hud_params;
float4 m_hud_fov_params;
float4 ogse_c_screen;
uniform float4 s3ds_param_1;
uniform float4 s3ds_param_2;
uniform float4 s3ds_param_3;
uniform float4 s3ds_param_4;
uniform float4 markswitch_current;
uniform float4 markswitch_color;
uniform float4 shader_param_8;

uniform float4 output_res;

float4 scope_w_ffp;
float4 scope_w_sfp;
float4 scope_w_eyepiece;
int scope_debug;

uniform int scope_svp;
float isSVPActive() { return scope_svp; }
float isSVPFrame() { return m_hud_params.w > 0.5; }

#define SCOPE_TO_FRAMEBUFFER_RATIO (isSVPFrame() ? 2.0 : 1.0)


float zoomRotateFactor() { return m_hud_params.x; }


float zoomFactor() {
    return s3ds_param_2.w;
}

uniform float4 shader_scope_params;
float curMag() { return shader_scope_params.x; }
float minMag() { return shader_scope_params.y; }
float maxMag() { return shader_scope_params.z; }
float mag_between_fovs(float current, float desired) {
	return 1.0 / (tan(desired/2.0) / tan(current/2.0));
}

float digitalZoom() {
    return isSVPActive()
		? 1.0
		: max( 1.0
		     , mag_between_fovs( ogse_c_screen.x * (3.14159/180.0)
		                       , shader_scope_params.w));
}

float2 ndc2(float4 p) {
	return p.xy / p.w;
}

float hack_tex_angle;

float2 SCOPECOORD_TO_TEXCOORD(float2 sc) {
	if (!isSVPActive() && scope_phase & SCOPE_PHASE_IMAGE) {
		// This is fake pip mode, so we have to correct the coordinates
		// FIXME: These coordinates need to be rotated to align with eye up
		float screen_delta  = length(ddy(scope.hpos.xy * output_res.zw));
		float texture_delta = length(ddy(scope.tc0.xy));
		float tc_multiplier = texture_delta / screen_delta;

		sc -= 0.5;

		float angle = hack_tex_angle;

		sc = float2( sc.x * cos(angle) - sc.y * sin(angle)
		           , sc.x * sin(angle) + sc.y * cos(angle));

		float f = (digitalZoom() * tc_multiplier);
		return ASPECT_UNCORRECT_TC(sc / f + 0.5);

	} else {
		return sc;
	}
}

float3 SampleBackbuffer(float2 tc) {
	return s_image.Sample(smp_base, tc).rgb;
}

bool VALID(float2 scopecoord) {
	return distance(scopecoord, scope.center) < scope.radius
		&& zoomRotateFactor() > 0.5;
}

float dbg_wp(v_out v, float4 p, float d) {
	float2 screen_tc = (v.hpos.xy - v.ssp_jitter) * output_res.zw;
	float2 ffp_ndc = ndc2(mul(m_VP, p));
	float2 ffp_tc  = ffp_ndc * float2(0.5, -0.5) + 0.5;

	float2 ffp_tc_a = ASPECT_CORRECT_TC(ffp_tc);
	float2 screen_tc_a = ASPECT_CORRECT_TC(screen_tc);

	return distance(ffp_tc_a, screen_tc_a) < d ? 1.0 : 0.0;
}

float2 world_to_corrected_tc(v_out v, float4 w_P) {
	float2 screen_tc = (v.hpos.xy - v.ssp_jitter) * output_res.zw;
	float2 ffp_ndc = ndc2(mul(m_VP, w_P));
	float2 ffp_tc  = ffp_ndc * float2(0.5, -0.5) + 0.5;

	return ASPECT_CORRECT_TC(ffp_tc);
}

Scope new_Scope(v_out v) {
	Scope s;

	s.ssp_jitter = v.ssp_jitter;

	float cm = .01;
	float eye_relief = s3ds_param_1.y * cm;
	if (eye_relief == 0) eye_relief = 2 * cm;

	// FIXME: Projection will flip if exit pupil is behind eye
	float4 scope_w_exit = scope_w_eyepiece - (normalize(scope_w_sfp - scope_w_ffp) * eye_relief);
    
	float y = dbg_wp(v, scope_w_exit, 0.002);
	float r = dbg_wp(v, scope_w_eyepiece, .002);
	float g = dbg_wp(v, scope_w_ffp, .004);
	float b = dbg_wp(v, scope_w_sfp, .008);
	s.dbg = float4(r, g-r, b-(g+r), max(max(r,g),b));
	s.dbg = max(float4(y,y, 0, y), s.dbg);

	float2 eye_tc = world_to_corrected_tc(v, scope_w_eyepiece);
	
    float screen_delta  = length(ddy((v.hpos.xy - v.ssp_jitter) * output_res.zw));
    float texture_delta = length(ddy(v.tc0.xy));
    float tc_multiplier = texture_delta / screen_delta;

	{
		float mag = curMag() / minMag();
		// COMPUTE FFP
		float2 ffp_tc = world_to_corrected_tc(v, scope_w_ffp);
		float2 ffp_offset_tc = eye_tc - ffp_tc;

		s.ffp = (v.tc0 + ffp_offset_tc*tc_multiplier - 0.5) / mag + 0.5 ;
	}

	{
		// COMPUTE SFP
		float2 sfp_tc = world_to_corrected_tc(v, scope_w_sfp);
		float2 sfp_offset_tc = eye_tc - sfp_tc;
		s.sfp = v.tc0 + sfp_offset_tc*tc_multiplier;
	}

	{
		// COMPUTE EXIT PUPIL
		float2 exit_tc = world_to_corrected_tc(v, scope_w_exit);
		float2 exit_offset_tc = eye_tc - exit_tc;

		s.exit_pupil = v.tc0 + exit_offset_tc*tc_multiplier;
	}

	s.tc0 = v.tc0;
	s.hpos = float4(v.hpos.xy - v.ssp_jitter, v.hpos.z, v.hpos.w);
	s.center = float2(0.5, 0.5);
	s.radius = 0.5;

	return s;
}































#endif