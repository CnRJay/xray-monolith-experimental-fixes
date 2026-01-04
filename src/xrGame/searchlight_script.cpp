#include "stdafx.h"

#ifdef PROJECTOR_NEW
#include "searchlight.h"
#include "pch_script.h"
#include "alife_space.h"

using namespace luabind;

#pragma optimize("s",on)
void CProjector::script_register(lua_State* L)
{
	module(L)
	[
		class_<CProjector, CGameObject>("CProjector")
		.def(constructor<>())
		.enum_("projector_enum")
		[
			value("eActive", int(CProjector::eActive)),
			value("eSwitch", int(CProjector::eSwitch)),
			value("eDesiredPos", int(CProjector::eDesiredPos)),
			value("eDesiredDir", int(CProjector::eDesiredDir)),
			value("eDesiredAng", int(CProjector::eDesiredAng))
		]
		.def("Action", &CProjector::Action)
		.def("SetParam", &CProjector::SetParam)
	];
}
#endif