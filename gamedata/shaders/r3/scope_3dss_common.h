/*
	=====================================================================
	Addon      : Shader 3D Scopes
	Link       : https://www.moddb.com/mods/stalker-anomaly/addons/shader-3d-scopes
	Authors    : LVutner, party_50

	All credit to original authors.
	=====================================================================
*/

#ifndef SCOPE_3DSS_COMMON_H
#define SCOPE_3DSS_COMMON_H


#include "scope_common.h"

#include "common.h"
#include "nv_utils.h"
#include "thermal_utils.h"

#define REFLECTIONS_FACTOR 0.3
#define SPECULAR_FACTOR 2.0

#define PI 3.1415926

#define RT_NONE 0
#define RT_LED 1
#define RT_LED_MASKED 2
#define RT_ACOG 3
#define RT_SPECTER 4
#define RT_GIPERON 5
#define RT_SCREEN 6
#define RT_ADDITIVE 7
#define RT_FLAT_SCREEN 8

#define IT_NONE 0
#define IT_NV 1
#define IT_THERMAL 2
#define IT_THERMAL_COLOR 3

#define ST_NVG_BLUR 0
#define ST_PARALLAX_SHADOW 1
#define ST_SEE_THROUGH 2
#define ST_THERMAL_PIXELATION 3
#define ST_CHROMATISM 4
#define ST_NEXT 5

#define SETTING(settings, shift) (settings & (1 << shift))

#define SETTINGS int(s3ds_param_4.w)
#define IMAGE_TYPE int(s3ds_param_3.x)
#define LENS_COLOR s3ds_param_4.xyz

#define RETICLE_TYPE int(s3ds_param_3.y)

Texture2D s_inside;
Texture2D s_dirt;
Texture2D s_heat_map;
TextureCube s_reflection;

float zoomPercent() {
	return minMag() == maxMag()
		? 0.0
		: (curMag() - minMag()) / (maxMag() - minMag());
}

float4 sampleScopeTex(float2 tc) {
	return s_image.Sample(smp_base, SCOPECOORD_TO_TEXCOORD(tc));
}

float3x3 cotangent_frame(float3 N, float3 P, float2 uv)
{
    float3 dp1 = ddx(P);
    float3 dp2 = ddy(P);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);

    float3 dp2perp = cross(dp2, N);
    float3 dp1perp = cross(N, dp1);
    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    float invmax = rsqrt(max(dot(T, T), dot(B, B)));
    return float3x3(T * invmax, B * invmax, N);
}

float4 sample_shadow(float2 tc, float shadow_width)
{
	float a = smoothstep(0.60 - shadow_width, 0.60, distance(tc, float2(0.5, 0.5))); 
	return float4(0, 0, 0, a);
}

float4 sample_zoom_switch_shadow(float2 tc)
{
	float shift = zoomPercent();
	float a = smoothstep(1 - 0.1, 1, distance(tc, float2(0.5 + shift * 3, 0.5)));
	float b = smoothstep(1 - 0.4, 1, distance(tc, float2(-2.5 + shift * 3, 0.5)));
	return float4(0, 0, 0, a * b);
}

float2 project(float2 tc, float2 tangent, float distance, float size)
{
    float2 parallax_tc = tc - tangent * distance;
	parallax_tc.x = (parallax_tc.x + (size - 1) / 2) / size;
	parallax_tc.y = (parallax_tc.y + (size - 1) / 2) / size;
    return parallax_tc;
}

