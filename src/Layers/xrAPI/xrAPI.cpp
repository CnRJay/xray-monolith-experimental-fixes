// xrAPI.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "../../Include/xrApi/xrAPI.h"

#include "../xrRender/dxRenderFactory.h"

extern dxRenderFactory RenderFactoryImpl;

XRAPI_API IRender_interface* Render = nullptr;
XRAPI_API dxRenderFactory* RenderFactory = &RenderFactoryImpl;
XRAPI_API CDUInterface* DU = nullptr;
XRAPI_API xr_token* vid_mode_token = nullptr;
XRAPI_API IUIRender* UIRender = nullptr;
#ifndef	_EDITOR
//XRAPI_API CSound_manager_interface*	Sound = nullptr;
XRAPI_API CGameMtlLibrary* PGMLib = nullptr;
#endif
XRAPI_API IDebugRender*	DRender = nullptr;

/*
// This is an example of an exported variable
XRAPI_API int nxrAPI=0;

// This is an example of an exported function.
XRAPI_API int fnxrAPI(void)
{
	return 42;
}

// This is the constructor of a class that has been exported.
// see xrAPI.h for the class definition
CxrAPI::CxrAPI()
{
	return;
}
*/
