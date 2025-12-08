/*
	=====================================================================
	Addon      : Shader 3D Scopes
	Link       : https://www.moddb.com/mods/stalker-anomaly/addons/shader-3d-scopes
	Authors    : LVutner, party_50

	All credit to original authors.
	=====================================================================
*/

#include "scope_3dss_common.h"

#define LCD_RES float2(640,640)

float3 scope_custom_image(Scope s) {
	float2 coord = s.sfp;
	if (RETICLE_TYPE == RT_SCREEN || RETICLE_TYPE == RT_FLAT_SCREEN) {
		coord = s.tc0;
	}

	float2 scope_tc = SCOPECOORD_TO_TEXCOORD(coord);
	float3 back = SampleBackbuffer(scope_tc);
	if ((IMAGE_TYPE == IT_THERMAL || IMAGE_TYPE == IT_THERMAL_COLOR) && markswitch_current.x < 2) {
		if (SETTING(SETTINGS, ST_THERMAL_PIXELATION)) {
			float pixelate = curMag();

			float2 sfp = (floor(coord * LCD_RES / pixelate) * pixelate + 0.5) / LCD_RES;
			scope_tc = SCOPECOORD_TO_TEXCOORD(sfp);
		}

		float2 rez = screen_res.xy;

		gbuffer_data gbd = gbuffer_load_data(scope_tc, scope_tc * rez.xy, 0);
		back = infrared(gbd, scope_tc * rez.xy, scope_tc);
		if (markswitch_current.x == 1) {
			back = 1 - back;
		}
		if (IMAGE_TYPE == IT_THERMAL_COLOR) {
		    back = s_heat_map.Sample(smp_base, float2(back.x, 0.5));
		}
	} else {
		// FIXME: Incorrect colorspace
		back *= LENS_COLOR;
	}

	if (false) {
		if (IMAGE_TYPE == IT_THERMAL || IMAGE_TYPE == IT_THERMAL_COLOR) {
			float SUBPIXELS = (LCD_RES * 4.0);
			float digitalZoom = SETTING(SETTINGS, ST_THERMAL_PIXELATION)
				? curMag()
				: 1.0;

			float multiplier = (SUBPIXELS / digitalZoom);

			float2 c = (coord - s.center) * multiplier + SUBPIXELS*10;

			// The LCD effect is designed for 1:1 pixel ratio, and LCD effect in general requires high output resolution to look correct.
			// It is absolutely necessary to multisample to get even a barely passable output.
			float2 o = float2(0.25,-0.25);
			float3 s = lcd_effect(c)
					+ lcd_effect(c + o.xx)
					+ lcd_effect(c + o.yy)
					+ lcd_effect(c + o.xy)
					+ lcd_effect(c + o.yx);

			back *= s / 5.0;
		}
	}

	if (IMAGE_TYPE == IT_NV && markswitch_current.x == 0) {
		// FIXME: Incorrect colorspace
		back = apply_nvg(scope_tc, back);
	}

	return back;
}