float3 chroma_sample(float2 lens_tc, float2 back_tc, float current_fov, float power)
{
    float3 color_sum = float3(0, 0, 0);
    float3 weight_sum = float3(0, 0, 0);
    
    for(int i = 0; i <= 16; ++i)
    {
        float step = i / 16.0;

        float2 delta = lens_tc - 0.5;
		float zoom_multiplier = min(1, 0.004 * (180 / current_fov));
        delta = sign(delta) * pow(abs(delta), 3.5) * (2 * power + zoom_multiplier);
        float2 coord = back_tc + (step - 0.5) * delta;
        
        float3 weight = float3(step, 1.0 - abs(step + step - 1.0), 1.0 - step);
        weight = lerp(float3(0.5, 0.5, 0.5), weight, 2.0);
        
        float3 color = sampleScopeTex(coord).rgb;
        color_sum += color * color * weight;
        
        weight_sum += weight;
    }

    return max(0.0001, sqrt(color_sum / weight_sum));
}

float3 back_image_sample(float2 lens_tc, float2 back_tc, float current_fov, float power, bool chroma)
{
	if (chroma) {
		return chroma_sample(lens_tc, back_tc, current_fov, power);
	}
	
	return sampleScopeTex(back_tc).rgb;
}

float3 apply_nvg(float2 tc, float3 img)
{
	img = BlackandWhite(img);
	img = Brightness(img, 0.45, 7);
	img = clamp(img, 0, 1);
	img = LevelsPass(img);
	img = Grain1(img, tc);
	img = Grain2(img, tc);
	return img;
}

float3 lcd_effect(int2 hpos)
{
	float pb = 0.4;
    float3 lcdColor = float3(pb, pb, pb);
	
    int px = fmod(hpos.x, 3);
	if (px == 1)
		lcdColor.r = 1;
    else if (px == 2)
		lcdColor.g = 1;
    else
		lcdColor.b = 1;
    
    float sclV = 0.25;
    if (int(fmod(hpos.y, 3)) == 0)
		lcdColor.rgb = float3(sclV, sclV, sclV);
	
	return lcdColor;
}

float2 fisheye_shift(float2 uv, float progress, float2 center)
{
	uv -= center;
    
	float ratio = 1;
	float pUvX = pow(uv.x * ratio, 2);
	float pUvY = pow(uv.y, 2);
	float pSum = pUvX + pUvY;
	float multiplier = 10 * (1 - progress);
	float strength = 1 - multiplier * pSum;
	uv *= strength;
    
	uv += center;
	return uv;
}

float2 fisheye(float2 tc, float2 tangent)
{
	float FISHEYE_STRENGTH = -0.3;
	float FISHEYE_PROJECT = 2;

	float fish_power = 1 + FISHEYE_STRENGTH * length(tangent);
	float2 fished = fisheye_shift(tc, fish_power, project(float2(0.5, 0.5), tangent, FISHEYE_PROJECT, 1.0));
	return fished - tc;
}

float current_lum()
{
	float lum_min = 0.85;
	float lum_max = 3;
	float lum = s_tonemap.Load(int3(0, 0, 0)).x;
	return clamp(1 - (lum - lum_min) / (lum_max - lum_min), 0, 1);
}

float4 rgba_blend(float4 b, float4 a)
{
	float na = a.a + b.a * (1 - a.a);
	float3 nc = na > 0 ? (a.rgb * a.a + b.rgb * b.a * (1 - a.a)) / na : float3(0, 0, 0);
	return float4(nc, na);
}

float sample_vignette(float2 tc)
{
    float dist = length(tc - 0.5);
    float vig = 1.0 - dist * 2;
    return pow(vig, dist * 1.0);
}

float4 sample_specular(float3 pnt, float3 normal, float3 light_direction)
{
	float w = pow(abs(dot(normalize(pnt + light_direction), normal)), 3072);
	return float4(L_sun_color.rgb * w, w);
}

float3 sample_lens_normalmap(float2 tc, float radius)
{
	float2 xy = (tc - 0.5) * 2;
	return float3(xy, sign(radius) * sqrt(pow(radius, 2) - dot(xy, xy)));
}

