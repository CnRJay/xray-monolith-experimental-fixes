#ifndef xrSkinningH
#define xrSkinningH
#pragma once

#include "bone.h"

// Forward references
struct vertRender;
struct vertBoned1W;
struct vertBoned2W;
struct vertBoned3W;
struct vertBoned4W;
class CBoneInstance;

// Skinning functions
void __stdcall xrSkin1W(vertRender* D, vertBoned1W* S, u32 vCount, CBoneInstance* Bones);
void __stdcall xrSkin2W(vertRender* D, vertBoned2W* S, u32 vCount, CBoneInstance* Bones);
void __stdcall xrSkin3W(vertRender* D, vertBoned3W* S, u32 vCount, CBoneInstance* Bones);
void __stdcall xrSkin4W(vertRender* D, vertBoned4W* S, u32 vCount, CBoneInstance* Bones);

#endif
