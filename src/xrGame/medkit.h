///////////////////////////////////////////////////////////////
// Medkit.h
// Medkit - аптечка, повышающая здоровье
///////////////////////////////////////////////////////////////


#pragma once

#include "eatable_item_object.h"
#include "script_export_space.h"

class CMedkit : public CEatableItemObject
{
public:
	CMedkit();
	virtual ~CMedkit();
	
	DECLARE_SCRIPT_REGISTER_FUNCTION
};