float3 sample_reflections(float2 tc, float parallax_factor, float radius, float dirt_factor, float3x3 TBNw_inv, float3 w_pos, float3 w_nrm, float lum, bool is_flat)
{
	float3 normalmap = sample_lens_normalmap(tc, radius);
	if (is_flat)
		normalmap = float3(0, 0, 1);
    float3 lensnormal = normalize(float3(dot(normalmap, TBNw_inv[0]), dot(normalmap, TBNw_inv[1]), dot(normalmap, TBNw_inv[2])));

	float3 reflections = s_reflection.SampleLevel(smp_base, reflect(normalize(normalize(w_pos - eye_position) - normalize(w_nrm) * parallax_factor), lensnormal), 0).rgb;
	if (!is_flat)
		reflections *= sample_vignette(tc);
	
	return reflections * normalize(0.3 + L_ambient.rgb + L_hemi_color.rgb);
}

float sample_reflections_weight(float dirt_factor, float3 w_pos, float3 w_nrm, float lum, bool see_through)
{
	float angle_factor = smoothstep(0, 0.03, (dot(normalize(w_pos - eye_position), normalize(w_nrm)) + 1) / 2);
	if (!see_through) {
		angle_factor = lerp(1, angle_factor, m_hud_params.x);
	}
	return
		angle_factor
		* smoothstep(0, 1, lum)
		* pow(1 - dirt_factor, 10)
		* REFLECTIONS_FACTOR;
}

float3 sample_specular(float2 tc, float parallax_factor, float radius, float specular_factor, float dirt_factor, float3x3 TBNw_inv, float3 w_pos, float3 w_nrm, bool see_through)
{
	float3 normalmap = sample_lens_normalmap(tc, radius);
	
	float angle_factor = smoothstep(0, 0.03, (dot(normalize(w_pos - eye_position), normalize(w_nrm)) + 1) / 2);
	if (!see_through) {
		angle_factor = lerp(1, angle_factor, m_hud_params.x);
	}

	float4 specular = sample_specular(
		normalize(normalize(w_pos - eye_position) - normalize(w_nrm) * parallax_factor),
		normalize(float3(dot(normalmap, TBNw_inv[0]), dot(normalmap, TBNw_inv[1]), dot(normalmap, TBNw_inv[2]))),
		normalize(L_sun_dir_w)
	) * 4;
	specular.w *= L_material.g * angle_factor;
	specular.w = min(1, specular.w);
	specular.w *= pow(1 - dirt_factor, 10);
	specular.w *= specular_factor;
	
	return specular.rgb * specular.a * normalize(0.3 + L_ambient.rgb + L_hemi_color.rgb);
}

float3 HSVtoRGB(float3 hsv)
{
	if (hsv.y == 0.0)
		return hsv.z;

	float i = floor(hsv.x * 6.0);
	float f = hsv.x * 6.0 - i;
	float p = hsv.z * (1.0 - hsv.y);
	float q = hsv.z * (1.0 - f * hsv.y);
	float t = hsv.z * (1.0 - (1.0 - f) * hsv.y);

	i = fmod(i, 6.0);

	if (i == 0.0)
		return float3(hsv.z, t, p);
	else if (i == 1.0)
		return float3(q, hsv.z, p);
	else if (i == 2.0)
		return float3(p, hsv.z, t);
	else if (i == 3.0)
		return float3(p, q, hsv.z);
	else if (i == 4.0)
		return float3(t, p, hsv.z);
	return float3(hsv.z, p, q);
}

float3 tonemap(float3 color, float3 v_pos)
{
    float fog = saturate(length(v_pos) * fog_params.w + fog_params.x);
    color = lerp(color, fog_color, fog);
	color = lerp(color, fog_color, saturate(fog * fog));

	color *= s_tonemap.Load(int3(0, 0, 0)).x;
	const float fWhiteIntensity = 1.7;
	const float fWhiteIntensitySQR = fWhiteIntensity * fWhiteIntensity;
    return (color * (1 + color / fWhiteIntensitySQR)) / (color + 1);
}

#endif